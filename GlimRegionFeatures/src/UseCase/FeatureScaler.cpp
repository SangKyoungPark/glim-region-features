// FeatureScaler.cpp
#include "UseCase/FeatureScaler.h"
#include <algorithm>
#include <cmath>

namespace Grf {

namespace {
	// 선형보간 percentile(p: 0~100). 입력은 값 복사(정렬 후 사용, 호출부 원본은 불변).
	double PercentileOf(std::vector<double> v, double p)
	{
		if (v.empty())
			return 0.0;
		std::sort(v.begin(), v.end());
		if (v.size() == 1)
			return v[0];
		double idx = (std::max(0.0, std::min(100.0, p)) / 100.0) * static_cast<double>(v.size() - 1);
		size_t lo = static_cast<size_t>(std::floor(idx));
		size_t hi = static_cast<size_t>(std::ceil(idx));
		if (lo == hi)
			return v[lo];
		double frac = idx - static_cast<double>(lo);
		return v[lo] + (v[hi] - v[lo]) * frac;
	}
}

FeatureScaler::FeatureScaler()
{
}

bool FeatureScaler::IsAreaLikeName(const std::string& name)
{
	return name.find("area") != std::string::npos
		|| name.find("contlength") != std::string::npos
		|| name.find("diameter") != std::string::npos;
}

ScalerParams FeatureScaler::Fit(const FeatureMatrix& matrix, const std::vector<std::string>& featureNames,
	int scaleMode, bool logAreaLike) const
{
	ScalerParams params;
	if (matrix.Empty())
		return params;

	std::vector<std::string> names = featureNames;
	if (names.empty())
		names = matrix.m_columns;

	// matrix 에 실제 존재하는 이름만 채택(순서 유지)
	std::vector<int> colIdx;
	for (size_t i = 0; i < names.size(); ++i)
	{
		int idx = matrix.ColumnIndex(names[i]);
		if (idx >= 0)
		{
			params.m_features.push_back(names[i]);
			colIdx.push_back(idx);
		}
	}
	if (params.m_features.empty())
		return params; // 빈 ScalerParams = 실패 신호(호출부가 판단)

	const size_t n = matrix.RowCount();
	const size_t m = params.m_features.size();
	params.m_mean.assign(m, 0.0);
	params.m_scale.assign(m, 1.0);
	params.m_log1p.assign(m, false);

	if (n == 0)
		return ScalerParams(); // 행이 없으면 스케일 계산 불가 -> 실패

	try
	{
		for (size_t j = 0; j < m; ++j)
		{
			if (logAreaLike && IsAreaLikeName(params.m_features[j]))
				params.m_log1p[j] = true;

			std::vector<double> col(n);
			for (size_t i = 0; i < n; ++i)
			{
				double v = matrix.m_rows[i][colIdx[j]];
				if (params.m_log1p[j])
					v = std::log1p(std::max(0.0, v));
				col[i] = v;
			}

			if (scaleMode == SCALE_MINMAX)
			{
				double vmin = col[0], vmax = col[0];
				for (size_t i = 1; i < n; ++i)
				{
					vmin = std::min(vmin, col[i]);
					vmax = std::max(vmax, col[i]);
				}
				double range = vmax - vmin;
				params.m_mean[j] = vmin;
				params.m_scale[j] = (range > 1e-12) ? range : 1.0;
			}
			else if (scaleMode == SCALE_ROBUST)
			{
				double median = PercentileOf(col, 50.0);
				double q1 = PercentileOf(col, 25.0);
				double q3 = PercentileOf(col, 75.0);
				double iqr = q3 - q1;
				params.m_mean[j] = median;
				params.m_scale[j] = (iqr > 1e-12) ? iqr : 1.0;
			}
			else // SCALE_ZSCORE (기본)
			{
				double sum = 0.0;
				for (size_t i = 0; i < n; ++i)
					sum += col[i];
				double mean = sum / static_cast<double>(n);
				double sq = 0.0;
				for (size_t i = 0; i < n; ++i)
				{
					double d = col[i] - mean;
					sq += d * d;
				}
				double stddev = std::sqrt(sq / static_cast<double>(n));
				params.m_mean[j] = mean;
				params.m_scale[j] = (stddev > 1e-12) ? stddev : 1.0;
			}
		}
	}
	catch (const std::exception&)
	{
		return ScalerParams(); // 계산 실패 시 빈 파라미터(호출부가 실패로 간주)
	}

	return params;
}

std::vector<std::vector<double> > FeatureScaler::Apply(const FeatureMatrix& matrix, const ScalerParams& scaler) const
{
	std::vector<std::vector<double> > out;
	if (scaler.m_features.empty())
		return out;

	const size_t n = matrix.RowCount();
	const size_t m = scaler.m_features.size();
	out.assign(n, std::vector<double>(m, 0.0));

	std::vector<int> colIdx(m, -1);
	for (size_t j = 0; j < m; ++j)
		colIdx[j] = matrix.ColumnIndex(scaler.m_features[j]);

	for (size_t i = 0; i < n; ++i)
	{
		for (size_t j = 0; j < m; ++j)
		{
			double v = 0.0;
			if (colIdx[j] >= 0 && colIdx[j] < static_cast<int>(matrix.m_rows[i].size()))
				v = matrix.m_rows[i][colIdx[j]];
			if (j < scaler.m_log1p.size() && scaler.m_log1p[j])
				v = std::log1p(std::max(0.0, v));
			double scale = (j < scaler.m_scale.size() && std::fabs(scaler.m_scale[j]) > 1e-12) ? scaler.m_scale[j] : 1.0;
			double mean = (j < scaler.m_mean.size()) ? scaler.m_mean[j] : 0.0;
			out[i][j] = (v - mean) / scale;
		}
	}
	return out;
}

std::vector<double> FeatureScaler::InverseTransform(const std::vector<double>& scaledPoint, const ScalerParams& scaler) const
{
	std::vector<double> out(scaledPoint.size(), 0.0);
	for (size_t j = 0; j < scaledPoint.size() && j < scaler.m_mean.size(); ++j)
	{
		double scale = (j < scaler.m_scale.size() && std::fabs(scaler.m_scale[j]) > 1e-12) ? scaler.m_scale[j] : 1.0;
		double v = scaledPoint[j] * scale + scaler.m_mean[j];
		if (j < scaler.m_log1p.size() && scaler.m_log1p[j])
			v = std::expm1(v); // log1p 역변환
		out[j] = v;
	}
	return out;
}

} // namespace Grf
