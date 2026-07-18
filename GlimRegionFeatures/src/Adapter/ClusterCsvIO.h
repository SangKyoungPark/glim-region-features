#pragma once
// ClusterCsvIO.h
// Adapter: 파일 I/O 를 담당(Domain/UseCase 는 파일의 존재를 알지 못한다).
//  - result.csv(CsvExporter::ExportFolder 산출물) -> FeatureMatrix 로드
//  - ClusterResult(+ClusterParams, +선택 KSelector::Curve) <-> clusters.json 저장/로드
//  - (옵트인) result.csv 에 cluster_id 컬럼을 덧붙이는 인플레이스 갱신
// clusters.json 스키마 상세는 docs/CLUSTERING.md 참조.
// 모든 정적 메서드는 무상태(입력 인자/지역변수로만 동작) — 재진입 안전.

#include <string>
#include <vector>
#include "Domain/FeatureMatrix.h"
#include "Domain/ClusterResult.h"
#include "UseCase/ClusterEngine.h"   // ClusterParams
#include "UseCase/KSelector.h"       // KSelector::Curve

namespace Grf {

class ClusterCsvIO {
public:
	// result.csv 를 읽어 FeatureMatrix 로 변환.
	//  featureNames 가 비어있으면 CSV 의 원시 특징값 컬럼(FileName/FilePath/RegionIndex/Channel,
	//  score_*, ClassifiedCode, *_mm 파생 컬럼 제외) 전부를 사용한다("전체 스칼라" 모드).
	//  RegionIndex<0(NO_REGION) 행은 제외한다. 실패 시 false, errorOut 채움.
	static bool LoadFeatureMatrix(const std::string& csvPath,
		const std::vector<std::string>& featureNames,
		FeatureMatrix& matrixOut, std::string& errorOut);

	// ClusterResult(+params, +선택 kCurve) -> clusters.json 저장.
	static bool SaveClusters(const std::string& jsonPath, const FeatureMatrix& matrix,
		const ClusterResult& result, const ClusterParams& params,
		const KSelector::Curve* curve, std::string& errorOut);

	// clusters.json -> ClusterResult(+params) 복원.
	//  주의: resultOut.m_labels 의 행 순서는 저장 당시 FeatureMatrix 행 순서와 1:1 대응한다.
	//  같은 result.csv 를 다시 LoadFeatureMatrix 로 읽으면(파일이 그 사이 바뀌지 않았다면)
	//  동일한 순서로 재현되므로 인덱스 매칭이 유효하다.
	static bool LoadClusters(const std::string& jsonPath,
		ClusterResult& resultOut, ClusterParams& paramsOut, std::string& errorOut);

	// (옵트인) result.csv 에 cluster_id 컬럼을 덧붙여 같은 경로에 다시 저장한다(기본 미사용).
	//  key = FileName+RegionIndex(+Channel) 매칭. matrix/result 는 이 csvPath 로부터 로드된 것이어야 한다.
	static bool WriteClusterIdInplace(const std::string& csvPath,
		const FeatureMatrix& matrix, const ClusterResult& result, std::string& errorOut);

private:
	static std::string JsonEscape(const std::string& s);
	static std::string JsonNum(double v);
};

} // namespace Grf
