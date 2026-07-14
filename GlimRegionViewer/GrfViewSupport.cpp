// GrfViewSupport.cpp : 뷰어 대시보드 공용 지원 구현
#include "stdafx.h"
#include "GrfViewSupport.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <thread>
#include <atomic>
#include <algorithm>

namespace GrfView {

// ────────────────────────────────────────────────────────────────
// 분류코드 → 고정색
// ────────────────────────────────────────────────────────────────
COLORREF DefectCodeColor(const std::string& code)
{
	// 자주 쓰는 코드에 대한 고정 팔레트(가독성 우선).
	struct Fixed { const char* code; COLORREF color; };
	static const Fixed kTable[] = {
		{ "OK",       RGB(120, 190, 120) },
		{ "PINHOLE",  RGB(220,  70,  70) },
		{ "BUBBLE",   RGB(240, 150,  40) },
		{ "TEAR",     RGB(150,  90, 200) },
		{ "ISLAND",   RGB(200,  80, 160) },
		{ "UNCOATED", RGB( 70, 130, 220) },
		{ "WRINKLE",  RGB( 40, 180, 180) },
		{ "SCRATCH",  RGB(200, 180,  50) },
	};
	const int n = sizeof(kTable) / sizeof(kTable[0]);
	for (int i = 0; i < n; ++i)
	{
		if (code == kTable[i].code)
			return kTable[i].color;
	}

	if (code.empty())
		return RGB(150, 150, 150);

	// 미등록 코드: 문자열 해시 → HSV 계열 안정적 색(너무 어둡지 않게 클램프).
	unsigned int h = 2166136261u; // FNV-1a
	for (size_t i = 0; i < code.size(); ++i)
	{
		h ^= static_cast<unsigned char>(code[i]);
		h *= 16777619u;
	}
	int r = 90 + static_cast<int>((h & 0xFF) % 140);
	int g = 90 + static_cast<int>(((h >> 8) & 0xFF) % 140);
	int b = 90 + static_cast<int>(((h >> 16) & 0xFF) % 140);
	return RGB(r, g, b);
}

// ────────────────────────────────────────────────────────────────
// ThumbCache
// ────────────────────────────────────────────────────────────────
ThumbCache::ThumbCache(size_t capacity)
	: m_capacity(capacity ? capacity : 1)
{
	::InitializeCriticalSection(&m_cs);
}

ThumbCache::~ThumbCache()
{
	::DeleteCriticalSection(&m_cs);
}

void ThumbCache::SetCapacity(size_t capacity)
{
	::EnterCriticalSection(&m_cs);
	m_capacity = capacity ? capacity : 1;
	while (m_lru.size() > m_capacity)
	{
		m_index.erase(m_lru.back().path);
		m_lru.pop_back();
	}
	::LeaveCriticalSection(&m_cs);
}

void ThumbCache::Clear()
{
	::EnterCriticalSection(&m_cs);
	m_lru.clear();
	m_index.clear();
	::LeaveCriticalSection(&m_cs);
}

cv::Mat ThumbCache::GetGray(const std::string& path)
{
	::EnterCriticalSection(&m_cs);
	std::map<std::string, std::list<Entry>::iterator>::iterator it = m_index.find(path);
	if (it != m_index.end())
	{
		// 최근 사용으로 승격
		m_lru.splice(m_lru.begin(), m_lru, it->second);
		cv::Mat g = it->second->gray; // 얕은복사(읽기전용 공유)
		::LeaveCriticalSection(&m_cs);
		return g;
	}
	::LeaveCriticalSection(&m_cs);

	// 캐시 미스: CS 밖에서 디코드(디스크 I/O 로 락 점유 최소화)
	cv::Mat gray;
	try
	{
		gray = cv::imread(path, cv::IMREAD_GRAYSCALE);
	}
	catch (...)
	{
		gray = cv::Mat();
	}
	if (gray.empty())
		return cv::Mat();

	::EnterCriticalSection(&m_cs);
	// 경합으로 이미 채워졌으면 그것을 사용
	it = m_index.find(path);
	if (it != m_index.end())
	{
		m_lru.splice(m_lru.begin(), m_lru, it->second);
		cv::Mat g = it->second->gray;
		::LeaveCriticalSection(&m_cs);
		return g;
	}
	Entry e;
	e.path = path;
	e.gray = gray;
	m_lru.push_front(e);
	m_index[path] = m_lru.begin();
	while (m_lru.size() > m_capacity)
	{
		m_index.erase(m_lru.back().path);
		m_lru.pop_back();
	}
	::LeaveCriticalSection(&m_cs);
	return gray;
}

// ────────────────────────────────────────────────────────────────
// AnalyzeFiles (병렬)
// ────────────────────────────────────────────────────────────────
namespace {

	std::string BaseName(const std::string& path)
	{
		size_t sl = path.find_last_of("\\/");
		return (sl != std::string::npos) ? path.substr(sl + 1) : path;
	}

