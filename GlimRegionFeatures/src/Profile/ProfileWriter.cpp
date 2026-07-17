// ProfileWriter.cpp
#include "Profile/ProfileWriter.h"
#include <sstream>
#include <algorithm>
#include <cmath>

namespace Grf {

ProfileWriter::ProfileWriter()
{
}

double ProfileWriter::Percentile(std::vector<double> values, double p)
{
	if (values.empty())
		return 0.0;
	std::sort(values.begin(), values.end());
	if (values.size() == 1)
		return values[0];
	const double pc = std::max(0.0, std::min(100.0, p)) / 100.0;
	const double idx = pc * static_cast<double>(values.size() - 1);
	const size_t lo = static_cast<size_t>(std::floor(idx));
	const size_t hi = static_cast<size_t>(std::ceil(idx));
	if (lo == hi)
		return values[lo];
	const double frac = idx - static_cast<double>(lo);
	return values[lo] + (values[hi] - values[lo]) * frac;
}

std::string ProfileWriter::BuildIni(const FeatureMatrix& matrix, const ClusterResult& result,
	const ProfileWriteParams& params, const std::vector<std::string>& features,
	bool& okOut, std::string& errorOut) const
{
	okOut = false;
	errorOut.clear();

	try
	{
		if (matrix.Empty())
		{
			errorOut = "빈 FeatureMatrix";
			return std::string();
		}
		if (!result.m_ok || result.m_labels.size() != matrix.RowCount())
		{
			errorOut = "ClusterResult 가 유효하지 않습니다(실패 또는 라벨 수 불일치)";
			return std::string();
		}
		if (params.m_labels.empty())
		{
			errorOut = "라벨(clusterId->불량코드)이 지정되지 않았습니다";
			return std::string();
		}

		std::vector<std::string> featNames = features;
		if (featNames.empty())
			featNames = result.m_scaler.m_features;
		if (featNames.empty())
		{
			errorOut = "룰 대상 feature 가 없습니다";
			return std::string();
		}

		// matrix 에서 실제 조회 가능한 컬럼만 채택
		std::vector<int> colIdx;
		std::vector<std::string> validFeat;
		for (size_t i = 0; i < featNames.size(); ++i)
		{
			const int idx = matrix.ColumnIndex(featNames[i]);
			if (idx >= 0)
			{
				colIdx.push_back(idx);
				validFeat.push_back(featNames[i]);
			}
		}
		if (validFeat.empty())
		{
			errorOut = "matrix 에서 조회 가능한 feature 가 없습니다";
			return std::string();
		}

		const double pLow = params.m_percentileLow;
		const double pHigh = params.m_percentileHigh;

		std::ostringstream oss;
		oss << "; GlimRegionFeatures cluster-to-profile 초안 (자동 생성, 검토 후 저장할 것)\r\n";
		oss << "[Profile]\r\n";
		oss << "name = " << (params.m_profileName.empty() ? std::string("ClusterDraft") : params.m_profileName) << "\r\n\r\n";

		// [Score] : 전체 샘플 기준 percentile envelope 초안. 방향은 항상 inc(증가형) — 특징값별 의미는
		//  사용자가 검토 후 필요 시 dec 로 수정할 것(엔진은 방향에 대한 사전 지식이 없음).
		oss << "[Score]\r\n";
		for (size_t f = 0; f < validFeat.size(); ++f)
		{
			std::vector<double> allVals;
			allVals.reserve(matrix.RowCount());
			for (size_t r = 0; r < matrix.RowCount(); ++r)
				allVals.push_back(matrix.m_rows[r][colIdx[f]]);
			double vMin = Percentile(allVals, pLow);
			double vMax = Percentile(allVals, pHigh);
			if (vMax <= vMin)
				vMax = vMin + 1e-6;
			oss << validFeat[f] << " = " << vMin << ", " << vMax << ", inc\r\n";
		}
		oss << "\r\n";

		// [SelectShape]
		oss << "[SelectShape]\r\n";
		oss << "codes = ";
		bool firstCode = true;
		for (std::map<int, std::string>::const_iterator it = params.m_labels.begin();
			it != params.m_labels.end(); ++it)
		{
			if (!firstCode) oss << ", ";
			oss << it->second;
			firstCode = false;
		}
		oss << "\r\n\r\n";

		// [Rule_<CODE>] : cluster 멤버 percentile envelope(AND 결합)
		int priority = 0;
		for (std::map<int, std::string>::const_iterator it = params.m_labels.begin();
			it != params.m_labels.end(); ++it, ++priority)
		{
			const int clusterId = it->first;
			const std::string& code = it->second;

			std::vector<size_t> memberRows;
			for (size_t r = 0; r < result.m_labels.size(); ++r)
				if (result.m_labels[r] == clusterId)
					memberRows.push_back(r);

			oss << "[Rule_" << code << "]\r\n";
			oss << "combine = AND\r\n";
			oss << "priority = " << priority << "\r\n";

			if (memberRows.empty())
			{
				oss << "; 경고: cluster " << clusterId << " 에 멤버가 없습니다(빈 규칙 — 검토 필요)\r\n\r\n";
				continue;
			}

			for (size_t f = 0; f < validFeat.size(); ++f)
			{
				std::vector<double> vals;
				vals.reserve(memberRows.size());
				for (size_t m = 0; m < memberRows.size(); ++m)
					vals.push_back(matrix.m_rows[memberRows[m]][colIdx[f]]);
				double vMin = Percentile(vals, pLow);
				double vMax = Percentile(vals, pHigh);
				if (vMax <= vMin)
					vMax = vMin + 1e-6;
				oss << validFeat[f] << " = " << vMin << ", " << vMax << "\r\n";
			}
			oss << "\r\n";
		}

		okOut = true;
		return oss.str();
	}
	catch (const std::exception& e)
	{
		errorOut = std::string("std::exception: ") + e.what();
		return std::string();
	}
	catch (...)
	{
		errorOut = "알 수 없는 예외";
		return std::string();
	}
}

} // namespace Grf
