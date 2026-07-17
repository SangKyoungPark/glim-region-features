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

void ScoreNormalizer::UpsertConfig(const ScoreConfig& config)
{
	for (size_t i = 0; i < m_configs.size(); ++i)
	{
		if (m_configs[i].m_featureName == config.m_featureName)
		{
			m_configs[i] = config; // 제자리 교체(순서 유지)
			return;
		}
	}
	m_configs.push_back(config);
}

const std::vector<ScoreConfig>& ScoreNormalizer::DefaultConfigs()
{
	// 전 스칼라 특징값 기본 정규화 범위(고정 순서). 컬럼 순서 결정성의 근거.
	//  - 비율형(0~1): vMin=0, vMax=1
	//  - 무한 스케일형(area/contlength/diameter 등): 공정 가설(스펙 9장) 기반 보수적 범위
	//  - 방향(increasing): 값이 클수록 Score 가 높아지는 방향
	//  - 위치성(center_row/col, 각도 phi/orientation, 좌표)은 제외
	static const std::vector<ScoreConfig> s_table = []()
	{
		std::vector<ScoreConfig> t;
		// 형상 계수(비율형 0~1)
		t.push_back(ScoreConfig("circularity",      0.0,   1.0,   true));
		t.push_back(ScoreConfig("compactness",      1.0,   5.0,   true));
		t.push_back(ScoreConfig("convexity",        0.0,   1.0,   true));
		t.push_back(ScoreConfig("rectangularity",   0.0,   1.0,   true));
		t.push_back(ScoreConfig("roundness",        0.0,   1.0,   true));
		t.push_back(ScoreConfig("sides",            0.0,  20.0,   true));
		// 타원/편심
		t.push_back(ScoreConfig("anisometry",       1.0,   5.0,   true));
		t.push_back(ScoreConfig("bulkiness",        0.0,   2.0,   true));
		t.push_back(ScoreConfig("structure_factor", 0.0,   5.0,   true));
		t.push_back(ScoreConfig("ra",               5.0, 500.0,   true));
		t.push_back(ScoreConfig("rb",               5.0, 500.0,   true));
		// 크기/기본(무한 스케일형)
		t.push_back(ScoreConfig("area",            10.0, 5000.0,  true));
		t.push_back(ScoreConfig("contlength",      10.0, 1000.0,  true));
		t.push_back(ScoreConfig("diameter",         5.0,  500.0,  true));
		t.push_back(ScoreConfig("area_holes",       0.0, 1000.0,  true));
		// 위상
		t.push_back(ScoreConfig("holes",            0.0,   10.0,  true));
		t.push_back(ScoreConfig("euler_number",   -10.0,    1.0,  true));
		// 파생/외접·내접
		t.push_back(ScoreConfig("aspect_ratio",     1.0,   10.0,  true));
		t.push_back(ScoreConfig("fill_ratio",       0.0,    1.0,  true));
		t.push_back(ScoreConfig("inner_outer_ratio",0.0,    1.0,  true));
		t.push_back(ScoreConfig("inner_rect_fill_ratio", 0.0, 1.0, true));
		// runlength
		t.push_back(ScoreConfig("num_runs",         1.0, 1000.0,  true));
		t.push_back(ScoreConfig("k_factor",         0.0,    5.0,  true));
		t.push_back(ScoreConfig("l_factor",         0.0,    5.0,  true));
		t.push_back(ScoreConfig("mean_run_length",  1.0,  500.0,  true));
		// Phase 3: 두께/런분포 요약(크기 성격 → Score 의미 있음)
		t.push_back(ScoreConfig("thickness_mean",   1.0,  500.0,  true));
		t.push_back(ScoreConfig("thickness_max",    1.0,  500.0,  true));
		t.push_back(ScoreConfig("run_len_max",      1.0,  500.0,  true));
		// Phase 3: 모멘트 불변량(스케일·회전 불변 형상 기술자 → 보수적 범위, 튜닝 대상)
		t.push_back(ScoreConfig("moment_phi1",      0.0,    1.0,  true));
		t.push_back(ScoreConfig("moment_phi2",      0.0,    0.5,  true));
		t.push_back(ScoreConfig("moment_psi1",      0.0,    0.5,  true));
		t.push_back(ScoreConfig("moment_psi2",      0.0,    0.5,  true));
		t.push_back(ScoreConfig("moment_psi3",      0.0,    0.5,  true));
		t.push_back(ScoreConfig("moment_psi4",      0.0,    0.1,  true));
		return t;
	}();
	return s_table;
}

void ScoreNormalizer::SeedDefaults()
{
	const std::vector<ScoreConfig>& t = DefaultConfigs();
	for (size_t i = 0; i < t.size(); ++i)
		UpsertConfig(t[i]);
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
