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

		cv::Mat bin;
		cv::threshold(gray, bin, m_threshold, 255.0, cv::THRESH_BINARY);
		return bin;
	}
	catch (const cv::Exception&)
	{
		return cv::Mat();
	}
}

} // namespace Grf
