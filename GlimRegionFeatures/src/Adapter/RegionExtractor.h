#pragma once
// RegionExtractor.h
// cv::Mat 이진화 이미지 → Region 목록 추출 어댑터.
// cv::connectedComponentsWithStats(8-연결) 로 라벨링 후,
// 라벨별 런렝스 + 컨투어(외곽/구멍) 를 채운다.

#include <vector>
#include <opencv2/core.hpp>
#include "Domain/Region.h"
#include "Adapter/IPreprocessor.h"

namespace Grf {

class RegionExtractor {
public:
	RegionExtractor();

	// 입력: 8UC1(0=배경, 그 외=전경). 8UC1 이 아니거나 0/255 가 아니어도 내부에서 이진화.
	// minArea: 이 픽셀 수 미만인 성분은 버린다(노이즈 제거). 0 이면 전부 유지.
	// (const 메서드 + 지역변수만 사용 → 서로 다른 입력에 대해 멀티쓰레드 동시 호출 안전)
	std::vector<Region> Extract(const cv::Mat& binary, int minArea = 1) const;

	// 원본(그레이/컬러) → 전처리기로 이진화 → Extract. 전처리 GPU 구현 삽입 지점.
	std::vector<Region> ExtractFromGray(const cv::Mat& src,
		const IPreprocessor& preprocessor, int minArea = 1) const;

	void SetConnectivity(int connectivity) { m_connectivity = connectivity; } // 4 또는 8

private:
	int m_connectivity;
};

} // namespace Grf
