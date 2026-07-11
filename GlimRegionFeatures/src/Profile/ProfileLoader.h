#pragma once
// ProfileLoader.h
// 공정 프로파일(코터/롤프레스/슬리터) INI 로더(Profile 계층).
// INI → ScoreNormalizer 설정 + SelectShapeRule 룰 세트로 변환.
//
// INI 포맷:
//  [Profile]
//  name = Coater
//
//  [Score]
//  ; featureName = vMin, vMax, direction(inc|dec)
//  circularity = 0.3, 1.0, inc
//  convexity   = 0.5, 1.0, inc
//
//  [SelectShape]
//  codes = PINHOLE, TEAR, BUBBLE   ; 판정할 불량코드 목록
//
//  [Rule_PINHOLE]
//  combine  = AND                  ; AND|OR
//  priority = 10                   ; 낮을수록 우선
//  ; featureName = min, max
//  circularity = 0.70, 1.00
//  area        = 5, 200

#include <string>
#include "Profile/IniFile.h"
#include "UseCase/ScoreNormalizer.h"
#include "UseCase/SelectShapeRule.h"

namespace Grf {

class ProfileLoader {
public:
	ProfileLoader();

	bool Load(const std::string& iniPath);
	bool IsLoaded() const { return m_loaded; }

	const std::string& ProfileName() const { return m_profileName; }
	const ScoreNormalizer& Normalizer() const { return m_normalizer; }
	const SelectShapeRule& RuleEngine() const { return m_ruleEngine; }

	ScoreNormalizer& NormalizerRef() { return m_normalizer; }
	SelectShapeRule& RuleEngineRef() { return m_ruleEngine; }

private:
	void ParseScoreSection(const IniFile& ini);
	void ParseSelectShape(const IniFile& ini);

	std::string m_profileName;
	ScoreNormalizer m_normalizer;
	SelectShapeRule m_ruleEngine;
	bool m_loaded;
};

} // namespace Grf
