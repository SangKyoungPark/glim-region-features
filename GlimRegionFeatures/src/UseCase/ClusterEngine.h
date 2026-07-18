#pragma once
// ClusterEngine.h
// 무상태 UseCase: FeatureMatrix + ClusterParams -> cv::kmeans 기반 다차원 군집화(ClusterResult).
// "군집 계산 단일 소스"(설계 원칙) — 웹/뷰어는 이 엔진의 실행 결과만 시각화한다.
//
// 결정성 보장(설계 문서 규칙):
//  1) cv::setRNGSeed(seed) 고정 + 고정 attempts + KMEANS_PP_CENTERS + 고정 TermCriteria
//  2) 군집화는 "단일 스레드 집계 단계"에서만 호출할 것(cv::setRNGSeed 는 OpenCV 전역 RNG 를 건드림).
//     파일 단위 병렬 검사(CsvExporter::ExportFolder)와는 무관 — 그 결과물(CSV)을 다 만든 뒤
//     별도 단계로 1회 호출하는 것을 전제로 한다.
//  3) 입력 샘플 순서 = FeatureMatrix 행 순서(=CSV 정렬 순서) 그대로 사용.
//  4) 정준(canonical) 라벨 재매핑: cluster_id 를 "군집 크기 desc, 동률 시 첫 feature 중심 오름차순"
//     순서로 재부여 -> 같은 입력이면 라벨까지 항상 동일하게 재현된다.
//  5) scaler 파라미터(ClusterResult::m_scaler)를 결과에 기록 -> 동일 변환 재적용/원 스케일 복원 가능.
//
// 재진입 안전: 클래스 자체는 멤버 변수가 없어(무상태) 여러 쓰레드가 서로 다른 Run() 호출을
// 동시에 수행해도 안전하다. 다만 규칙 2)에 따라 실제 배치 파이프라인에서는 군집화 단계를
// 단일 호출로 운용할 것을 권장한다(정확성 문제라기보다 "재현성 있는 운용 관례"에 대한 권고).

#include <string>
#include <vector>
#include "Domain/FeatureMatrix.h"
#include "Domain/ClusterResult.h"

namespace Grf {

struct ClusterParams {
	std::vector<std::string> m_features; // 비어있으면 FeatureMatrix 전 컬럼 사용
	int  m_scaleMode;   // 0=zscore 1=minmax 2=robust (FeatureScaler::ScaleMode)
	bool m_logAreaLike;
	int  m_k;           // >0 이면 지정 K 로 실행. <=0 은 실패(자동 K 는 KSelector 로 먼저 결정할 것)
	int  m_kMax;        // KSelector 스윕 상한(엔진 자체는 참고만, 실제 스윕은 KSelector 가 수행)
	int  m_seed;
	int  m_attempts;

	ClusterParams()
		: m_scaleMode(0), m_logAreaLike(false), m_k(0), m_kMax(8), m_seed(12345), m_attempts(5) {}
};

class ClusterEngine {
public:
	ClusterEngine();

	// K 지정 실행. params.m_k<=0 이거나 K>샘플수 등 위험 입력은 가드 후 ClusterResult::m_ok=false 로 반환.
	ClusterResult Run(const FeatureMatrix& matrix, const ClusterParams& params) const;

	// 실루엣 계수(평균). scaled 좌표(FeatureScaler::Apply 결과)와 라벨을 그대로 받는다.
	//  KSelector 의 K 스윕에서도 재사용(단일 소스)한다. O(N^2) — 오프라인 튜닝 용도 전제.
	static double ComputeSilhouette(const std::vector<std::vector<double> >& scaled,
		const std::vector<int>& labels, int k);

private:
	static std::vector<int> CanonicalRelabel(
		const std::vector<int>& rawLabels, const std::vector<std::vector<double> >& scaledCentroids,
		std::vector<std::vector<double> >& centroidsOut, std::vector<int>& sizesOut);
};

} // namespace Grf
