#pragma once
// ScoreResult.h
// 특징값 정규화 결과(0~100 Score) 보관 도메인 모델.

#include <string>
#include <map>
#include <vector>

namespace Grf {

struct ScoreResult {
	std::map<std::string, double> m_scores; // featureName -> 0~100

	void Set(const std::string& name, double score) { m_scores[name] = score; }

	double Get(const std::string& name) const
	{
		std::map<std::string, double>::const_iterator it = m_scores.find(name);
		if (it == m_scores.end())
			return 0.0;
		return it->second;
	}

	bool Has(const std::string& name) const
	{
		return m_scores.find(name) != m_scores.end();
	}

	// 전체 평균 Score (설정된 특징값 기준). 없으면 0.
	double Overall() const
	{
		if (m_scores.empty())
			return 0.0;
		double sum = 0.0;
		for (std::map<std::string, double>::const_iterator it = m_scores.begin();
			it != m_scores.end(); ++it)
		{
			sum += it->second;
		}
		return sum / static_cast<double>(m_scores.size());
	}
};

} // namespace Grf
