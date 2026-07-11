// IniFile.cpp
#include "Profile/IniFile.h"
#include <fstream>
#include <sstream>
#include <cstdlib>

namespace Grf {

IniFile::IniFile()
	: m_loaded(false)
{
}

std::string IniFile::Trim(const std::string& s)
{
	const char* ws = " \t\r\n";
	size_t b = s.find_first_not_of(ws);
	if (b == std::string::npos)
		return std::string();
	size_t e = s.find_last_not_of(ws);
	return s.substr(b, e - b + 1);
}

std::vector<std::string> IniFile::SplitCsv(const std::string& value)
{
	std::vector<std::string> out;
	std::stringstream ss(value);
	std::string item;
	while (std::getline(ss, item, ','))
		out.push_back(Trim(item));
	return out;
}

bool IniFile::Load(const std::string& path)
{
	m_sections.clear();
	m_loaded = false;

	std::ifstream in(path.c_str(), std::ios::binary);
	if (!in.is_open())
		return false;

	std::string line;
	int curIdx = -1; // 현재 섹션 인덱스(포인터는 push_back 재할당으로 무효화되므로 인덱스 사용)
	bool firstLine = true;

	while (std::getline(in, line))
	{
		// 선행 UTF-8 BOM 제거(첫 줄만)
		if (firstLine && line.size() >= 3 &&
			static_cast<unsigned char>(line[0]) == 0xEF &&
			static_cast<unsigned char>(line[1]) == 0xBB &&
			static_cast<unsigned char>(line[2]) == 0xBF)
		{
			line = line.substr(3);
		}
		firstLine = false;

		std::string t = Trim(line);
		if (t.empty())
			continue;
		if (t[0] == ';' || t[0] == '#')
			continue;

		if (t[0] == '[')
		{
			size_t end = t.find(']');
			if (end == std::string::npos)
				continue;
			Section sec;
			sec.m_name = Trim(t.substr(1, end - 1));
			m_sections.push_back(sec);
			curIdx = static_cast<int>(m_sections.size()) - 1;
			continue;
		}

		size_t eq = t.find('=');
		if (eq == std::string::npos)
			continue;

		std::string key = Trim(t.substr(0, eq));
		std::string val = Trim(t.substr(eq + 1));

		// 값 뒤 인라인 주석 제거(' ;' 또는 ' #')
		// 단, 값 자체에 세미콜론이 데이터로 쓰이지 않는다는 전제.
		size_t cpos = val.find(';');
		if (cpos != std::string::npos)
			val = Trim(val.substr(0, cpos));

		if (curIdx < 0)
		{
			// 섹션 없는 최상위 키 → 익명 섹션 생성
			Section sec;
			sec.m_name = "";
			m_sections.push_back(sec);
			curIdx = static_cast<int>(m_sections.size()) - 1;
		}
		m_sections[curIdx].m_pairs.push_back(std::make_pair(key, val));
	}

	m_loaded = true;
	return true;
}

const IniFile::Section* IniFile::FindSection(const std::string& name) const
{
	for (size_t i = 0; i < m_sections.size(); ++i)
	{
		if (m_sections[i].m_name == name)
			return &m_sections[i];
	}
	return NULL;
}

std::vector<std::string> IniFile::Sections() const
{
	std::vector<std::string> out;
	for (size_t i = 0; i < m_sections.size(); ++i)
		out.push_back(m_sections[i].m_name);
	return out;
}

bool IniFile::HasSection(const std::string& section) const
{
	return FindSection(section) != NULL;
}

std::vector<std::string> IniFile::Keys(const std::string& section) const
{
	std::vector<std::string> out;
	const Section* s = FindSection(section);
	if (s == NULL)
		return out;
	for (size_t i = 0; i < s->m_pairs.size(); ++i)
		out.push_back(s->m_pairs[i].first);
	return out;
}

std::string IniFile::GetString(const std::string& section, const std::string& key,
	const std::string& def) const
{
	const Section* s = FindSection(section);
	if (s == NULL)
		return def;
	for (size_t i = 0; i < s->m_pairs.size(); ++i)
	{
		if (s->m_pairs[i].first == key)
			return s->m_pairs[i].second;
	}
	return def;
}

double IniFile::GetDouble(const std::string& section, const std::string& key, double def) const
{
	std::string v = GetString(section, key, std::string());
	if (v.empty())
		return def;
	try { return std::atof(v.c_str()); }
	catch (...) { return def; }
}

int IniFile::GetInt(const std::string& section, const std::string& key, int def) const
{
	std::string v = GetString(section, key, std::string());
	if (v.empty())
		return def;
	try { return std::atoi(v.c_str()); }
	catch (...) { return def; }
}

bool IniFile::GetBool(const std::string& section, const std::string& key, bool def) const
{
	std::string v = GetString(section, key, std::string());
	if (v.empty())
		return def;
	v = Trim(v);
	if (v == "1" || v == "true" || v == "TRUE" || v == "True" ||
		v == "yes" || v == "YES" || v == "on" || v == "ON")
		return true;
	if (v == "0" || v == "false" || v == "FALSE" || v == "False" ||
		v == "no" || v == "NO" || v == "off" || v == "OFF")
		return false;
	return def;
}

} // namespace Grf
