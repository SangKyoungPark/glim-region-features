#pragma once
// KSelector.h
// 무상태 UseCase: K=kMin..kMax 스윕 -> 실루엣/관성(inertia) 곡선 + 실루엣 최대 K 추천.
// 재진입 안전(멤버 없음). ClusterEngine::ComputeSilhouette 를 재사용해 "군집 계산 단일 소스"
// 원칙을 유지한다(실루엣 계산 로직이 ClusterEngine 과 분기되지 않도록).

#include <string>
#include <vector>
#include "Domain/FeatureMatrix.h"
#include "UseCase/ClusterEngine.h"

namespace Grf {

class KSelector {
public:
	struct Curve {
		std::vector<int> m_k;
		std::vector<double> m_silhouette;
		std::vector<double> m_inertia; // cv::kmeans 관성(스케일된 좌표 기준 compactness)
		int m_recommendedK;            // 실루엣 최대 K(동률이면 먼저 나온 작은 K)
		bool m_ok;
		std::string m_error;

		Curve() : m_recommendedK(0), m_ok(false) {}
	};

	KSelector();

	// baseParams 의 m_k/m_kMax 는 무시되고 [kMin,kMax] 범위로 스윕한다.
	// (m_features/m_scaleMode/m_logAreaLike/m_seed/m_attempts 는 그대로 사용)
	// 유효 K 범위가 없으면(샘플 수 부족 등) Curve::m_ok=false.
	Curve Sweep(const FeatureMatrix& matrix, const ClusterParams& baseParams, int kMin, int kMax) const;
};

} // namespace Grf
