// FeatureVector.cpp
#include "Domain/FeatureVector.h"
#include <sstream>
#include <map>

namespace Grf {

FeatureVector::FeatureVector()
{
	area = 0.0; centerRow = 0.0; centerCol = 0.0;
	areaHoles = 0.0;
	contLength = 0.0;
	diameter = 0.0; diaRow1 = 0.0; diaCol1 = 0.0; diaRow2 = 0.0; diaCol2 = 0.0;
	circularity = 0.0; compactness = 0.0; convexity = 0.0; rectangularity = 0.0;
	roundnessDistance = 0.0; roundnessSigma = 0.0; roundness = 0.0; roundnessSides = 0.0;
	ra = 0.0; rb = 0.0; phi = 0.0;
	anisometry = 0.0; bulkiness = 0.0; structureFactor = 0.0;
	orientation = 0.0;
	bboxRow1 = 0.0; bboxCol1 = 0.0; bboxRow2 = 0.0; bboxCol2 = 0.0;
	rect2CenterRow = 0.0; rect2CenterCol = 0.0; rect2Len1 = 0.0; rect2Len2 = 0.0; rect2Phi = 0.0;
	smallestCircleRow = 0.0; smallestCircleCol = 0.0; smallestCircleRadius = 0.0;
	innerCircleRow = 0.0; innerCircleCol = 0.0; innerCircleRadius = 0.0;
	connectComponents = 0; holes = 0; eulerNumber = 0;
	for (int i = 0; i < 7; ++i) hu[i] = 0.0;
	innerRectRow1 = 0.0; innerRectCol1 = 0.0; innerRectRow2 = 0.0; innerRectCol2 = 0.0;
	innerRectFillRatio = 0.0;
	numRuns = 0; kFactor = 0.0; lFactor = 0.0; meanRunLength = 0.0;
	aspectRatio = 0.0; fillRatio = 0.0; innerOuterRatio = 0.0;
}

double FeatureVector::GetByName(const std::string& name) const
{
	// 이름 → 값 매핑. 스코어/룰에서 참조하는 핵심 값 위주.
	if (name == "area")             return area;
	if (name == "center_row")       return centerRow;
	if (name == "center_col")       return centerCol;
	if (name == "area_holes")       return areaHoles;
	if (name == "contlength")       return contLength;
	if (name == "diameter")         return diameter;
	if (name == "circularity")      return circularity;
	if (name == "compactness")      return compactness;
	if (name == "convexity")        return convexity;
	if (name == "rectangularity")   return rectangularity;
	if (name == "roundness")        return roundness;
	if (name == "roundness_distance") return roundnessDistance;
	if (name == "roundness_sigma")  return roundnessSigma;
	if (name == "sides")            return roundnessSides;
	if (name == "ra")               return ra;
	if (name == "rb")               return rb;
	if (name == "phi")              return phi;
	if (name == "anisometry")       return anisometry;
	if (name == "bulkiness")        return bulkiness;
	if (name == "structure_factor") return structureFactor;
	if (name == "orientation")      return orientation;
	if (name == "smallest_circle_radius") return smallestCircleRadius;
	if (name == "inner_circle_radius")    return innerCircleRadius;
	if (name == "connect_components") return static_cast<double>(connectComponents);
	if (name == "holes")            return static_cast<double>(holes);
	if (name == "euler_number")     return static_cast<double>(eulerNumber);
	if (name == "aspect_ratio")     return aspectRatio;
	if (name == "fill_ratio")       return fillRatio;
	if (name == "inner_outer_ratio")return innerOuterRatio;
	if (name == "rect2_len1")       return rect2Len1;
	if (name == "rect2_len2")       return rect2Len2;
	if (name == "inner_rect_row1")  return innerRectRow1;
	if (name == "inner_rect_col1")  return innerRectCol1;
	if (name == "inner_rect_row2")  return innerRectRow2;
	if (name == "inner_rect_col2")  return innerRectCol2;
	if (name == "inner_rect_fill_ratio") return innerRectFillRatio;
	if (name == "num_runs")         return static_cast<double>(numRuns);
	if (name == "k_factor")         return kFactor;
	if (name == "l_factor")         return lFactor;
	if (name == "mean_run_length")  return meanRunLength;
	return 0.0;
}

std::string FeatureVector::CsvHeader()
{
	return
		"area,center_row,center_col,area_holes,contlength,diameter,"
		"circularity,compactness,convexity,rectangularity,"
		"roundness_distance,roundness_sigma,roundness,sides,"
		"ra,rb,phi,anisometry,bulkiness,structure_factor,orientation,"
		"bbox_row1,bbox_col1,bbox_row2,bbox_col2,"
		"rect2_center_row,rect2_center_col,rect2_len1,rect2_len2,rect2_phi,"
		"smallest_circle_row,smallest_circle_col,smallest_circle_radius,"
		"inner_circle_row,inner_circle_col,inner_circle_radius,"
		"connect_components,holes,euler_number,"
		"aspect_ratio,fill_ratio,inner_outer_ratio,"
		"hu0,hu1,hu2,hu3,hu4,hu5,hu6,"
		"inner_rect_row1,inner_rect_col1,inner_rect_row2,inner_rect_col2,inner_rect_fill_ratio,"
		"num_runs,k_factor,l_factor,mean_run_length";
}

std::string FeatureVector::ToCsvRow() const
{
	std::ostringstream oss;
	oss.precision(6);
	oss << std::fixed;
	oss << area << ',' << centerRow << ',' << centerCol << ','
		<< areaHoles << ',' << contLength << ',' << diameter << ','
		<< circularity << ',' << compactness << ',' << convexity << ',' << rectangularity << ','
		<< roundnessDistance << ',' << roundnessSigma << ',' << roundness << ',' << roundnessSides << ','
		<< ra << ',' << rb << ',' << phi << ',' << anisometry << ',' << bulkiness << ',' << structureFactor << ',' << orientation << ','
		<< bboxRow1 << ',' << bboxCol1 << ',' << bboxRow2 << ',' << bboxCol2 << ','
		<< rect2CenterRow << ',' << rect2CenterCol << ',' << rect2Len1 << ',' << rect2Len2 << ',' << rect2Phi << ','
		<< smallestCircleRow << ',' << smallestCircleCol << ',' << smallestCircleRadius << ','
		<< innerCircleRow << ',' << innerCircleCol << ',' << innerCircleRadius << ','
		<< connectComponents << ',' << holes << ',' << eulerNumber << ','
		<< aspectRatio << ',' << fillRatio << ',' << innerOuterRatio;
	for (int i = 0; i < 7; ++i)
		oss << ',' << hu[i];
	oss << ',' << innerRectRow1 << ',' << innerRectCol1 << ',' << innerRectRow2 << ',' << innerRectCol2 << ',' << innerRectFillRatio
		<< ',' << numRuns << ',' << kFactor << ',' << lFactor << ',' << meanRunLength;
	return oss.str();
}

} // namespace Grf
