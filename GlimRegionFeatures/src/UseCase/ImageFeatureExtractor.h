#pragma once
// ImageFeatureExtractor.h
// 고수준 facade(UseCase): 크롭 이미지 1장(그레이/컬러) → 흑/백 극성별 이진화 → 컨투어(Region)
//  → Region 별 특징값(FeatureVector) 전부. "128x128 던지면 알아서 feature" 단일 진입점.
//
//  기존 조각(CpuPreprocessor + RegionExtractor + FeatureCalculator)을 한 번의 호출로 감싼다.
//  호출자(뷰어/Batch/웹)는 이 함수만 부르면 극성 분리·이진화·컨투어·특징값 계산을 한 번에 얻는다.
//
// [흑/백 불량 각각]
//  m_black : 어두운 불량(흑점/구멍 등) — POLARITY_DARK 기본. tag 'B'.
//  m_white : 밝은 불량(돌기/이물 등) — POLARITY_BRIGHT 기본. tag 'W'.
//  두 채널은 독립 이진화 파라미터를 가지며, 각각 켜고 끌 수 있다. 둘 다 켜면 두 채널을 이어붙여 반환.
//
// [무상태/재진입 안전] 멤버 변수 없음, 모든 계산은 const 메서드 + 지역 변수. 여러 쓰레드가
//  서로 다른 입력으로 동시에 Extract() 호출해도 안전(내부 CpuPreprocessor/RegionExtractor/
//  FeatureCalculator 모두 무상태). 단, 군집화 단계(ClusterEngine)와 달리 전역 RNG 를 건드리지 않는다.

#include <vector>
#include <opencv2/core.hpp>
#include "Domain/Region.h"
#include "Domain/FeatureVector.h"
#include "Adapter/IPreprocessor.h"

namespace Grf {

// 극성 채널 하나의 이진화 옵션(흑/백 불량 각각 독립 설정).
struct ExtractChannel {
	bool m_enabled;             // 이 채널을 계산할지
	BinarizeParams m_binarize;  // 이진화 파라미터(mode/threshold/offset/극성 등)

	ExtractChannel() : m_enabled(true) {}
	ExtractChannel(bool enabled, const BinarizeParams& binarize)
		: m_enabled(enabled), m_binarize(binarize) {}
};

// 이미지 → 특징값 추출 옵션.
struct ImageExtractOptions {
	ExtractChannel m_black;   // 'B' 흑(어두운) 불량 채널
	ExtractChannel m_white;   // 'W' 백(밝은) 불량 채널
	int  m_minArea;           // 노이즈 제거: 이 픽셀 수 미만 Region 은 버림(0=전부 유지)
	bool m_computePriority2;  // 우선순위2 특징값(외접/내접/Hu/모멘트 등) 계산 여부(성능)

	// 기본값: 흑=OTSU/DARK, 백=OTSU/BRIGHT, minArea=5, priority2 on.
	//  (검사기 크롭은 이진화 전 원본이므로 자동 임계값(OTSU)이 실전 기본 — 프로젝트 메모 근거)
	ImageExtractOptions();
};

// 한 Region(=한 불량 후보)의 추출 결과.
struct DefectSample {
	char          m_channel;      // 'B' 또는 'W'
	int           m_regionIndex;  // 채널 내 0-based 인덱스
	Region        m_region;       // 컨투어/런렝스 포함(뷰어 오버레이용)
	FeatureVector m_features;     // 전체 특징값

	DefectSample() : m_channel('W'), m_regionIndex(-1) {}
};

class ImageFeatureExtractor {
public:
	ImageFeatureExtractor();

	// 이미지 1장 → 활성 채널(흑/백)별 Region 특징값 전부.
	//  src: 8UC1 그레이 또는 컬러(내부에서 그레이 변환). 빈 이미지는 빈 벡터.
	//  결과 순서: 흑 채널 Region 전부(index 오름차순) → 백 채널 Region 전부. 항상 결정적.
	std::vector<DefectSample> Extract(const cv::Mat& src, const ImageExtractOptions& opt) const;

	// 단일 채널만 추출(내부 공용 + 호출자 편의). polarityTag = 'B' 또는 'W'.
	//  이진화/추출 중 예외는 방어적으로 삼켜 빈 결과를 돌려준다(다른 채널 계속 진행용).
	std::vector<DefectSample> ExtractOneChannel(const cv::Mat& src,
		const BinarizeParams& binarize, char polarityTag,
		int minArea, bool computePriority2) const;
};

} // namespace Grf
