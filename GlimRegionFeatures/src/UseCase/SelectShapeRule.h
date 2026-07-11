#pragma once
// SelectShapeRule.h
// Halcon select_shape 재현 룰 엔진(UseCase). 스펙 7장.
//  - 특징값 이름 + [min,max] 구간 조건 배열
//  - AND/OR 결합으로 불량코드 판정.

#include <string>
#include <vector>
#include "Domain/FeatureVector.h"

namespace Grf {

// 단일 조건: featureName 값이 [min, max] 구간 안에 있는가
struct RuleCondition {
	std::string m_featureName;
	double m_min;
	double m_max;

	RuleCondition()
		: m_min(-1e300), m_max(1e300) {}
	RuleCondition(const std::string& name, double vmin, double vmax)
		: m_featureName(name), m_min(vmin), m_max(vmax) {}

	bool Evaluate(const FeatureVector& fv) const
	{
		const double v = fv.GetByName(m_featureName);
		return (v >= m_min && v <= m_max);
	}
};

enum RuleCombine {
	RULE_AND = 0,
	RULE_OR = 1
};

// 불량코드 하나 = 조건들의 AND/OR
struct DefectRule {
	std::string m_code;                       // 불량코드(예: PINHOLE)
	RuleCombine m_combine;                     // AND/OR
	std::vector<RuleCondition> m_conditions;
	int m_priority;                            // 낮을수록 우선(동시 매칭 시)

	DefectRule()
		: m_combine(RULE_AND), m_priority(0) {}

	bool Evaluate(const FeatureVector& fv) const
	{
		if (m_conditions.empty())
			return false;
		if (m_combine == RULE_AND)
		{
			for (size_t i = 0; i < m_conditions.size(); ++i)
				if (!m_conditions[i].Evaluate(fv))
					return false;
			return true;
		}
		else // OR
		{
			for (size_t i = 0; i < m_conditions.size(); ++i)
				if (m_conditions[i].Evaluate(fv))
					return true;
			return false;
		}
	}
};

class SelectShapeRule {
public:
	SelectShapeRule();

	void AddRule(const DefectRule& rule);
	void SetRules(const std::vector<DefectRule>& rules);
	void Clear();

	size_t RuleCount() const { return m_rules.size(); }
	const std::vector<DefectRule>& Rules() const { return m_rules; }

	// 매칭되는 첫 불량코드 반환(priority 오름차순). 없으면 defaultCode.
	std::string Classify(const FeatureVector& fv, const std::string& defaultCode = "OK") const;

	// 매칭되는 모든 불량코드 반환.
	std::vector<std::string> ClassifyAll(const FeatureVector& fv) const;

private:
	std::vector<DefectRule> m_rules;
};

} // namespace Grf
