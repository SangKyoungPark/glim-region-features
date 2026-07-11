// Region.cpp
#include "Domain/Region.h"
#include <opencv2/imgproc.hpp> // cv::contourArea

namespace Grf {

Region::Region()
	: m_area(0)
	, m_bbox(0, 0, 0, 0)
	, m_bboxInit(false)
{
}

void Region::UpdateBBoxWithRun(const Run& run)
{
	// OpenCV Rect 는 (x=col, y=row, width, height)
	if (!m_bboxInit)
	{
		m_bbox = cv::Rect(run.m_colStart, run.m_row,
			run.m_colEnd - run.m_colStart + 1, 1);
		m_bboxInit = true;
		return;
	}

	int left = m_bbox.x;
	int top = m_bbox.y;
	int right = m_bbox.x + m_bbox.width - 1;
	int bottom = m_bbox.y + m_bbox.height - 1;

	if (run.m_colStart < left)  left = run.m_colStart;
	if (run.m_colEnd > right)   right = run.m_colEnd;
	if (run.m_row < top)        top = run.m_row;
	if (run.m_row > bottom)     bottom = run.m_row;

	m_bbox = cv::Rect(left, top, right - left + 1, bottom - top + 1);
}

void Region::AddRun(int row, int colStart, int colEnd)
{
	if (colEnd < colStart)
		return;

	Run run(row, colStart, colEnd);
	m_runs.push_back(run);
	m_area += run.Length();
	UpdateBBoxWithRun(run);
}

void Region::SetContours(const std::vector<std::vector<cv::Point> >& contours,
	const std::vector<cv::Vec4i>& hierarchy)
{
	m_contours = contours;
	m_hierarchy = hierarchy;
	m_outerContour.clear();

	// 최상위(부모 없음) 컨투어 중 최대 면적을 외곽으로 선택.
	double bestArea = -1.0;
	int bestIdx = -1;
	for (size_t i = 0; i < m_contours.size(); ++i)
	{
		bool isTopLevel = true;
		if (i < m_hierarchy.size())
			isTopLevel = (m_hierarchy[i][3] == -1); // parent == -1

		if (!isTopLevel)
			continue;

		double a = 0.0;
		try
		{
			a = cv::contourArea(m_contours[i]);
		}
		catch (const cv::Exception&)
		{
			a = 0.0;
		}

		if (a > bestArea)
		{
			bestArea = a;
			bestIdx = static_cast<int>(i);
		}
	}

	// 최상위가 하나도 없으면(계층 정보 부재) 전체 중 최대 면적 사용.
	if (bestIdx < 0)
	{
		for (size_t i = 0; i < m_contours.size(); ++i)
		{
			double a = 0.0;
			try { a = cv::contourArea(m_contours[i]); }
			catch (const cv::Exception&) { a = 0.0; }
			if (a > bestArea)
			{
				bestArea = a;
				bestIdx = static_cast<int>(i);
			}
		}
	}

	if (bestIdx >= 0)
		m_outerContour = m_contours[bestIdx];
}

int Region::HoleCount() const
{
	int holes = 0;
	for (size_t i = 0; i < m_contours.size(); ++i)
	{
		if (i < m_hierarchy.size() && m_hierarchy[i][3] != -1)
			holes++; // 부모가 있는 컨투어 = 구멍
	}
	return holes;
}

RawMoments Region::ComputeRawMoments() const
{
	RawMoments rm;

	// Σc, Σc^2 를 닫힌 형식으로 누적한다.
	//  f(k) = Σ_{c=0}^{k} c^2 = k(k+1)(2k+1)/6  (k >= 0)
	for (size_t i = 0; i < m_runs.size(); ++i)
	{
		const Run& run = m_runs[i];
		const double r = static_cast<double>(run.m_row);       // y
		const int c0 = run.m_colStart;
		const int c1 = run.m_colEnd;                           // x 범위
		const double n = static_cast<double>(c1 - c0 + 1);

		// Σc (등차수열 합)
		const double sumC = (static_cast<double>(c0) + static_cast<double>(c1)) * n * 0.5;

		// Σc^2 = f(c1) - f(c0-1)
		double f1 = 0.0;
		double f0 = 0.0;
		{
			const double k1 = static_cast<double>(c1);
			f1 = k1 * (k1 + 1.0) * (2.0 * k1 + 1.0) / 6.0;
		}
		if (c0 - 1 >= 0)
		{
			const double k0 = static_cast<double>(c0 - 1);
			f0 = k0 * (k0 + 1.0) * (2.0 * k0 + 1.0) / 6.0;
		}
		const double sumC2 = f1 - f0;

		rm.m00 += n;
		rm.m10 += sumC;          // Σx
		rm.m01 += r * n;         // Σy
		rm.m20 += sumC2;         // Σx^2
		rm.m02 += r * r * n;     // Σy^2
		rm.m11 += r * sumC;      // Σxy
	}

	return rm;
}

cv::Point2d Region::Centroid() const
{
	RawMoments rm = ComputeRawMoments();
	if (rm.m00 <= 0.0)
		return cv::Point2d(0.0, 0.0);
	return cv::Point2d(rm.m10 / rm.m00, rm.m01 / rm.m00); // (x=col, y=row)
}

cv::Mat Region::ToMask(cv::Point& offsetOut) const
{
	offsetOut = cv::Point(m_bbox.x, m_bbox.y);

	if (m_bbox.width <= 0 || m_bbox.height <= 0)
		return cv::Mat();

	cv::Mat mask = cv::Mat::zeros(m_bbox.height, m_bbox.width, CV_8UC1);
	for (size_t i = 0; i < m_runs.size(); ++i)
	{
		const Run& run = m_runs[i];
		const int y = run.m_row - m_bbox.y;
		const int x0 = run.m_colStart - m_bbox.x;
		const int x1 = run.m_colEnd - m_bbox.x;
		if (y < 0 || y >= mask.rows)
			continue;
		unsigned char* p = mask.ptr<unsigned char>(y);
		for (int x = x0; x <= x1; ++x)
		{
			if (x >= 0 && x < mask.cols)
				p[x] = 255;
		}
	}
	return mask;
}

} // namespace Grf
