#pragma once
// ScoreNormalizer.h
// 원시 특징값 → 0~100 Score 정규화(UseCase). 스펙 8장.
//  score = 100 · clamp((v - vMin)/(vMax - vMin), 0, 1)
//  감소형(Decreasing)은 100 - score.

#include <string>
#include <vector>
#include "Domain/FeatureVector.h"
#include "Domain/ScoreResult.h"

namespace Grf {

// 특징값 하나에 대한 정규화 설정
struct ScoreConfig {
	std::string m_featureName;
	double m_vMin;
	double m_vMax;
	bool m_increasing; // true=증가형(값↑→Score↑), false=감소형

	ScoreConfig()
		: m_vMin(0.0), m_vMax(1.0), m_increasing(true) {}
	ScoreConfig(const std::string& name, double vMin, double vMax, bool increasing)
		: m_featureName(name), m_vMin(vMin), m_vMax(vMax), m_increasing(increasing) {}
};

class ScoreNormalizer {
public:
	ScoreNormalizer();

	void AddConfig(const ScoreConfig& config);
	void SetConfigs(const std::vector<ScoreConfig>& configs);
	void Clear();

	// 같은 이름의 설정이 있으면 제자리 교체(값/방향 갱신), 없으면 추가.
	//  → 기본 테이블 위에 프로파일 INI 를 덮어쓸 때 컬럼 순서를 유지한다.
	void UpsertConfig(const ScoreConfig& config);

	// 전 스칼라 특징값 기본 정규화 테이블(고정 순서)로 현재 설정을 채운다.
	//  프로파일이 없거나 [Score] 가 비어도 모든 특징값 Score 가 계산되도록 한다.
	//  위치성(row/col)·각도(phi/orientation) 값은 Score 대상에서 제외.
	void SeedDefaults();

	// 기본 정규화 테이블(고정 순서, 스칼라 특징값만). 최초 1회 생성 후 공유.
	static const std::vector<ScoreConfig>& DefaultConfigs();

	size_t ConfigCount() const { return m_configs.size(); }
	const std::vector<ScoreConfig>& Configs() const { return m_configs; }

	// FeatureVector → ScoreResult(각 특징값별 0~100).
	ScoreResult Normalize(const FeatureVector& fv) const;

	// 단일 값 정규화(유틸).
	static double NormalizeValue(double v, const ScoreConfig& config);

private:
	std::vector<ScoreConfig> m_configs;
};

} // namespace Grf
