#pragma once
// RegionRelation.h
// Halcon Regions>Features 의 "Region 간 관계/질의" 오퍼레이터 재현(UseCase).
//  - find_neighbors / hamming_distance / hamming_distance_norm
//  - get_region_index / select_region_point / select_region_spatial / spatial_relation
//  - select_shape_proto / select_shape_std
// 모두 무상태(static) 함수 — 입력은 const 참조, 내부는 지역변수만 사용해 재진입 안전.
// 좌표계: 입출력 위치 인자는 Halcon 관례 (row, col).

#include <string>
#include <vector>
#include "Domain/Region.h"

namespace Grf {

class RegionRelation {
public:
	// find_neighbors: 각 regions1[i] 에 대해 경계 최소거리 <= maxDistance 인 regions2 인덱스 목록.
	//  maxDistance <= 0 이면 픽셀이 겹치는(교집합>0) Region 만 이웃으로 본다.
	static std::vector<std::vector<int> > FindNeighbors(
		const std::vector<Region>& regions1,
		const std::vector<Region>& regions2,
		double maxDistance);

	// hamming_distance: 두 Region 의 대칭차 픽셀 수(Distance) 와 유사도(Similarity).
	//  Distance   = |R1 Δ R2| = A1 + A2 - 2·|R1∩R2|
	//  Similarity = 1 - Distance / (A1 + A2)   (동일=1, 서로소=0)
	static void HammingDistance(const Region& r1, const Region& r2,
		long long& distanceOut, double& similarityOut);

	// hamming_distance_norm: 정규화 버전.
	//  DistanceNorm = Distance / normFactor  (normFactor<=0 이면 Distance 그대로)
	//  Similarity 는 hamming_distance 와 동일.
	static void HammingDistanceNorm(const Region& r1, const Region& r2,
		double normFactor, double& distanceNormOut, double& similarityOut);

	// get_region_index: (row,col) 픽셀을 포함하는 모든 Region 인덱스.
	static std::vector<int> GetRegionIndex(
		const std::vector<Region>& regions, int row, int col);

	// select_region_point: 픽셀 (row,col) 을 포함하는 Region 인덱스 선택(get_region_index 와 동일 결과).
	static std::vector<int> SelectRegionPoint(
		const std::vector<Region>& regions, int row, int col);

	// 방향 관계
	enum SpatialDir {
		SPATIAL_LEFT_OF = 0, // reference 보다 왼쪽(col 작음)
		SPATIAL_RIGHT_OF,    // 오른쪽(col 큼)
		SPATIAL_ABOVE,       // 위(row 작음)
		SPATIAL_BELOW        // 아래(row 큼)
	};

	// select_region_spatial: reference Region 대비 무게중심 방향 관계로 regions 선택.
	static std::vector<int> SelectRegionSpatial(
		const std::vector<Region>& regions, const Region& reference,
		SpatialDir dir);

	// spatial_relation: 두 Region 의 좌표축(바운딩박스) 기준 관계 문자열 집합.
	//  가능 값: "left","right","above","below","over"(겹침).
	//  percent(0~100): 경계 판정 완화 비율(바운딩박스 폭/높이 대비).
	static std::vector<std::string> SpatialRelation(
		const Region& r1, const Region& r2, double percent);

	// select_shape_proto 의 관계 feature
	enum ProtoFeature {
		PROTO_DISTANCE_CENTER = 0, // 무게중심 간 유클리드 거리
		PROTO_DISTANCE_CONTOUR,    // 컨투어 최소거리(겹치면 0)
		PROTO_OVERLAP,             // |R∩P| / |R|  (0~1)
		PROTO_OVERLAP_ABS          // |R∩P|        (픽셀 수)
	};

	// select_shape_proto: prototype 과의 관계 feature 값이 [minVal,maxVal] 인 Region 선택.
	static std::vector<int> SelectShapeProto(
		const std::vector<Region>& regions, const Region& prototype,
		ProtoFeature feature, double minVal, double maxVal);

	// select_shape_std 의 표준 형상
	enum StdShape {
		STD_MAX_AREA = 0, // 최대 면적(percent 허용오차 내 다중 선택 가능)
		STD_RECTANGLE1,   // 축평행 바운딩박스 충실도(area/bboxArea)
		STD_RECTANGLE2    // 최소회전사각형 충실도(area/minAreaRectArea)
	};

	// select_shape_std: 표준 형상에 percent 허용오차로 부합하는 Region 선택.
	//  rectangle1/2: 충실도 >= 1 - percent/100 인 것.
	//  max_area   : 면적 >= maxArea·(1 - percent/100) 인 것.
	static std::vector<int> SelectShapeStd(
		const std::vector<Region>& regions, StdShape shape, double percent);

private:
	static bool RegionContainsPoint(const Region& region, int row, int col);
	static long long IntersectionArea(const Region& r1, const Region& r2);
	static double RegionMinDistance(const Region& r1, const Region& r2);
	static double BBoxGap(const cv::Rect& a, const cv::Rect& b); // 축분리 최소 간극(겹치면 0)
};

} // namespace Grf
