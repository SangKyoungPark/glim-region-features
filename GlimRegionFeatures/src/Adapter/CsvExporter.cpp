// CsvExporter.cpp
#include "Adapter/CsvExporter.h"
#include "Adapter/RegionExtractor.h"
#include "UseCase/FeatureCalculator.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <filesystem>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

namespace fs = std::filesystem;

namespace Grf {

namespace {
	std::string ToLower(const std::string& s)
	{
		std::string out(s);
		std::transform(out.begin(), out.end(), out.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return out;
	}
}

bool CsvExporter::IsSupportedImage(const std::string& ext)
{
	std::string e = ToLower(ext);
	return (e == ".bmp" || e == ".png" || e == ".jpg" ||
		e == ".jpeg" || e == ".tif" || e == ".tiff");
}

std::string CsvExporter::CsvQuote(const std::string& s)
{
	std::string out = "\"";
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] == '"')
			out += "\"\"";
		else
			out += s[i];
	}
	out += "\"";
	return out;
}

int CsvExporter::FeatureColumnCount()
{
	std::string h = FeatureVector::CsvHeader();
	int cols = 1;
	for (size_t i = 0; i < h.size(); ++i)
		if (h[i] == ',') ++cols;
	return cols;
}

std::vector<std::string> CsvExporter::ScoreFeatureNames(const ProfileLoader* profile)
{
	std::vector<std::string> names;
	if (profile == NULL || !profile->IsLoaded())
		return names;
	const std::vector<ScoreConfig>& cfgs = profile->Normalizer().Configs();
	for (size_t i = 0; i < cfgs.size(); ++i)
		names.push_back(cfgs[i].m_featureName);
	return names;
}

std::string CsvExporter::BuildHeader(const ProfileLoader* profile)
{
	// FileName 뒤에 FilePath 삽입(기존 사용자 인지 순서 유지). RegionIndex 뒤에 Channel(B/W) 추가.
	std::string header = "FileName,FilePath,RegionIndex,Channel," + FeatureVector::CsvHeader();
	if (profile != NULL && profile->IsLoaded())
	{
		std::vector<std::string> names = ScoreFeatureNames(profile);
		for (size_t i = 0; i < names.size(); ++i)
			header += ",score_" + names[i];
		header += ",ClassifiedCode";
	}
	// CSV 끝: mm 파생 컬럼(픽셀 원시 컬럼은 전부 유지, 병기). 스케일 1.0이면 픽셀값과 동일.
	header += ",area_mm2,width_mm,height_mm,diameter_mm";
	return header;
}

std::string CsvExporter::BuildRegionRow(const std::string& fileName, const std::string& filePath,
	int regionIndex, const FeatureVector& fv, const ProfileLoader* profile,
	const std::string& channel, double scaleX, double scaleY)
{
	std::ostringstream oss;
	oss << CsvQuote(fileName) << "," << CsvQuote(filePath) << ","
		<< regionIndex << "," << channel << "," << fv.ToCsvRow();

	if (profile != NULL && profile->IsLoaded())
	{
		std::vector<std::string> names = ScoreFeatureNames(profile);
		ScoreResult sr = profile->Normalizer().Normalize(fv);
		for (size_t i = 0; i < names.size(); ++i)
			oss << "," << sr.Get(names[i]);
		std::string code = profile->RuleEngine().Classify(fv, "OK");
		oss << "," << CsvQuote(code);
	}

	// mm 파생값(bbox 는 inclusive 좌표라 폭/높이는 +1). diameter 는 등방 스케일 근사(sqrt(sx*sy)).
	const double widthPx = fv.bboxCol2 - fv.bboxCol1 + 1.0;
	const double heightPx = fv.bboxRow2 - fv.bboxRow1 + 1.0;
	const double areaMm2 = fv.area * scaleX * scaleY;
	const double widthMm = widthPx * scaleX;
	const double heightMm = heightPx * scaleY;
	const double diameterMm = fv.diameter * std::sqrt(scaleX * scaleY);
	oss << "," << areaMm2 << "," << widthMm << "," << heightMm << "," << diameterMm;
	return oss.str();
}

std::string CsvExporter::BuildEmptyRow(const std::string& fileName, const std::string& filePath,
	const ProfileLoader* profile)
{
	const int featCols = FeatureColumnCount();
	const std::string emptyFeat(featCols > 0 ? (featCols - 1) : 0, ',');

	std::ostringstream oss;
	// RegionIndex=-1, Channel 공란, 특징값 전부 공란
	oss << CsvQuote(fileName) << "," << CsvQuote(filePath) << ",-1,," << emptyFeat;

	if (profile != NULL && profile->IsLoaded())
	{
		std::vector<std::string> names = ScoreFeatureNames(profile);
		for (size_t i = 0; i < names.size(); ++i)
			oss << ",";
		oss << ",NO_REGION";
	}
	// mm 파생 컬럼 4개 공란
	oss << ",,,,";
	return oss.str();
}

