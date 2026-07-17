#pragma once
// FeatureScaler.h
// 무상태(stateless) UseCase: FeatureMatrix 원시값 -> 스케일 적용 좌표.
// 재진입 안전(멤버 변수 없음, 모든 계산은 지역변수/인자로만 수행) — 멀티쓰레드 락프리 호출 가능.
//  scaleMode: 0=zscore(평균0,분산1) 1=minmax(0~1) 2=robust(median/IQR)
//  logAreaLike: 이름에 area/contlength/diameter 를 포함하는 컬럼에 log1p 선적용(면적류 왜곡 완화 옵션).

#include <string>
#include <vector>
#include "Domain/FeatureMatrix.h"
#include "Domain/ClusterResult.h" // ScalerParams

namespace Grf {

enum ScaleMode {
	SCALE_ZSCORE = 0,
	SCALE_MINMAX = 1,
	SCALE_ROBUST = 2
};

class FeatureScaler {
public:
	FeatureScaler();

	// 선택된 컬럼(featureNames)에 대해 scaler 파라미터를 적합(fit)한다.
	//  featureNames 가 비어있으면 matrix 의 전 컬럼을 사용한다.
	//  matrix 에 없는 이름은 무시(스킵)한다 — 결과 m_features 가 비어있으면 호출부는 실패로 간주할 것.
	ScalerParams Fit(const FeatureMatrix& matrix, const std::vector<std::string>& featureNames,
		int scaleMode, bool logAreaLike) const;

	// scaler 로 matrix 를 스케일링한 N x len(features) 좌표(행 순서는 matrix 와 동일)를 반환.
	//  matrix 에 scaler.m_features 이름이 없으면 그 컬럼은 0.0 으로 채운다(방어적 동작).
	std::vector<std::vector<double> > Apply(const FeatureMatrix& matrix, const ScalerParams& scaler) const;

	// 스케일된 좌표 1개(센트로이드 등)를 원 스케일로 역변환.
	std::vector<double> InverseTransform(const std::vector<double>& scaledPoint, const ScalerParams& scaler) const;

private:
	static bool IsAreaLikeName(const std::string& name);
};

} // namespace Grf
