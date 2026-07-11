#pragma once
// FeatureVector.h
// 하나의 Region 에서 계산된 모든 원시 특징값을 담는 구조체.
// 이름 기반 조회(GetByName)로 Score 정규화/룰 엔진과 연결한다.
// CSV 덤프(Header/Row)로 데이터 수집·파라미터 튜닝을 지원한다.
// 좌표계: 위치성 값(center/bbox/circle 등)은 Halcon 관례인 (row, col) 로 보관한다.

#include <string>

namespace Grf {

struct FeatureVector {
	// --- area_center ---
	double area;        // 픽셀 수
	double centerRow;   // r̄
	double centerCol;   // c̄

	// --- area_holes ---
	double areaHoles;

	// --- contlength ---
	double contLength;

	// --- diameter_region ---
	double diameter;
	double diaRow1, diaCol1, diaRow2, diaCol2; // 최대거리 두 점(row,col)

	// --- 형상 계수 ---
	double circularity;
	double compactness;
	double convexity;
	double rectangularity;

	// --- roundness (컨투어 기반) ---
	double roundnessDistance; // 중심-컨투어 평균 거리
	double roundnessSigma;    // 표준편차
	double roundness;         // 1 - Sigma/Distance
	double roundnessSides;    // 변 수 추정

	// --- elliptic_axis ---
	double ra;   // 장반경(반장축)
	double rb;   // 단반경(반단축)
	double phi;  // 라디안, +x(col) 축 기준

	// --- eccentricity ---
	double anisometry;      // Ra/Rb
	double bulkiness;       // π·Ra·Rb/A
	double structureFactor; // Anisometry·Bulkiness - 1

	// --- orientation_region ---
	double orientation; // 라디안 (-π, π]

	// --- (우선순위 2) smallest_rectangle1 : 축평행 바운딩박스 ---
	double bboxRow1, bboxCol1, bboxRow2, bboxCol2;

	// --- (우선순위 2) smallest_rectangle2 : 최소면적 회전 사각형 ---
	double rect2CenterRow, rect2CenterCol;
	double rect2Len1, rect2Len2; // 반변 길이(장/단)
	double rect2Phi;             // 라디안

	// --- (우선순위 2) smallest_circle / inner_circle ---
	double smallestCircleRow, smallestCircleCol, smallestCircleRadius;
	double innerCircleRow, innerCircleCol, innerCircleRadius;

	// --- (우선순위 2) 위상 ---
	int connectComponents; // 연결 성분 수(Region 단위이므로 보통 1)
	int holes;             // 구멍 수
	int eulerNumber;       // connect - holes

	// --- (우선순위 2) Hu 모멘트(크기·회전 불변) ---
	double hu[7];

	// --- 파생 Score 보조값 ---
	double aspectRatio;    // Rect2 장/단변 비
	double fillRatio;      // A / Rect2 면적
	double innerOuterRatio;// inner_circle.r / smallest_circle.r

	FeatureVector();

	// 이름으로 특징값 조회(스코어/룰 엔진용). 미지원 이름은 0.0 반환.
	double GetByName(const std::string& name) const;

	// CSV 덤프
	static std::string CsvHeader();
	std::string ToCsvRow() const;
};

} // namespace Grf
