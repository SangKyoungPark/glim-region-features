// FeatureCalculator.cpp
#include "UseCase/FeatureCalculator.h"
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <limits>

namespace Grf {

namespace {
	const double kPi = 3.14159265358979323846;
	const double kEps = 1e-12;

	// (-pi, pi] 로 정규화
	double NormalizeAngle(double a)
	{
		while (a <= -kPi) a += 2.0 * kPi;
		while (a > kPi)   a -= 2.0 * kPi;
		return a;
	}
}

FeatureCalculator::FeatureCalculator()
	: m_computePriority2(true)
{
}

FeatureVector FeatureCalculator::Compute(const Region& region) const
{
	FeatureVector fv;

	try
	{
		if (region.IsEmpty())
			return fv;

		const RawMoments rm = region.ComputeRawMoments();

		// 우선순위 1
		ComputeAreaCenter(region, rm, fv);
		ComputeAreaHoles(region, fv);
		ComputeContLength(region, fv);
		ComputeDiameter(region, fv);
		ComputeShapeFactors(region, fv);
		ComputeRoundness(region, fv);
		ComputeEllipticAxis(region, rm, fv);
		ComputeEccentricity(fv);
		ComputeOrientation(region, fv);

		// 우선순위 2
		if (m_computePriority2)
		{
			ComputeBoundingRects(region, fv);
			ComputeCircles(region, fv);
			ComputeTopology(region, fv);
			ComputeHuMoments(region, fv);
			ComputeInnerRectangle(region, fv);
			ComputeRunlength(region, fv);
			ComputeDerived(fv);
		}
	}
	catch (const cv::Exception&)
	{
		// 계산 도중 예외 시 지금까지의 값으로 반환.
	}
	catch (...)
	{
	}

	return fv;
}

// ------------------------------------------------------------------
// 우선순위 1
// ------------------------------------------------------------------

void FeatureCalculator::ComputeAreaCenter(const Region& region, const RawMoments& rm, FeatureVector& fv) const
{
	fv.area = static_cast<double>(region.Area());
	if (rm.m00 > 0.0)
	{
		// x=col, y=row → Halcon (row, col)
		fv.centerCol = rm.m10 / rm.m00;
		fv.centerRow = rm.m01 / rm.m00;
	}
}

void FeatureCalculator::ComputeAreaHoles(const Region& region, FeatureVector& fv) const
{
	// fill_up(Region) 면적 - A. 외곽 컨투어를 FILLED 로 채운 면적 - 실제 픽셀 수.
	const std::vector<cv::Point>& outer = region.OuterContour();
	if (outer.size() < 3)
	{
		fv.areaHoles = 0.0;
		return;
	}

	cv::Rect box = region.BoundingBox();
	if (box.width <= 0 || box.height <= 0)
	{
		fv.areaHoles = 0.0;
		return;
	}

	// ROI 로컬 좌표로 외곽만 채운다.
	cv::Mat filled = cv::Mat::zeros(box.height, box.width, CV_8UC1);
	std::vector<std::vector<cv::Point> > polys(1);
	polys[0].reserve(outer.size());
	for (size_t i = 0; i < outer.size(); ++i)
		polys[0].push_back(cv::Point(outer[i].x - box.x, outer[i].y - box.y));

	cv::drawContours(filled, polys, 0, cv::Scalar(255), cv::FILLED);
	const int filledArea = cv::countNonZero(filled);

	double holes = static_cast<double>(filledArea) - fv.area;
	if (holes < 0.0)
		holes = 0.0; // 수치 오차 가드
	fv.areaHoles = holes;
}

void FeatureCalculator::ComputeContLength(const Region& region, FeatureVector& fv) const
{
	const std::vector<cv::Point>& outer = region.OuterContour();
	if (outer.size() < 2)
	{
		fv.contLength = 0.0;
		return;
	}
	// 닫힌 컨투어 둘레. 인접 픽셀 수평/수직=1, 대각=√2 로 자연히 반영됨.
	fv.contLength = cv::arcLength(outer, true);
}

void FeatureCalculator::ComputeDiameter(const Region& region, FeatureVector& fv) const
{
	const std::vector<cv::Point>& outer = region.OuterContour();
	if (outer.empty())
		return;

	// 볼록껍질 위에서 최대 거리 쌍 탐색(껍질 점 수가 적어 브루트포스면 충분).
	std::vector<cv::Point> hull;
	try
	{
		cv::convexHull(outer, hull);
	}
	catch (const cv::Exception&)
	{
		hull = outer;
	}
	if (hull.size() < 2)
		return;

	double maxD2 = -1.0;
	int bi = 0, bj = 1;
	for (size_t i = 0; i < hull.size(); ++i)
	{
		for (size_t j = i + 1; j < hull.size(); ++j)
		{
			const double dx = static_cast<double>(hull[i].x - hull[j].x);
			const double dy = static_cast<double>(hull[i].y - hull[j].y);
			const double d2 = dx * dx + dy * dy;
			if (d2 > maxD2)
			{
				maxD2 = d2;
				bi = static_cast<int>(i);
				bj = static_cast<int>(j);
			}
		}
	}

	fv.diameter = std::sqrt(maxD2 > 0.0 ? maxD2 : 0.0);
	// (row, col) 로 저장
	fv.diaCol1 = hull[bi].x; fv.diaRow1 = hull[bi].y;
	fv.diaCol2 = hull[bj].x; fv.diaRow2 = hull[bj].y;
}

void FeatureCalculator::ComputeShapeFactors(const Region& region, FeatureVector& fv) const
{
	const std::vector<cv::Point>& outer = region.OuterContour();
	const double area = fv.area;

	// circularity: A / (π·max²), max = 중심→컨투어 최대 거리
	if (!outer.empty() && area > 0.0)
	{
		const cv::Point2d c = region.Centroid();
		double maxD2 = 0.0;
		for (size_t i = 0; i < outer.size(); ++i)
		{
			const double dx = outer[i].x - c.x;
			const double dy = outer[i].y - c.y;
			const double d2 = dx * dx + dy * dy;
			if (d2 > maxD2) maxD2 = d2;
		}
		const double denom = kPi * maxD2;
		if (denom > kEps)
		{
			double circ = area / denom;
			if (circ > 1.0) circ = 1.0; // 원=1 상한
			fv.circularity = circ;
		}
	}

	// compactness: L² / (4πA)
	if (area > 0.0 && fv.contLength > 0.0)
	{
		fv.compactness = (fv.contLength * fv.contLength) / (4.0 * kPi * area);
	}

	// convexity: A / A_convex
	if (!outer.empty() && area > 0.0)
	{
		std::vector<cv::Point> hull;
		try { cv::convexHull(outer, hull); }
		catch (const cv::Exception&) { hull.clear(); }
		if (hull.size() >= 3)
		{
			double aConvex = 0.0;
			try { aConvex = cv::contourArea(hull); }
			catch (const cv::Exception&) { aConvex = 0.0; }
			if (aConvex > kEps)
			{
				double conv = area / aConvex;
				if (conv > 1.0) conv = 1.0;
				fv.convexity = conv;
			}
		}
	}

	// rectangularity: A / A_rect, A_rect = minAreaRect 면적
	if (!outer.empty() && area > 0.0 && outer.size() >= 3)
	{
		try
		{
			cv::RotatedRect rr = cv::minAreaRect(outer);
			const double aRect = static_cast<double>(rr.size.width) * static_cast<double>(rr.size.height);
			if (aRect > kEps)
			{
				double rect = area / aRect;
				if (rect > 1.0) rect = 1.0;
				fv.rectangularity = rect;
			}
		}
		catch (const cv::Exception&) {}
	}
}

void FeatureCalculator::ComputeRoundness(const Region& region, FeatureVector& fv) const
{
	const std::vector<cv::Point>& outer = region.OuterContour();
	if (outer.empty())
		return;

	const cv::Point2d c = region.Centroid();

	// Distance = mean(d_i)
	double sum = 0.0;
	const size_t n = outer.size();
	std::vector<double> d(n);
	for (size_t i = 0; i < n; ++i)
	{
		const double dx = outer[i].x - c.x;
		const double dy = outer[i].y - c.y;
		d[i] = std::sqrt(dx * dx + dy * dy);
		sum += d[i];
	}
	const double distance = sum / static_cast<double>(n);
	fv.roundnessDistance = distance;

	// Sigma = sqrt(mean((d_i - Distance)²))
	double var = 0.0;
	for (size_t i = 0; i < n; ++i)
	{
		const double e = d[i] - distance;
		var += e * e;
	}
	var /= static_cast<double>(n);
	const double sigma = std::sqrt(var);
	fv.roundnessSigma = sigma;

	// Roundness = 1 - Sigma/Distance
	if (distance > kEps)
	{
		double roundness = 1.0 - sigma / distance;
		if (roundness < 0.0) roundness = 0.0;
		fv.roundness = roundness;

		// Sides = 1.4111·(Distance/Sigma)^0.4724
		if (sigma > kEps)
			fv.roundnessSides = 1.4111 * std::pow(distance / sigma, 0.4724);
		else
			fv.roundnessSides = 0.0; // 완전한 원(Sigma≈0) → 변 수 무한대 개념. 0 으로 표기.
	}
}

void FeatureCalculator::ComputeEllipticAxis(const Region& region, const RawMoments& rm, FeatureVector& fv) const
{
	if (rm.m00 <= 0.0)
		return;

	// 정규화 중심 2차 모멘트 (x=col, y=row)
	const double xbar = rm.m10 / rm.m00;
	const double ybar = rm.m01 / rm.m00;
	const double mu20 = rm.m20 / rm.m00 - xbar * xbar; // x(col) 분산
	const double mu02 = rm.m02 / rm.m00 - ybar * ybar; // y(row) 분산
	const double mu11 = rm.m11 / rm.m00 - xbar * ybar; // 공분산

	const double diff = mu20 - mu02;
	const double common = std::sqrt(diff * diff + 4.0 * mu11 * mu11);

	// 스펙 3장 수식: 반장축/반단축. (솔리드 타원의 반축과 일치)
	double raSq = 2.0 * (mu20 + mu02 + common);
	double rbSq = 2.0 * (mu20 + mu02 - common);
	if (raSq < 0.0) raSq = 0.0;
	if (rbSq < 0.0) rbSq = 0.0;
	fv.ra = std::sqrt(raSq);
	fv.rb = std::sqrt(rbSq);

	// Phi = 0.5·atan2(2μ11, μ20-μ02). +x(col) 축 기준, y-down 프레임.
	// (이 좌표계에서 OpenCV 로 그린 타원의 회전각과 일치함을 해석적으로 검증)
	fv.phi = 0.5 * std::atan2(2.0 * mu11, diff);
}

void FeatureCalculator::ComputeEccentricity(FeatureVector& fv) const
{
	if (fv.rb > kEps)
		fv.anisometry = fv.ra / fv.rb;
	else
		fv.anisometry = 0.0;

	if (fv.area > kEps)
		fv.bulkiness = kPi * fv.ra * fv.rb / fv.area;
	else
		fv.bulkiness = 0.0;

	fv.structureFactor = fv.anisometry * fv.bulkiness - 1.0;
}

void FeatureCalculator::ComputeOrientation(const Region& region, FeatureVector& fv) const
{
	const std::vector<cv::Point>& outer = region.OuterContour();
	if (outer.empty())
	{
		fv.orientation = NormalizeAngle(fv.phi);
		return;
	}

	const cv::Point2d c = region.Centroid();

	// 중심에서 가장 먼 컨투어 점 방향으로 부호/방향 결정.
	double maxD2 = -1.0;
	double fx = 0.0, fy = 0.0;
	for (size_t i = 0; i < outer.size(); ++i)
	{
		const double dx = outer[i].x - c.x;
		const double dy = outer[i].y - c.y;
		const double d2 = dx * dx + dy * dy;
		if (d2 > maxD2)
		{
			maxD2 = d2;
			fx = dx; fy = dy;
		}
	}

	const double farAngle = std::atan2(fy, fx);
	double orient = fv.phi;
	// phi 는 축 방향(±π 모호). 가장 먼 점 방향과 예각이 되도록 π 보정.
	if (std::cos(farAngle - orient) < 0.0)
		orient += kPi;

	fv.orientation = NormalizeAngle(orient);
}

// ------------------------------------------------------------------
// 우선순위 2
// ------------------------------------------------------------------

void FeatureCalculator::ComputeBoundingRects(const Region& region, FeatureVector& fv) const
{
	// smallest_rectangle1: 축평행 바운딩박스 (row1,col1,row2,col2)
	cv::Rect box = region.BoundingBox();
	fv.bboxRow1 = box.y;
	fv.bboxCol1 = box.x;
	fv.bboxRow2 = box.y + box.height - 1;
	fv.bboxCol2 = box.x + box.width - 1;

	// smallest_rectangle2: 최소면적 회전 사각형
	const std::vector<cv::Point>& outer = region.OuterContour();
	if (outer.size() >= 3)
	{
		try
		{
			cv::RotatedRect rr = cv::minAreaRect(outer);
			fv.rect2CenterCol = rr.center.x;
			fv.rect2CenterRow = rr.center.y;
			double w = rr.size.width;
			double h = rr.size.height;
			// Len1=장반변, Len2=단반변 (반길이)
			double lenMajor = (w > h ? w : h) * 0.5;
			double lenMinor = (w > h ? h : w) * 0.5;
			fv.rect2Len1 = lenMajor;
			fv.rect2Len2 = lenMinor;
			fv.rect2Phi = rr.angle * kPi / 180.0; // deg → rad
		}
		catch (const cv::Exception&) {}
	}
}

void FeatureCalculator::ComputeCircles(const Region& region, FeatureVector& fv) const
{
	const std::vector<cv::Point>& outer = region.OuterContour();

	// smallest_circle: 최소 외접원
	if (!outer.empty())
	{
		try
		{
			cv::Point2f center;
			float radius = 0.0f;
			cv::minEnclosingCircle(outer, center, radius);
			fv.smallestCircleCol = center.x;
			fv.smallestCircleRow = center.y;
			fv.smallestCircleRadius = radius;
		}
		catch (const cv::Exception&) {}
	}

	// inner_circle: 최대 내접원 (거리변환 최대값)
	try
	{
		cv::Point offset;
		cv::Mat mask = region.ToMask(offset);
		if (!mask.empty())
		{
			cv::Mat dist;
			cv::distanceTransform(mask, dist, cv::DIST_L2, 3);
			double minVal = 0.0, maxVal = 0.0;
			cv::Point minLoc, maxLoc;
			cv::minMaxLoc(dist, &minVal, &maxVal, &minLoc, &maxLoc);
			fv.innerCircleRadius = maxVal;
			fv.innerCircleCol = offset.x + maxLoc.x;
			fv.innerCircleRow = offset.y + maxLoc.y;
		}
	}
	catch (const cv::Exception&) {}
}

void FeatureCalculator::ComputeTopology(const Region& region, FeatureVector& fv) const
{
	// Region 은 단일 연결 성분으로 추출되므로 connect=1.
	fv.connectComponents = 1;
	fv.holes = region.HoleCount();
	fv.eulerNumber = fv.connectComponents - fv.holes;
}

void FeatureCalculator::ComputeHuMoments(const Region& region, FeatureVector& fv) const
{
	try
	{
		cv::Point offset;
		cv::Mat mask = region.ToMask(offset);
		if (mask.empty())
			return;
		cv::Moments mm = cv::moments(mask, true); // 이진 픽셀 모멘트
		double hu[7] = { 0 };
		cv::HuMoments(mm, hu);
		for (int i = 0; i < 7; ++i)
			fv.hu[i] = hu[i];
	}
	catch (const cv::Exception&) {}
}

void FeatureCalculator::ComputeInnerRectangle(const Region& region, FeatureVector& fv) const
{
	// inner_rectangle1: 최대 축평행 내접 사각형(모두 전경 픽셀). Halcon (row1,col1,row2,col2).
	// 히스토그램 스택 방식 O(W·H): 각 행마다 열별 높이를 갱신하고, 히스토그램 최대 직사각형을 구한다.
	try
	{
		cv::Point offset;
		cv::Mat mask = region.ToMask(offset); // 로컬 ROI(전경=255), offset=좌상단(col,row)
		if (mask.empty())
			return;

		const int H = mask.rows;
		const int W = mask.cols;
		if (H <= 0 || W <= 0)
			return;

		// heights[c] = 현재 행에서 위로 연속된 전경 픽셀 수(로컬 좌표)
		std::vector<int> heights(W, 0);
		std::vector<int> stackIdx;       // 인덱스 스택(높이 오름차순 유지)
		stackIdx.reserve(W + 1);

		long long bestArea = 0;
		int bestTop = 0, bestLeft = 0, bestBottom = -1, bestRight = -1; // 로컬 inclusive

		for (int r = 0; r < H; ++r)
		{
			const unsigned char* p = mask.ptr<unsigned char>(r);
			for (int c = 0; c < W; ++c)
				heights[c] = (p[c] != 0) ? (heights[c] + 1) : 0;

			// 히스토그램 최대 직사각형(위치 추적). i==W 는 높이 0 센티널.
			stackIdx.clear();
			for (int i = 0; i <= W; ++i)
			{
				const int curH = (i < W) ? heights[i] : 0;
				while (!stackIdx.empty() && heights[stackIdx.back()] >= curH)
				{
					const int topIdx = stackIdx.back();
					stackIdx.pop_back();
					const int h = heights[topIdx];
					const int leftCol = stackIdx.empty() ? 0 : (stackIdx.back() + 1);
					const int rightCol = i - 1;
					const long long area = static_cast<long long>(h) * (rightCol - leftCol + 1);
					if (h > 0 && area > bestArea)
					{
						bestArea = area;
						bestTop = r - h + 1;   // 로컬 상단 행
						bestBottom = r;        // 로컬 하단 행(현재 행)
						bestLeft = leftCol;
						bestRight = rightCol;
					}
				}
				stackIdx.push_back(i);
			}
		}

		if (bestBottom < 0 || bestRight < 0)
			return; // 전경 없음

		// 로컬 → 절대(row,col). offset.x=col, offset.y=row.
		fv.innerRectRow1 = static_cast<double>(offset.y + bestTop);
		fv.innerRectCol1 = static_cast<double>(offset.x + bestLeft);
		fv.innerRectRow2 = static_cast<double>(offset.y + bestBottom);
		fv.innerRectCol2 = static_cast<double>(offset.x + bestRight);

		// 파생: 내접사각형 면적 / 바운딩박스 면적 (충실도 지표, 0~1)
		const double bboxArea = static_cast<double>(H) * static_cast<double>(W);
		if (bboxArea > kEps)
		{
			double ratio = static_cast<double>(bestArea) / bboxArea;
			if (ratio > 1.0) ratio = 1.0;
			fv.innerRectFillRatio = ratio;
		}
	}
	catch (const cv::Exception&) {}
}

void FeatureCalculator::ComputeRunlength(const Region& region, FeatureVector& fv) const
{
	// runlength_features(Halcon13 재현). 런 = 한 행의 연속 전경 픽셀 구간.
	//  NumRuns   = 런 개수
	//  KFactor   = NumRuns / sqrt(Area)
	//  LFactor   = NumRuns / 바운딩박스 높이(Row2-Row1+1)  ← Halcon: "런 수 / region 높이"
	//  MeanLength= Area / NumRuns
	//  Bytes(메모리 추정치)는 재현 의미가 낮아 생략(스펙 문서 기록).
	const std::vector<Run>& runs = region.Runs();
	fv.numRuns = static_cast<int>(runs.size());

	const double area = static_cast<double>(region.Area());
	const double nRuns = static_cast<double>(fv.numRuns);

	if (fv.numRuns > 0 && area > 0.0)
	{
		fv.kFactor = nRuns / std::sqrt(area);
		fv.meanRunLength = area / nRuns;
	}

	const int bboxH = region.BoundingBox().height; // Row2-Row1+1
	if (bboxH > 0)
		fv.lFactor = nRuns / static_cast<double>(bboxH);
}

void FeatureCalculator::ComputeDerived(FeatureVector& fv) const
{
	// 종횡비 = Rect2 장/단변
	if (fv.rect2Len2 > kEps)
		fv.aspectRatio = fv.rect2Len1 / fv.rect2Len2;

	// 충진율 = A / Rect2 면적 (= (2*Len1)*(2*Len2))
	const double rect2Area = (2.0 * fv.rect2Len1) * (2.0 * fv.rect2Len2);
	if (rect2Area > kEps)
		fv.fillRatio = fv.area / rect2Area;

	// 내외접비 = inner_circle.r / smallest_circle.r
	if (fv.smallestCircleRadius > kEps)
		fv.innerOuterRatio = fv.innerCircleRadius / fv.smallestCircleRadius;
}

} // namespace Grf
