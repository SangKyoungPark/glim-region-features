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

	// --- (Phase 2) inner_rectangle1 : 최대 축평행 내접 사각형 (row,col, inclusive) ---
	double innerRectRow1, innerRectCol1, innerRectRow2, innerRectCol2;
	double innerRectFillRatio; // 내접사각형 면적 / 바운딩박스 면적 (0~1, 사각형=1)

	// --- (Phase 2) runlength_features (행 단위 런 통계) ---
	int    numRuns;        // 런 개수(행 단위 연속 전경 구간 수)
	double kFactor;        // NumRuns / sqrt(Area) — 줄무늬/복잡도 계수
	double lFactor;        // NumRuns / 바운딩박스 높이(Row2-Row1+1)
	double meanRunLength;  // Area / NumRuns — 평균 런 길이

	// --- 파생 Score 보조값 ---
	double aspectRatio;    // Rect2 장/단변 비
	double fillRatio;      // A / Rect2 면적
	double innerOuterRatio;// inner_circle.r / smallest_circle.r

	// --- (Phase 3) get_region_thickness (주축 방향 두께 프로파일 요약) ---
	double thicknessMean;  // 주축 방향 두께 평균
	double thicknessMax;   // 주축 방향 두께 최대
	double thicknessLength;// 주축 방향 길이(프로파일 구간 수)

	// --- (Phase 3) runlength_distribution (런 길이 분포 요약) ---
	int runLenMin;   // 최소 런 길이
	int runLenMax;   // 최대 런 길이
	int runLenMode;  // 최빈 런 길이

	// --- (Phase 3) moments_region_2nd (정규화 중심 2차 모멘트, Halcon row/col) ---
	double m2ndM20;  // (1/A)Σ(r-rc)^2  (row 분산)
	double m2ndM02;  // (1/A)Σ(c-cc)^2  (col 분산)
	double m2ndM11;  // (1/A)Σ(r-rc)(c-cc)
	double m2ndIa;   // 주축 관성 최대 고유값
	double m2ndIb;   // 주축 관성 최소 고유값

	// --- (Phase 3) moments_region_central (비정규화 중심 2차 모멘트) ---
	double mcMu20;   // Σ(r-rc)^2
	double mcMu02;   // Σ(c-cc)^2
	double mcMu11;   // Σ(r-rc)(c-cc)

	// --- (Phase 3) moments_region_3rd (정규화 중심 3차 모멘트, /A) ---
	double m3rdM30;  // (1/A)Σ(r-rc)^3
	double m3rdM03;  // (1/A)Σ(c-cc)^3
	double m3rdM21;  // (1/A)Σ(r-rc)^2(c-cc)
	double m3rdM12;  // (1/A)Σ(r-rc)(c-cc)^2

	// --- (Phase 3) moments_region_2nd_rel_invar (회전 불변 PHI) ---
	double momentPhi1; // η20+η02
	double momentPhi2; // (η20-η02)^2 + 4η11^2

	// --- (Phase 3) moments_region_central_invar (스케일 불변 PSI) ---
	double momentPsi1; // η20
	double momentPsi2; // η11
	double momentPsi3; // η02
	double momentPsi4; // η20·η02 − η11^2

	FeatureVector();

	// 이름으로 특징값 조회(스코어/룰 엔진용). 미지원 이름은 0.0 반환.
	double GetByName(const std::string& name) const;

	// CSV 덤프
	static std::string CsvHeader();
	std::string ToCsvRow() const;
};

} // namespace Grf
