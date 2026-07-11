#pragma once
// IniFile.h
// 경량 INI 파서(Profile 계층). 외부 의존 없이 std::ifstream 으로 파싱.
//  - 섹션 [Section], key=value, ';' 또는 '#' 주석 지원
//  - 선행 UTF-8 BOM 자동 제거
//  - 섹션/키 열거 지원(룰 섹션의 임의 키 조회용)
// 주의: INI 파일 주석은 CP949 오독 방지를 위해 ASCII 로 유지 권장.

#include <string>
#include <vector>
#include <map>

namespace Grf {

class IniFile {
public:
	IniFile();

	bool Load(const std::string& path);
	bool IsLoaded() const { return m_loaded; }

	// 섹션 이름 목록(등장 순서 유지)
	std::vector<std::string> Sections() const;

	bool HasSection(const std::string& section) const;

	// 섹션의 키 목록(등장 순서 유지)
	std::vector<std::string> Keys(const std::string& section) const;

	std::string GetString(const std::string& section, const std::string& key,
		const std::string& def = std::string()) const;
	double GetDouble(const std::string& section, const std::string& key, double def) const;
	int GetInt(const std::string& section, const std::string& key, int def) const;
	bool GetBool(const std::string& section, const std::string& key, bool def) const;

	// "a, b, c" → ["a","b","c"] (trim 적용)
	static std::vector<std::string> SplitCsv(const std::string& value);
	static std::string Trim(const std::string& s);

private:
	struct Section {
		std::string m_name;
		std::vector<std::pair<std::string, std::string> > m_pairs; // 순서 유지
	};

	const Section* FindSection(const std::string& name) const;

	std::vector<Section> m_sections;
	bool m_loaded;
};

} // namespace Grf
