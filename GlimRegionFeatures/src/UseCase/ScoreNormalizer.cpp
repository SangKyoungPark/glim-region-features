// ScoreNormalizer.cpp
#include "UseCase/ScoreNormalizer.h"

namespace Grf {

ScoreNormalizer::ScoreNormalizer()
{
}

void ScoreNormalizer::AddConfig(const ScoreConfig& config)
{
	m_configs.push_back(config);
}

void ScoreNormalizer::SetConfigs(const std::vector<ScoreConfig>& configs)
{
	m_configs = configs;
}

void ScoreNormalizer::Clear()
{
	m_configs.clear();
}

double ScoreNormalizer::NormalizeValue(double v, const ScoreConfig& config)
{
	const double range = config.m_vMax - config.m_vMin;
	double t = 0.0;
	if (range > 1e-12)
		t = (v - config.m_vMin) / range;
	else if (range < -1e-12)
		t = (v - config.m_vMin) / range; // vMin>vMax 로 방향을 뒤집어 준 경우도 허용
	// clamp 0~1
	if (t < 0.0) t = 0.0;
	if (t > 1.0) t = 1.0;

	double score = 100.0 * t;
	if (!config.m_increasing)
		score = 100.0 - score;
	return score;
}

ScoreResult ScoreNormalizer::Normalize(const FeatureVector& fv) const
{
	ScoreResult result;
	try
	{
		for (size_t i = 0; i < m_configs.size(); ++i)
		{
			const ScoreConfig& cfg = m_configs[i];
			const double v = fv.GetByName(cfg.m_featureName);
			const double score = NormalizeValue(v, cfg);
			result.Set(cfg.m_featureName, score);
		}
	}
	catch (...)
	{
	}
	return result;
}

} // namespace Grf
