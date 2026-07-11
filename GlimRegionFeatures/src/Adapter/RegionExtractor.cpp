// RegionExtractor.cpp
#include "Adapter/RegionExtractor.h"
#include <opencv2/imgproc.hpp>

namespace Grf {

RegionExtractor::RegionExtractor()
	: m_connectivity(8)
{
}

std::vector<Region> RegionExtractor::Extract(const cv::Mat& binary, int minArea) const
{
	std::vector<Region> regions;

	try
	{
		if (binary.empty())
			return regions;

		// --- 입력 정규화: 8UC1 이진(0/255) 으로 맞춘다 ---
		cv::Mat bin;
		if (binary.type() != CV_8UC1)
		{
			cv::Mat gray;
			if (binary.channels() != 1)
				cv::cvtColor(binary, gray, cv::COLOR_BGR2GRAY);
			else
				gray = binary;
			gray.convertTo(bin, CV_8UC1);
		}
		else
		{
			bin = binary;
		}
		// 0/255 보장
		cv::Mat mask;
		cv::compare(bin, 0, mask, cv::CMP_GT); // >0 → 255

		// --- 8-연결 라벨링 ---
		cv::Mat labels, stats, centroids;
		int numLabels = cv::connectedComponentsWithStats(
			mask, labels, stats, centroids, m_connectivity, CV_32S);

		if (numLabels <= 1) // 라벨 0 = 배경만
			return regions;

		regions.reserve(static_cast<size_t>(numLabels - 1));

		for (int label = 1; label < numLabels; ++label)
		{
			const int left = stats.at<int>(label, cv::CC_STAT_LEFT);
			const int top = stats.at<int>(label, cv::CC_STAT_TOP);
			const int width = stats.at<int>(label, cv::CC_STAT_WIDTH);
			const int height = stats.at<int>(label, cv::CC_STAT_HEIGHT);
			const int compArea = stats.at<int>(label, cv::CC_STAT_AREA);

			if (compArea < minArea)
				continue;
			if (width <= 0 || height <= 0)
				continue;

			Region region;

			// --- 라벨 ROI 스캔 → 런렝스 + 지역 마스크 동시 생성 ---
			cv::Mat roiMask = cv::Mat::zeros(height, width, CV_8UC1);
			for (int ry = 0; ry < height; ++ry)
			{
				const int gy = top + ry;
				const int* pLabel = labels.ptr<int>(gy);
				unsigned char* pRoi = roiMask.ptr<unsigned char>(ry);

				int runStart = -1;
				for (int rx = 0; rx < width; ++rx)
				{
					const int gx = left + rx;
					const bool on = (pLabel[gx] == label);
					if (on)
					{
						pRoi[rx] = 255;
						if (runStart < 0)
							runStart = rx;
					}
					if ((!on || rx == width - 1) && runStart >= 0)
					{
						const int runEnd = on ? rx : (rx - 1);
						region.AddRun(gy, left + runStart, left + runEnd);
						runStart = -1;
					}
				}
			}

			// --- 컨투어(외곽+구멍) ---
			std::vector<std::vector<cv::Point> > contours;
			std::vector<cv::Vec4i> hierarchy;
			try
			{
				cv::findContours(roiMask, contours, hierarchy,
					cv::RETR_CCOMP, cv::CHAIN_APPROX_NONE);
			}
			catch (const cv::Exception&)
			{
				contours.clear();
				hierarchy.clear();
			}

			// ROI 지역좌표 → 전체 이미지 좌표로 오프셋 보정
			for (size_t ci = 0; ci < contours.size(); ++ci)
			{
				for (size_t pi = 0; pi < contours[ci].size(); ++pi)
				{
					contours[ci][pi].x += left;
					contours[ci][pi].y += top;
				}
			}

			region.SetContours(contours, hierarchy);
			regions.push_back(region);
		}
	}
	catch (const cv::Exception&)
	{
		// OpenCV 예외 발생 시 지금까지 수집한 결과만 반환.
	}
	catch (...)
	{
	}

	return regions;
}

} // namespace Grf
