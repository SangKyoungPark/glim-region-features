#pragma once
// FeatureCalculator.h
// Region → FeatureVector 계산기(UseCase).
// 우선순위 1(크기/형상/타원) + 우선순위 2(외접·내접/위상/Hu) 특징값을 계산한다.
// 실시간 검사 삽입 전제: 입력은 const 참조, 내부 재할당 최소화, 위험 연산은 가드+try-catch.

#include "Domain/Region.h"
#include "Domain/FeatureVector.h"

namespace Grf {

class FeatureCalculator {
public:
	FeatureCalculator();

	// Region 전체 특징값 계산.
	FeatureVector Compute(const Region& region) const;

	// 우선순위 2 특징값 계산 여부(성능 옵션). 기본 true.
	void SetComputePriority2(bool enable) { m_computePriority2 = enable; }

private:
	// --- 우선순위 1 ---
	void ComputeAreaCenter(const Region& region, const RawMoments& rm, FeatureVector& fv) const;
	void ComputeAreaHoles(const Region& region, FeatureVector& fv) const;
	void ComputeContLength(const Region& region, FeatureVector& fv) const;
	void ComputeDiameter(const Region& region, FeatureVector& fv) const;
	void ComputeShapeFactors(const Region& region, FeatureVector& fv) const;
	void ComputeRoundness(const Region& region, FeatureVector& fv) const;
	void ComputeEllipticAxis(const Region& region, const RawMoments& rm, FeatureVector& fv) const;
	void ComputeEccentricity(FeatureVector& fv) const;
	void ComputeOrientation(const Region& region, FeatureVector& fv) const;

	// --- 우선순위 2 ---
	void ComputeBoundingRects(const Region& region, FeatureVector& fv) const;
	void ComputeCircles(const Region& region, FeatureVector& fv) const;
	void ComputeTopology(const Region& region, FeatureVector& fv) const;
	void ComputeHuMoments(const Region& region, FeatureVector& fv) const;
	void ComputeInnerRectangle(const Region& region, FeatureVector& fv) const;
	void ComputeRunlength(const Region& region, FeatureVector& fv) const;
	void ComputeDerived(FeatureVector& fv) const;

	bool m_computePriority2;
};

} // namespace Grf
