// ProfileLoader.cpp
#include "Profile/ProfileLoader.h"
#include <cstdlib>

namespace Grf {

ProfileLoader::ProfileLoader()
	: m_loaded(false)
{
}

bool ProfileLoader::Load(const std::string& iniPath)
{
	m_loaded = false;
	m_profileName.clear();
	m_normalizer.Clear();
	m_ruleEngine.Clear();

	IniFile ini;
	if (!ini.Load(iniPath))
		return false;

	m_profileName = ini.GetString("Profile", "name", "Unknown");

	ParseScoreSection(ini);
	ParseSelectShape(ini);

	m_loaded = true;
	return true;
}

void ProfileLoader::ParseScoreSection(const IniFile& ini)
{
	// [Score] featureName = vMin, vMax, direction
	if (!ini.HasSection("Score"))
		return;

	std::vector<std::string> keys = ini.Keys("Score");
	for (size_t i = 0; i < keys.size(); ++i)
	{
		const std::string& name = keys[i];
		std::string raw = ini.GetString("Score", name, std::string());
		std::vector<std::string> parts = IniFile::SplitCsv(raw);
		if (parts.size() < 2)
			continue;

		ScoreConfig cfg;
		cfg.m_featureName = name;
		cfg.m_vMin = std::atof(parts[0].c_str());
		cfg.m_vMax = std::atof(parts[1].c_str());
		cfg.m_increasing = true;
		if (parts.size() >= 3)
		{
			std::string dir = IniFile::Trim(parts[2]);
			if (dir == "dec" || dir == "DEC" || dir == "decreasing" || dir == "down")
				cfg.m_increasing = false;
		}
		m_normalizer.AddConfig(cfg);
	}
}

void ProfileLoader::ParseSelectShape(const IniFile& ini)
{
	// [SelectShape] codes = A, B, C  →  각 [Rule_X] 섹션 파싱
	std::string codesStr = ini.GetString("SelectShape", "codes", std::string());
	std::vector<std::string> codes = IniFile::SplitCsv(codesStr);

	for (size_t c = 0; c < codes.size(); ++c)
	{
		const std::string& code = codes[c];
		if (code.empty())
			continue;

		const std::string section = "Rule_" + code;
		if (!ini.HasSection(section))
			continue;

		DefectRule rule;
		rule.m_code = code;

		std::string combineStr = ini.GetString(section, "combine", "AND");
		combineStr = IniFile::Trim(combineStr);
		rule.m_combine = (combineStr == "OR" || combineStr == "or") ? RULE_OR : RULE_AND;
		rule.m_priority = ini.GetInt(section, "priority", static_cast<int>(c));

		// combine/priority 를 제외한 나머지 키 = 조건
		std::vector<std::string> keys = ini.Keys(section);
		for (size_t k = 0; k < keys.size(); ++k)
		{
			const std::string& key = keys[k];
			if (key == "combine" || key == "priority")
				continue;

			std::string raw = ini.GetString(section, key, std::string());
			std::vector<std::string> parts = IniFile::SplitCsv(raw);
			if (parts.size() < 2)
				continue;

			RuleCondition cond;
			cond.m_featureName = key;
			cond.m_min = std::atof(parts[0].c_str());
			cond.m_max = std::atof(parts[1].c_str());
			rule.m_conditions.push_back(cond);
		}

		if (!rule.m_conditions.empty())
			m_ruleEngine.AddRule(rule);
	}
}

} // namespace Grf
