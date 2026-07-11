// SelectShapeRule.cpp
#include "UseCase/SelectShapeRule.h"
#include <algorithm>

namespace Grf {

SelectShapeRule::SelectShapeRule()
{
}

void SelectShapeRule::AddRule(const DefectRule& rule)
{
	m_rules.push_back(rule);
}

void SelectShapeRule::SetRules(const std::vector<DefectRule>& rules)
{
	m_rules = rules;
}

void SelectShapeRule::Clear()
{
	m_rules.clear();
}

std::string SelectShapeRule::Classify(const FeatureVector& fv, const std::string& defaultCode) const
{
	try
	{
		// priority 오름차순으로 정렬된 인덱스 순회.
		std::vector<size_t> order(m_rules.size());
		for (size_t i = 0; i < m_rules.size(); ++i)
			order[i] = i;

		std::stable_sort(order.begin(), order.end(),
			[this](size_t a, size_t b) {
				return m_rules[a].m_priority < m_rules[b].m_priority;
			});

		for (size_t k = 0; k < order.size(); ++k)
		{
			const DefectRule& rule = m_rules[order[k]];
			if (rule.Evaluate(fv))
				return rule.m_code;
		}
	}
	catch (...)
	{
	}
	return defaultCode;
}

std::vector<std::string> SelectShapeRule::ClassifyAll(const FeatureVector& fv) const
{
	std::vector<std::string> matched;
	try
	{
		for (size_t i = 0; i < m_rules.size(); ++i)
		{
			if (m_rules[i].Evaluate(fv))
				matched.push_back(m_rules[i].m_code);
		}
	}
	catch (...)
	{
	}
	return matched;
}

} // namespace Grf
