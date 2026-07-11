#pragma once
// Region.h
// 이진화 이미지에서 추출한 하나의 연결 성분(Region) 도메인 모델.
// 내부 표현: 런렝스(run-length) + 외곽/구멍 컨투어 + 픽셀 수(면적).
// 좌표계 주의: Halcon 은 (row, col), OpenCV 는 (x=col, y=row).
//  - Run 은 (row, colStart, colEnd) 로 Halcon 관례에 가깝게 보관.
//  - cv::Point 컨투어 점은 OpenCV 관례 그대로 (x=col, y=row).
//  - RawMoments 는 x=col, y=row 축으로 계산(수식 문서와 일치하도록 주석 명시).

#include <vector>
#include <opencv2/core.hpp>

namespace Grf {

// 한 행(row)에서 연속된 전경 픽셀 구간 [colStart, colEnd] (양끝 포함)
struct Run {
	int m_row;
	int m_colStart;
	int m_colEnd;

	Run() : m_row(0), m_colStart(0), m_colEnd(0) {}
	Run(int row, int colStart, int colEnd)
		: m_row(row), m_colStart(colStart), m_colEnd(colEnd) {}

	int Length() const { return m_colEnd - m_colStart + 1; }
};

// 원점(0,0) 기준 픽셀 모멘트. 축은 x=col, y=row.
//  m00 = 면적, m10 = Σx, m01 = Σy, m20 = Σx^2, m02 = Σy^2, m11 = Σxy
struct RawMoments {
	double m00;
	double m10;
	double m01;
	double m20;
	double m02;
	double m11;

	RawMoments() : m00(0.0), m10(0.0), m01(0.0), m20(0.0), m02(0.0), m11(0.0) {}
};

class Region {
public:
	Region();

	// --- 구성(Adapter 가 채운다) ---
	void AddRun(int row, int colStart, int colEnd);
	void SetContours(const std::vector<std::vector<cv::Point> >& contours,
		const std::vector<cv::Vec4i>& hierarchy);

	// --- 조회 ---
	const std::vector<Run>& Runs() const { return m_runs; }
	const std::vector<cv::Point>& OuterContour() const { return m_outerContour; }
	const std::vector<std::vector<cv::Point> >& AllContours() const { return m_contours; }
	const std::vector<cv::Vec4i>& Hierarchy() const { return m_hierarchy; }

	int Area() const { return m_area; }             // 픽셀 수
	cv::Rect BoundingBox() const { return m_bbox; } // OpenCV (x=col, y=row)
	int HoleCount() const;                          // 내부(구멍) 컨투어 수

	// 런렝스로부터 원점 픽셀 모멘트 계산 (x=col, y=row). 0 나눗셈 가드는 호출부 책임.
	RawMoments ComputeRawMoments() const;

	// 무게중심 (x=col, y=row). 면적 0 이면 (0,0).
	cv::Point2d Centroid() const;

	// Region 을 담는 BoundingBox 크기의 이진 마스크(255) 생성. offsetOut = 좌상단(col,row).
	cv::Mat ToMask(cv::Point& offsetOut) const;

	bool IsEmpty() const { return m_area <= 0; }

private:
	void UpdateBBoxWithRun(const Run& run);

	std::vector<Run> m_runs;
	std::vector<std::vector<cv::Point> > m_contours;   // findContours 결과(외곽+구멍)
	std::vector<cv::Vec4i> m_hierarchy;
	std::vector<cv::Point> m_outerContour;             // 최대 면적 외곽 컨투어
	int m_area;
	cv::Rect m_bbox;
	bool m_bboxInit;
};

} // namespace Grf
