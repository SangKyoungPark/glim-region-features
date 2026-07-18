// RegionRelation.cpp
#include "UseCase/RegionRelation.h"
#include <opencv2/imgproc.hpp> // cv::minAreaRect
#include <cmath>
#include <limits>
#include <algorithm>

namespace Grf {

namespace {
	const double kEps = 1e-12;

	// 두 Rect 교집합
	cv::Rect IntersectRect(const cv::Rect& a, const cv::Rect& b)
	{
		const int x1 = std::max(a.x, b.x);
		const int y1 = std::max(a.y, b.y);
		const int x2 = std::min(a.x + a.width, b.x + b.width);
		const int y2 = std::min(a.y + a.height, b.y + b.height);
		if (x2 <= x1 || y2 <= y1)
			return cv::Rect(0, 0, 0, 0);
		return cv::Rect(x1, y1, x2 - x1, y2 - y1);
	}
}

// ------------------------------------------------------------------
// 내부 헬퍼
// ------------------------------------------------------------------

bool RegionRelation::RegionContainsPoint(const Region& region, int row, int col)
{
	// (row,col) 이 어떤 런 [colStart,colEnd] 안에 있는가. O(런수).
	const std::vector<Run>& runs = region.Runs();
	for (size_t i = 0; i < runs.size(); ++i)
	{
		const Run& r = runs[i];
		if (r.m_row == row && col >= r.m_colStart && col <= r.m_colEnd)
			return true;
	}
	return false;
}

long long RegionRelation::IntersectionArea(const Region& r1, const Region& r2)
{
	// 교집합은 두 바운딩박스가 겹치는 영역에만 존재 → 로컬 마스크 AND 로 안전 계수.
	try
	{
		const cv::Rect ov = IntersectRect(r1.BoundingBox(), r2.BoundingBox());
		if (ov.width <= 0 || ov.height <= 0)
			return 0;

		cv::Mat m1 = cv::Mat::zeros(ov.height, ov.width, CV_8UC1);
		cv::Mat m2 = cv::Mat::zeros(ov.height, ov.width, CV_8UC1);

		// r1 런을 overlap ROI 로컬 좌표로 래스터화
		const std::vector<Run>& runs1 = r1.Runs();
		for (size_t i = 0; i < runs1.size(); ++i)
		{
			const Run& r = runs1[i];
			const int y = r.m_row - ov.y;
			if (y < 0 || y >= ov.height) continue;
			const int x0 = std::max(r.m_colStart, ov.x) - ov.x;
			const int x1 = std::min(r.m_colEnd, ov.x + ov.width - 1) - ov.x;
			if (x1 < x0) continue;
			unsigned char* p = m1.ptr<unsigned char>(y);
			for (int x = x0; x <= x1; ++x) p[x] = 1;
		}
		const std::vector<Run>& runs2 = r2.Runs();
		for (size_t i = 0; i < runs2.size(); ++i)
		{
			const Run& r = runs2[i];
			const int y = r.m_row - ov.y;
			if (y < 0 || y >= ov.height) continue;
			const int x0 = std::max(r.m_colStart, ov.x) - ov.x;
			const int x1 = std::min(r.m_colEnd, ov.x + ov.width - 1) - ov.x;
			if (x1 < x0) continue;
			unsigned char* p = m2.ptr<unsigned char>(y);
			for (int x = x0; x <= x1; ++x) p[x] = 1;
		}

		cv::Mat both;
		cv::bitwise_and(m1, m2, both);
		return static_cast<long long>(cv::countNonZero(both));
	}
	catch (const cv::Exception&) {}
	catch (...) {}
	return 0;
}

double RegionRelation::BBoxGap(const cv::Rect& a, const cv::Rect& b)
{
	// 축분리 최소 간극(두 박스가 겹치면 0). 이웃 빠른 하한.
	double dx = 0.0, dy = 0.0;
	if (b.x > a.x + a.width - 1)       dx = b.x - (a.x + a.width - 1);
	else if (a.x > b.x + b.width - 1)  dx = a.x - (b.x + b.width - 1);
	if (b.y > a.y + a.height - 1)      dy = b.y - (a.y + a.height - 1);
	else if (a.y > b.y + b.height - 1) dy = a.y - (b.y + b.height - 1);
	return std::sqrt(dx * dx + dy * dy);
}

double RegionRelation::RegionMinDistance(const Region& r1, const Region& r2)
{
	// 겹치면 0, 아니면 외곽 컨투어 점 쌍 최소 유클리드 거리(껍질 점 수가 적어 충분).
	try
	{
		if (IntersectionArea(r1, r2) > 0)
			return 0.0;

		const std::vector<cv::Point>& c1 = r1.OuterContour();
		const std::vector<cv::Point>& c2 = r2.OuterContour();
		if (c1.empty() || c2.empty())
			return std::numeric_limits<double>::max();

		double best2 = std::numeric_limits<double>::max();
		for (size_t i = 0; i < c1.size(); ++i)
		{
			for (size_t j = 0; j < c2.size(); ++j)
			{
				const double dx = static_cast<double>(c1[i].x - c2[j].x);
				const double dy = static_cast<double>(c1[i].y - c2[j].y);
				const double d2 = dx * dx + dy * dy;
				if (d2 < best2) best2 = d2;
			}
		}
		return std::sqrt(best2);
	}
	catch (const cv::Exception&) {}
	catch (...) {}
	return std::numeric_limits<double>::max();
}

// ------------------------------------------------------------------
// 공개 오퍼레이터
// ------------------------------------------------------------------

std::vector<std::vector<int> > RegionRelation::FindNeighbors(
	const std::vector<Region>& regions1,
	const std::vector<Region>& regions2,
	double maxDistance)
{
	std::vector<std::vector<int> > result(regions1.size());
	try
	{
		for (size_t i = 0; i < regions1.size(); ++i)
		{
			const cv::Rect b1 = regions1[i].BoundingBox();
			for (size_t j = 0; j < regions2.size(); ++j)
			{
				// 바운딩박스 하한으로 빠른 배제
				const double gap = BBoxGap(b1, regions2[j].BoundingBox());
				if (maxDistance <= 0.0)
				{
					// 접촉/겹침만 이웃
					if (gap == 0.0 && IntersectionArea(regions1[i], regions2[j]) > 0)
						result[i].push_back(static_cast<int>(j));
					continue;
				}
				if (gap > maxDistance)
					continue;
				const double d = RegionMinDistance(regions1[i], regions2[j]);
				if (d <= maxDistance)
					result[i].push_back(static_cast<int>(j));
			}
		}
	}
	catch (...) {}
	return result;
}

void RegionRelation::HammingDistance(const Region& r1, const Region& r2,
	long long& distanceOut, double& similarityOut)
{
	distanceOut = 0;
	similarityOut = 0.0;
	try
	{
		const long long a1 = static_cast<long long>(r1.Area());
		const long long a2 = static_cast<long long>(r2.Area());
		const long long inter = IntersectionArea(r1, r2);
		long long dist = a1 + a2 - 2 * inter;
		if (dist < 0) dist = 0; // 수치 가드
		distanceOut = dist;
		const long long sum = a1 + a2;
		if (sum > 0)
			similarityOut = 1.0 - static_cast<double>(dist) / static_cast<double>(sum);
		else
			similarityOut = 1.0; // 둘 다 빈 Region → 동일 취급
	}
	catch (...) {}
}

void RegionRelation::HammingDistanceNorm(const Region& r1, const Region& r2,
	double normFactor, double& distanceNormOut, double& similarityOut)
{
	distanceNormOut = 0.0;
	similarityOut = 0.0;
	long long dist = 0;
	double sim = 0.0;
	HammingDistance(r1, r2, dist, sim);
	similarityOut = sim;
	if (normFactor > kEps)
		distanceNormOut = static_cast<double>(dist) / normFactor;
	else
		distanceNormOut = static_cast<double>(dist);
}

std::vector<int> RegionRelation::GetRegionIndex(
	const std::vector<Region>& regions, int row, int col)
{
	std::vector<int> idx;
	try
	{
		for (size_t i = 0; i < regions.size(); ++i)
		{
			// 바운딩박스 빠른 배제 후 런 검사
			const cv::Rect b = regions[i].BoundingBox();
			if (col < b.x || col > b.x + b.width - 1 || row < b.y || row > b.y + b.height - 1)
				continue;
			if (RegionContainsPoint(regions[i], row, col))
				idx.push_back(static_cast<int>(i));
		}
	}
	catch (...) {}
	return idx;
}

std::vector<int> RegionRelation::SelectRegionPoint(
	const std::vector<Region>& regions, int row, int col)
{
	// 픽셀을 포함하는 Region 선택 → get_region_index 와 동일 인덱스 집합.
	return GetRegionIndex(regions, row, col);
}

std::vector<int> RegionRelation::SelectRegionSpatial(
	const std::vector<Region>& regions, const Region& reference,
	SpatialDir dir)
{
	std::vector<int> idx;
	try
	{
		const cv::Point2d rc = reference.Centroid(); // (x=col, y=row)
		for (size_t i = 0; i < regions.size(); ++i)
		{
			const cv::Point2d c = regions[i].Centroid();
			bool ok = false;
			switch (dir)
			{
			case SPATIAL_LEFT_OF:  ok = (c.x < rc.x); break;
			case SPATIAL_RIGHT_OF: ok = (c.x > rc.x); break;
			case SPATIAL_ABOVE:    ok = (c.y < rc.y); break;
			case SPATIAL_BELOW:    ok = (c.y > rc.y); break;
			default: ok = false; break;
			}
			if (ok)
				idx.push_back(static_cast<int>(i));
		}
	}
	catch (...) {}
	return idx;
}

std::vector<std::string> RegionRelation::SpatialRelation(
	const Region& r1, const Region& r2, double percent)
{
	std::vector<std::string> rels;
	try
	{
		const cv::Rect a = r1.BoundingBox();
		const cv::Rect b = r2.BoundingBox();
		const int aL = a.x, aR = a.x + a.width - 1, aT = a.y, aB = a.y + a.height - 1;
		const int bL = b.x, bR = b.x + b.width - 1, bT = b.y, bB = b.y + b.height - 1;

		// percent 완화: 각 박스 폭/높이 대비 허용치.
		const double p = (percent < 0.0 ? 0.0 : percent) / 100.0;
		const double tolX = p * 0.5 * (a.width + b.width);
		const double tolY = p * 0.5 * (a.height + b.height);

		bool over = true;
		if (aR < bL - tolX) { rels.push_back("left");  over = false; }
		if (aL > bR + tolX) { rels.push_back("right"); over = false; }
		if (aB < bT - tolY) { rels.push_back("above"); over = false; }
		if (aT > bB + tolY) { rels.push_back("below"); over = false; }
		if (over)
			rels.push_back("over");
	}
	catch (...) {}
	return rels;
}

std::vector<int> RegionRelation::SelectShapeProto(
	const std::vector<Region>& regions, const Region& prototype,
	ProtoFeature feature, double minVal, double maxVal)
{
	std::vector<int> idx;
	try
	{
		const cv::Point2d pc = prototype.Centroid();
		const long long pArea = static_cast<long long>(prototype.Area());
		(void)pArea;

		for (size_t i = 0; i < regions.size(); ++i)
		{
			double v = 0.0;
			switch (feature)
			{
			case PROTO_DISTANCE_CENTER:
			{
				const cv::Point2d c = regions[i].Centroid();
				const double dx = c.x - pc.x, dy = c.y - pc.y;
				v = std::sqrt(dx * dx + dy * dy);
				break;
			}
			case PROTO_DISTANCE_CONTOUR:
				v = RegionMinDistance(regions[i], prototype);
				break;
			case PROTO_OVERLAP:
			{
				const long long inter = IntersectionArea(regions[i], prototype);
				const long long a = static_cast<long long>(regions[i].Area());
				v = (a > 0) ? static_cast<double>(inter) / static_cast<double>(a) : 0.0;
				break;
			}
			case PROTO_OVERLAP_ABS:
				v = static_cast<double>(IntersectionArea(regions[i], prototype));
				break;
			default:
				v = 0.0; break;
			}
			if (v >= minVal && v <= maxVal)
				idx.push_back(static_cast<int>(i));
		}
	}
	catch (...) {}
	return idx;
}

std::vector<int> RegionRelation::SelectShapeStd(
	const std::vector<Region>& regions, StdShape shape, double percent)
{
	std::vector<int> idx;
	try
	{
		const double p = (percent < 0.0 ? 0.0 : percent) / 100.0;

		if (shape == STD_MAX_AREA)
		{
			long long maxA = 0;
			for (size_t i = 0; i < regions.size(); ++i)
				maxA = std::max(maxA, static_cast<long long>(regions[i].Area()));
			if (maxA <= 0)
				return idx;
			const double thr = static_cast<double>(maxA) * (1.0 - p);
			for (size_t i = 0; i < regions.size(); ++i)
				if (static_cast<double>(regions[i].Area()) >= thr)
					idx.push_back(static_cast<int>(i));
			return idx;
		}

		const double fillThr = 1.0 - p; // 충실도 하한
		for (size_t i = 0; i < regions.size(); ++i)
		{
			const double area = static_cast<double>(regions[i].Area());
			if (area <= 0.0)
				continue;

			double denom = 0.0;
			if (shape == STD_RECTANGLE1)
			{
				const cv::Rect b = regions[i].BoundingBox();
				denom = static_cast<double>(b.width) * static_cast<double>(b.height);
			}
			else // STD_RECTANGLE2
			{
				const std::vector<cv::Point>& outer = regions[i].OuterContour();
				if (outer.size() < 3)
					continue;
				cv::RotatedRect rr = cv::minAreaRect(outer);
				denom = static_cast<double>(rr.size.width) * static_cast<double>(rr.size.height);
			}

			if (denom <= kEps)
				continue;
			double fill = area / denom;
			if (fill > 1.0) fill = 1.0;
			if (fill >= fillThr)
				idx.push_back(static_cast<int>(i));
		}
	}
	catch (const cv::Exception&) {}
	catch (...) {}
	return idx;
}

} // namespace Grf
