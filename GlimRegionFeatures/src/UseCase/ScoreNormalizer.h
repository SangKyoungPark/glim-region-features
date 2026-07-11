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
