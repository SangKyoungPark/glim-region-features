#pragma once
// RegionExtractor.h
// cv::Mat 이진화 이미지 → Region 목록 추출 어댑터.
// cv::connectedComponentsWithStats(8-연결) 로 라벨링 후,
// 라벨별 런렝스 + 컨투어(외곽/구멍) 를 채운다.

#include <vector>
#include <opencv2/core.hpp>
#include "Domain/Region.h"

namespace Grf {

class RegionExtractor {
public:
	RegionExtractor();

	// 입력: 8UC1(0=배경, 그 외=전경). 8UC1 이 아니거나 0/255 가 아니어도 내부에서 이진화.
	// minArea: 이 픽셀 수 미만인 성분은 버린다(노이즈 제거). 0 이면 전부 유지.
	std::vector<Region> Extract(const cv::Mat& binary, int minArea = 1) const;

	void SetConnectivity(int connectivity) { m_connectivity = connectivity; } // 4 또는 8

private:
	int m_connectivity;
};

} // namespace Grf