	// 파일 하나 처리 → RegionResult 목록(파일 내). 예외는 삼켜 빈 목록 반환.
	void ProcessOne(const std::string& path, int fileIndex,
		const Grf::IPreprocessor& pre, const Grf::ProfileLoader* profile,
		int minArea, std::vector<RegionResult>& outFile)
	{
		try
		{
			cv::Mat gray = cv::imread(path, cv::IMREAD_GRAYSCALE);
			if (gray.empty())
				return;

			std::vector<Grf::BinChannel> channels = pre.BinarizeMulti(gray);
			Grf::RegionExtractor extractor;
			Grf::FeatureCalculator calc;

			int regionIdx = 0;
			for (size_t c = 0; c < channels.size(); ++c)
			{
				if (channels[c].image.empty())
					continue;
				std::vector<Grf::Region> regions = extractor.Extract(channels[c].image, minArea);
				for (size_t r = 0; r < regions.size(); ++r)
				{
					RegionResult rr;
					rr.fileIndex = fileIndex;
					rr.regionIndex = regionIdx++;
					rr.filePath = path;
					rr.fileName = BaseName(path);
					rr.channel = channels[c].tag;
					rr.fv = calc.Compute(regions[r]);
					rr.bbox = regions[r].BoundingBox();
					rr.contours = regions[r].AllContours();

					if (profile && profile->IsLoaded())
					{
						try
						{
							Grf::ScoreResult sc = profile->Normalizer().Normalize(rr.fv);
							rr.overallScore = sc.Overall();
							rr.scores = sc.m_scores; // 묶음막대(코드×특징 평균)용
							rr.code = profile->RuleEngine().Classify(rr.fv, "OK");
						}
						catch (...)
						{
							rr.overallScore = -1.0;
							rr.code.clear();
						}
					}
					outFile.push_back(rr);
				}
			}
		}
		catch (...)
		{
			// 개별 파일 오류는 무시(나머지 파일 계속 처리)
		}
	}

} // namespace

void AnalyzeFiles(const std::vector<std::string>& files,
	const Grf::IPreprocessor& pre,
	const Grf::ProfileLoader* profile,
	int numThreads,
	int minArea,
	std::vector<RegionResult>& out,
	volatile long* progress)
{
	out.clear();
	const size_t n = files.size();
	if (n == 0)
		return;

	std::vector<std::vector<RegionResult> > perFile(n);

	int workers = numThreads;
	if (workers <= 0)
		workers = static_cast<int>(std::thread::hardware_concurrency());
	if (workers <= 0)
		workers = 1;
	if (workers > static_cast<int>(n))
		workers = static_cast<int>(n);

	std::atomic<size_t> next(0);
	auto worker = [&]()
	{
		for (;;)
		{
			size_t i = next.fetch_add(1);
			if (i >= n)
				break;
			ProcessOne(files[i], static_cast<int>(i), pre, profile, minArea, perFile[i]);
			if (progress)
				::InterlockedIncrement(progress);
		}
	};

	if (workers <= 1)
	{
		worker();
	}
	else
	{
		std::vector<std::thread> pool;
		pool.reserve(workers);
		for (int t = 0; t < workers; ++t)
			pool.push_back(std::thread(worker));
		for (size_t t = 0; t < pool.size(); ++t)
			pool[t].join();
	}

	// 파일 순서대로 평탄화
	size_t total = 0;
	for (size_t i = 0; i < n; ++i)
		total += perFile[i].size();
	out.reserve(total);
	for (size_t i = 0; i < n; ++i)
		for (size_t j = 0; j < perFile[i].size(); ++j)
			out.push_back(perFile[i][j]);
}

// ────────────────────────────────────────────────────────────────
// DrawMatFit
// ────────────────────────────────────────────────────────────────
CRect DrawMatFit(CDC* pDC, const cv::Mat& img, const CRect& dest, bool nearest)
{
	if (pDC == NULL || img.empty() || dest.Width() <= 0 || dest.Height() <= 0)
		return CRect(0, 0, 0, 0);

	// 그레이 → BGR 로 승격(표시용)
	cv::Mat bgr;
	try
	{
		if (img.type() == CV_8UC1)
			cv::cvtColor(img, bgr, cv::COLOR_GRAY2BGR);
		else if (img.type() == CV_8UC3)
			bgr = img;
		else
			return CRect(0, 0, 0, 0);
	}
	catch (...)
	{
		return CRect(0, 0, 0, 0);
	}

	const int w = bgr.cols;
	const int h = bgr.rows;
	if (w <= 0 || h <= 0)
		return CRect(0, 0, 0, 0);

	// 종횡비 유지 fit
	const double sx = static_cast<double>(dest.Width()) / w;
	const double sy = static_cast<double>(dest.Height()) / h;
	double scale = (sx < sy) ? sx : sy;
	int dw = static_cast<int>(w * scale);
	int dh = static_cast<int>(h * scale);
	if (dw < 1) dw = 1;
	if (dh < 1) dh = 1;
	const int dx = dest.left + (dest.Width() - dw) / 2;
	const int dy = dest.top + (dest.Height() - dh) / 2;

	// top-down DIB 로 변환
	const int stride = ((w * 3 + 3) / 4) * 4;
	std::vector<BYTE> buf(static_cast<size_t>(stride) * h);
	for (int y = 0; y < h; ++y)
	{
		const unsigned char* src = bgr.ptr<unsigned char>(y);
		memcpy(&buf[static_cast<size_t>(y) * stride], src, static_cast<size_t>(w) * 3);
	}

	BITMAPINFO bmi;
	::ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = -h; // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;

	const int oldMode = pDC->SetStretchBltMode(nearest ? COLORONCOLOR : HALFTONE);
	::StretchDIBits(pDC->GetSafeHdc(),
		dx, dy, dw, dh,
		0, 0, w, h,
		&buf[0], &bmi, DIB_RGB_COLORS, SRCCOPY);
	pDC->SetStretchBltMode(oldMode);

	return CRect(dx, dy, dx + dw, dy + dh);
}

} // namespace GrfView
