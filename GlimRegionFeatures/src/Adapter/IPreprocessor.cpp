// IPreprocessor.cpp
#include "Adapter/IPreprocessor.h"
#include <opencv2/imgproc.hpp>

namespace Grf {

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

} // namespace Grf
