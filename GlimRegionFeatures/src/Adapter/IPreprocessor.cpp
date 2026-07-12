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
