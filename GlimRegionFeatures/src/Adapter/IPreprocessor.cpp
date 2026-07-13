// IPreprocessor.cpp
#include "Adapter/IPreprocessor.h"
#include <opencv2/imgproc.hpp>

namespace Grf {

namespace {
	// 그레이 8UC1 표준화(내부 공용)
	bool ToGray8U(const cv::Mat& src, cv::Mat& gray)
	{
		if (src.empty())
			return false;
		if (src.channels() != 1)
			cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
		else
			gray = src;
		if (gray.type() != CV_8UC1)
		{
			cv::Mat tmp;
			gray.convertTo(tmp, CV_8UC1);
			gray = tmp;
		}
		return true;
	}

	// PROJECTION 흑/백 이진화(검사기 Do_Search_Candidate 근사).
	//  proj[x] = 열평균(전체 행). 검사기는 uchar 정수 강하(nSum/nCnt)이나 여기선 float 계산.
	//  국소평균 = boxFilter(kxk) (검사기 4x4 커널 평균 근사).
	//  흑: proj[x] - 국소평균 > blackTh,  백: 국소평균 - proj[x] > whiteTh (strict >, 검사기 동일).
	void ProjectionBinarize(const cv::Mat& gray, double blackTh, double whiteTh, int k,
		cv::Mat& outBlack, cv::Mat& outWhite)
	{
		if (k < 1) k = 1;

		// 열평균 프로파일(1 x cols, float) → 전체 행으로 브로드캐스트
		cv::Mat colMean;
		cv::reduce(gray, colMean, 0 /*행 축소 → 열별 평균*/, cv::REDUCE_AVG, CV_32F);
		cv::Mat projFull;
		cv::repeat(colMean, gray.rows, 1, projFull); // rows x cols, CV_32F

		// 국소평균(4x4 근사)
		cv::Mat smooth;
		cv::boxFilter(gray, smooth, CV_32F, cv::Size(k, k));

		// 흑: proj - smooth > blackTh  (어두운 불량)
		cv::Mat diffB = projFull - smooth;
		outBlack = (diffB > blackTh);        // CV_8U 0/255
		// 백: smooth - proj > whiteTh  (밝은 불량)
		cv::Mat diffW = smooth - projFull;
		outWhite = (diffW > whiteTh);
	}
} // namespace

cv::Mat CpuPreprocessor::Binarize(const cv::Mat& src) const
{
	// 무상태: 모든 작업은 지역 Mat 으로만 수행(재진입 안전).
	if (src.empty())
		return cv::Mat();

	try
	{
		cv::Mat gray;
		if (src.channels() != 1)
			cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
		else
			gray = src;

		if (gray.type() != CV_8UC1)
		{
			cv::Mat tmp;
			gray.convertTo(tmp, CV_8UC1);
			gray = tmp;
		}

		// 극성: DARK 는 어두운 쪽을 Region 으로(THRESH_BINARY_INV)
		const int baseType = (m_params.m_polarity == POLARITY_DARK)
			? cv::THRESH_BINARY_INV : cv::THRESH_BINARY;

		cv::Mat bin;
		switch (m_params.m_mode)
		{
		case BINMODE_OTSU:
			// Otsu: 임계값 자동 산출(반환값 무시). thresh 인자는 무시됨.
			cv::threshold(gray, bin, 0.0, 255.0, baseType | cv::THRESH_OTSU);
			break;

		case BINMODE_MEAN_OFFSET:
		{
			// 배경 평균 기반 자동 임계값(밝기 변동 대응)
			const double mean = cv::mean(gray)[0];
			double thr = (m_params.m_polarity == POLARITY_DARK)
				? (mean - m_params.m_offset)   // 어두운 불량: mean-offset 미만이 Region
				: (mean + m_params.m_offset);  // 밝은 불량:   mean+offset 초과가 Region
			if (thr < 0.0)   thr = 0.0;
			if (thr > 255.0) thr = 255.0;
			cv::threshold(gray, bin, thr, 255.0, baseType);
			break;
		}

		case BINMODE_BINARY:
			// 이미 이진화된 이미지: 0 초과를 Region 으로(극성 반영). 실제 배치 로직과 동일.
			cv::threshold(gray, bin, 0.0, 255.0, baseType);
			break;

		case BINMODE_WRINKLE:
		{
			// 주름(밝은 무지부 위의 세로 방향 미세 어두운 선) 검출 전처리.
			//  실측 검증 파이프라인(NullWrinkle 60/60, 포화 0). 극성(polarity)은 무시(항상 어두운 선).
			//  코팅부(어두운 세로 띠)가 통째로 잡히는 문제를 "밝은영역 마스크 ∩ 세로선 특화 응답"으로 회피.

			// 1) 밝은영역 마스크: Otsu(밝기 변동 210~236 → 고정 TH 금지) → 최대 연결성분 → erode(ELLIPSE 21x21)
			//    (무지부-코팅부 경계 전이폭 + 후속 커널 반경을 커버)
			cv::Mat brightMask;
			cv::threshold(gray, brightMask, 0.0, 255.0,
				cv::THRESH_BINARY | cv::THRESH_OTSU);

			cv::Mat labels, stats, centroids;
			const int nLabels = cv::connectedComponentsWithStats(
				brightMask, labels, stats, centroids, 8, CV_32S);
			if (nLabels > 1)
			{
				int best = -1, bestArea = -1;
				for (int i = 1; i < nLabels; ++i) // 0 = 배경 제외
				{
					const int area = stats.at<int>(i, cv::CC_STAT_AREA);
					if (area > bestArea) { bestArea = area; best = i; }
				}
				if (best >= 0)
					brightMask = (labels == best); // 최대 성분만 255
			}
			const cv::Mat erKernel =
				cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(21, 21));
			cv::erode(brightMask, brightMask, erKernel);

			// 2) 어두운 세로선 강조: BLACKHAT + 수평 RECT 커널(가로 kernelW x 세로 1).
			//    코팅 경계의 세로 에지에는 무반응, 세로 주름선만 강조.
			int kw = m_params.m_kernelSize;
			if (kw < 1) kw = 1;
			const cv::Mat bhKernel =
				cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kw, 1));
			cv::Mat blackhat;
			cv::morphologyEx(gray, blackhat, cv::MORPH_BLACKHAT, bhKernel);

			// 3) 세로 누적 blur(가로 1 x 세로 blurH): 픽셀 SNR ~1.5σ 라 이 누적 없이는 분리 불가(핵심)
			int bh = m_params.m_blurH;
			if (bh < 1) bh = 1;
			cv::Mat accum;
			cv::blur(blackhat, accum, cv::Size(1, bh));

			// 4) 이진화: 누적 응답 > responseThresh AND 밝은영역 마스크
			cv::Mat resp;
			cv::threshold(accum, resp, m_params.m_responseThresh, 255.0,
				cv::THRESH_BINARY);
			cv::bitwise_and(resp, brightMask, bin);
			break;
		}