bool CsvExporter::ExportFolder(const std::string& inputDir, const std::string& outputCsv,
	const ProfileLoader* profile, BatchStat& statOut,
	int numThreads, const IPreprocessor* preprocessor,
	const std::string& overlayDir, const ExportOptions& options)
{
	statOut = BatchStat();

	// 오버레이 출력 폴더 준비(비어있지 않으면)
	const bool makeOverlay = !overlayDir.empty();
	if (makeOverlay)
	{
		try { fs::create_directories(overlayDir); }
		catch (...) { /* 생성 실패해도 CSV 는 진행 */ }
	}

	// 이진화 덤프 폴더 준비(비어있지 않으면)
	const bool makeDumpBin = !options.m_dumpBinDir.empty();
	if (makeDumpBin)
	{
		try { fs::create_directories(options.m_dumpBinDir); }
		catch (...) { /* 생성 실패해도 CSV 는 진행 */ }
	}
	const double sx = options.m_scaleX;
	const double sy = options.m_scaleY;
	const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();

	// 전처리기: null 이면 기본 CPU 구현(GPU 구현으로 교체 가능). const 공유(무상태).
	CpuPreprocessor defaultPre;
	const IPreprocessor& pre = (preprocessor != NULL) ? *preprocessor : defaultPre;

	// --- 파일 수집(정렬로 결정적 순서) ---
	std::vector<std::string> files;
	try
	{
		if (!fs::exists(inputDir) || !fs::is_directory(inputDir))
			return false;
		for (fs::directory_iterator it(inputDir); it != fs::directory_iterator(); ++it)
		{
			if (!it->is_regular_file())
				continue;
			const std::string ext = it->path().extension().string();
			if (IsSupportedImage(ext))
				files.push_back(it->path().string());
		}
	}
	catch (const std::exception&)
	{
		return false;
	}
	std::sort(files.begin(), files.end());
	statOut.m_totalFiles = files.size();

	// --- CSV 열기(헤더만 먼저 기록, 본문은 병렬 처리 후 인덱스 순서로 기록) ---
	std::ofstream ofs(outputCsv.c_str(), std::ios::binary);
	if (!ofs.is_open())
		return false;
	ofs << BuildHeader(profile) << "\r\n";

	// 파일별 결과 블록(각 파일의 CSV 행들을 순서대로 담음). 인덱스별 소유 → 락 불필요.
	std::vector<std::string> blocks(files.size());

	// 공유 카운터/보호
	std::atomic<size_t> nextIdx(0);
	std::atomic<size_t> processed(0);
	std::atomic<long long> totalRegions(0);
	std::mutex failMutex;

	// 쓰레드 수 결정: 0=자동, 파일 수로 상한.
	unsigned hw = std::thread::hardware_concurrency();
	size_t nThreads = (numThreads > 0) ? static_cast<size_t>(numThreads) : (hw > 0 ? hw : 1);
	if (nThreads < 1) nThreads = 1;
	if (!files.empty() && nThreads > files.size()) nThreads = files.size();
	if (files.empty()) nThreads = 1;

	// 워커: 작업큐를 atomic 인덱스로 분배(작업 훔치기). 각 워커는 자신의 계산기 보유.
	auto worker = [&]()
	{
		RegionExtractor extractor;
		FeatureCalculator calc;
		for (;;)
		{
			const size_t i = nextIdx.fetch_add(1);
			if (i >= files.size())
				break;

			const std::string& path = files[i];
			std::string fileName = path;
			std::string filePath = path; // 절대경로(스캔 시 조합된 풀패스, 실패해도 원본 유지)
			try { fileName = fs::path(path).filename().string(); }
			catch (...) {}
			try { filePath = fs::absolute(fs::path(path)).string(); }
			catch (...) {}

			try
			{
				cv::Mat img = cv::imread(path, cv::IMREAD_GRAYSCALE);
				if (img.empty())
				{
					std::lock_guard<std::mutex> lk(failMutex);
					statOut.m_failedFiles.push_back(fileName + " (load failed)");
					continue;
				}

				// 채널 단위 이진화(PROJECTION=흑/백 2채널, 그 외=극성 태그 1채널). GPU 전처리 삽입 지점.
				std::vector<BinChannel> channels = pre.BinarizeMulti(img);
				if (channels.empty())
				{
					std::lock_guard<std::mutex> lk(failMutex);
					statOut.m_failedFiles.push_back(fileName + " (binarize failed)");
					continue;
				}
				const bool multiCh = (channels.size() > 1);

				// 오버레이 준비(원본 그레이→BGR). 채널별 색: B=빨강, W=초록.
				cv::Mat ov;
				if (makeOverlay)
				{
					try { cv::cvtColor(img, ov, cv::COLOR_GRAY2BGR); }
					catch (...) { ov = cv::Mat(); }
				}

				std::string block;
				int regionCounter = 0;      // 이미지 내 채널 통합 연속 인덱스
				long long fileRegions = 0;

				for (size_t c = 0; c < channels.size(); ++c)
				{
					const BinChannel& ch = channels[c];
					if (ch.image.empty())
						continue;
					std::vector<Region> regions = extractor.Extract(ch.image, 1);

					// 이진화 덤프 PNG: 다채널이면 <이름>_bin_<태그>.png, 단채널이면 <이름>_bin.png
					if (makeDumpBin)
					{
						try
						{
							std::string binName = multiCh
								? (fileName + "_bin_" + std::string(1, ch.tag) + ".png")
								: (fileName + "_bin.png");
							const std::string binPath =
								(fs::path(options.m_dumpBinDir) / binName).string();
							cv::imwrite(binPath, ch.image);
						}
						catch (...) { /* 덤프 실패는 CSV 에 영향 없음 */ }
					}

					// 오버레이 컨투어(채널색)
					if (makeOverlay && !ov.empty())
					{
						try
						{
							const cv::Scalar col = (ch.tag == 'B')
								? cv::Scalar(0, 0, 255)   // 빨강(흑 불량)
								: cv::Scalar(0, 255, 0);  // 초록(백 불량)
							for (size_t r = 0; r < regions.size(); ++r)
								cv::drawContours(ov, regions[r].AllContours(), -1, col, 1);
						}
						catch (...) {}
					}

					const std::string chTag(1, ch.tag);
					for (size_t r = 0; r < regions.size(); ++r)
					{
						FeatureVector fv = calc.Compute(regions[r]);
						block += BuildRegionRow(fileName, filePath, regionCounter++, fv,
							profile, chTag, sx, sy);
						block += "\r\n";
					}
					fileRegions += static_cast<long long>(regions.size());
				}

				// 오버레이 저장(채널 컨투어 합성). 파일별 독립 → 병렬 안전.
				if (makeOverlay && !ov.empty())
				{
					try
					{
						const std::string ovPath =
							(fs::path(overlayDir) / (fileName + "_ov.png")).string();
						cv::imwrite(ovPath, ov);
					}
					catch (...) { /* 오버레이 실패는 CSV 에 영향 없음 */ }
				}

				if (fileRegions == 0)
				{
					block = BuildEmptyRow(fileName, filePath, profile);
					block += "\r\n";
				}
				else
				{
					totalRegions.fetch_add(fileRegions);
				}
				blocks[i] = block;          // 인덱스 소유 → 경합 없음
				processed.fetch_add(1);
			}
			catch (const cv::Exception& e)
			{
				std::lock_guard<std::mutex> lk(failMutex);
				statOut.m_failedFiles.push_back(fileName + " (cv: " + e.what() + ")");
			}
			catch (const std::exception& e)
			{
				std::lock_guard<std::mutex> lk(failMutex);
				statOut.m_failedFiles.push_back(fileName + " (std: " + e.what() + ")");
			}
			catch (...)
			{
				std::lock_guard<std::mutex> lk(failMutex);
				statOut.m_failedFiles.push_back(fileName + " (unknown)");
			}
		}
	};

	// nThreads-1 개 스폰 + 현재 쓰레드도 1개 참여.
	std::vector<std::thread> pool;
	pool.reserve(nThreads > 0 ? nThreads - 1 : 0);
	for (size_t t = 1; t < nThreads; ++t)
		pool.push_back(std::thread(worker));
	worker();
	for (size_t t = 0; t < pool.size(); ++t)
		pool[t].join();

	// 결과를 파일 인덱스 순서대로 기록(병렬이어도 순서/내용 결정적).
	for (size_t i = 0; i < blocks.size(); ++i)
		ofs << blocks[i];

	ofs.flush();
	ofs.close();

	statOut.m_processedFiles = processed.load();
	statOut.m_totalRegions = totalRegions.load();

	const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
	statOut.m_elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
	return true;
}

} // namespace Grf
