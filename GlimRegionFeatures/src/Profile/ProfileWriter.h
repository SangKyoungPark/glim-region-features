#pragma once
// ProfileWriter.h
// Profile 계층: ClusterResult + 사용자 라벨(clusterId->불량코드) -> 프로파일 INI 초안 생성.
// ProfileLoader(INI -> 룰 엔진)의 역방향. 분위수(envelope) 계산은 여기(엔진)에서 수행하여
// 웹/뷰어 등 호출자가 동일 스펙을 공유하도록 한다(단일 소스 원칙 — 설계 문서).
// 무상태(stateless): 모든 계산은 인자로 받은 FeatureMatrix/ClusterResult 로만 수행, 재진입 안전.
// 생성된 INI 는 어디까지나 "초안"이며 사용자가 검토 후 저장하는 것을 전제로 한다.

#include <string>
#include <vector>
#include <map>
#include "Domain/FeatureMatrix.h"
#include "Domain/ClusterResult.h"

namespace Grf {

struct ProfileWriteParams {
	std::string m_profileName;           // [Profile] name
	std::map<int, std::string> m_labels; // clusterId -> 불량코드(라벨 없는 cluster 는 초안에서 제외)
	double m_percentileLow;              // 기본 5 (0~100)
	double m_percentileHigh;             // 기본 95 (0~100)

	ProfileWriteParams()
		: m_percentileLow(5.0), m_percentileHigh(95.0) {}
};

class ProfileWriter {
public:
	ProfileWriter();

	// INI 초안 텍스트 생성.
	//  features 가 비어있으면 result.m_scaler.m_features(군집화에 사용한 feature 목록)를 사용한다.
	// 실패(빈 matrix/실패한 result/라벨 없음/유효 feature 없음) 시 okOut=false, errorOut 채움, 빈 문자열 반환.
	std::string BuildIni(const FeatureMatrix& matrix, const ClusterResult& result,
		const ProfileWriteParams& params, const std::vector<std::string>& features,
		bool& okOut, std::string& errorOut) const;

private:
	// 값 목록에서 percentile(0~100, 선형보간) 계산. 값이 없으면 0 반환.
	static double Percentile(std::vector<double> values, double p);
};

} // namespace Grf