		case BINMODE_PROJECTION:
		{
			// 단일 Binarize() 호출 시에는 극성에 맞는 채널 1장을 반환(흑/백 모두 필요하면 BinarizeMulti).
			cv::Mat black, white;
			ProjectionBinarize(gray, m_params.m_projBlackTh, m_params.m_projWhiteTh,
				m_params.m_projKernel, black, white);
			bin = (m_params.m_polarity == POLARITY_DARK) ? black : white;
			break;
		}

		case BINMODE_FIXED:
		default:
			cv::threshold(gray, bin, m_params.m_threshold, 255.0, baseType);
			break;
		}
		return bin;
	}
	catch (const cv::Exception&)
	{
		return cv::Mat();
	}
}

std::vector<BinChannel> CpuPreprocessor::BinarizeMulti(const cv::Mat& src) const
{
	std::vector<BinChannel> out;
	if (src.empty())
		return out;

	try
	{
		cv::Mat gray;
		if (!ToGray8U(src, gray))
			return out;

		if (m_params.m_mode == BINMODE_PROJECTION)
		{
			// 흑/백 2채널(검사기 흑/백 이미지 대응). 순서: B 먼저, W 다음.
			cv::Mat black, white;
			ProjectionBinarize(gray, m_params.m_projBlackTh, m_params.m_projWhiteTh,
				m_params.m_projKernel, black, white);
			if (!black.empty()) out.push_back(BinChannel(black, 'B'));
			if (!white.empty()) out.push_back(BinChannel(white, 'W'));
			return out;
		}

		// 그 외 모드: 단일 채널. 태그는 극성 기준(dark→B, bright→W).
		cv::Mat b = Binarize(src);
		if (!b.empty())
		{
			const char tag = (m_params.m_polarity == POLARITY_DARK) ? 'B' : 'W';
			out.push_back(BinChannel(b, tag));
		}
		return out;
	}
	catch (const cv::Exception&)
	{
		return out;
	}
}

} // namespace Grf
