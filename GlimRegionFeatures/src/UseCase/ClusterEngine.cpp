// ClusterEngine.cpp
#include "UseCase/ClusterEngine.h"
#include "UseCase/FeatureScaler.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <opencv2/core.hpp>

namespace Grf {

ClusterEngine::ClusterEngine()
{
}

double ClusterEngine::ComputeSilhouette(const std::vector<std::vector<double> >& scaled,
	const std::vector<int>& labels, int k)
{
	const size_t n = scaled.size();
	if (n < 2 || k < 2 || labels.size() != n)
		return 0.0;

	try
	{
		// 군집별 인덱스 목록
		std::vector<std::vector<int> > byCluster(k);
		for (size_t i = 0; i < n; ++i)
			if (labels[i] >= 0 && labels[i] < k)
				byCluster[labels[i]].push_back(static_cast<int>(i));

		double sum = 0.0;
		int counted = 0;
		for (size_t i = 0; i < n; ++i)
		{
			const int ci = labels[i];
			if (ci < 0 || ci >= k)
				continue;
			const std::vector<int>& own = byCluster[ci];
			if (own.size() <= 1)
				continue; // 단일 샘플 군집은 실루엣 정의상 평균 계산에서 제외

			double a = 0.0;
			int aCount = 0;
			for (size_t t = 0; t < own.size(); ++t)
			{
				const int j = own[t];
				if (j == static_cast<int>(i))
					continue;
				double d = 0.0;
				for (size_t f = 0; f < scaled[i].size(); ++f)
				{
					const double diff = scaled[i][f] - scaled[j][f];
					d += diff * diff;
				}
				a += std::sqrt(d);
				aCount++;
			}
			a = (aCount > 0) ? (a / aCount) : 0.0;

			double b = -1.0;
			for (int c = 0; c < k; ++c)
			{
				if (c == ci)
					continue;
				const std::vector<int>& other = byCluster[c];
				if (other.empty())
					continue;
				double d = 0.0;
				for (size_t t = 0; t < other.size(); ++t)
				{
					const int j = other[t];
					double dd = 0.0;
					for (size_t f = 0; f < scaled[i].size(); ++f)
					{
						const double diff = scaled[i][f] - scaled[j][f];
						dd += diff * diff;
					}
					d += std::sqrt(dd);
				}
				d /= static_cast<double>(other.size());
				if (b < 0.0 || d < b)
					b = d;
			}
			if (b < 0.0)
				continue;

			const double denom = std::max(a, b);
			const double s = (denom > 1e-12) ? (b - a) / denom : 0.0;
			sum += s;
			counted++;
		}
		return (counted > 0) ? (sum / counted) : 0.0;
	}
	catch (const std::exception&)
	{
		return 0.0;
	}
}

std::vector<int> ClusterEngine::CanonicalRelabel(
	const std::vector<int>& rawLabels, const std::vector<std::vector<double> >& scaledCentroids,
	std::vector<std::vector<double> >& centroidsOut, std::vector<int>& sizesOut)
{
	const int k = static_cast<int>(scaledCentroids.size());
	std::vector<int> rawSizes(k, 0);
	for (size_t i = 0; i < rawLabels.size(); ++i)
		if (rawLabels[i] >= 0 && rawLabels[i] < k)
			rawSizes[rawLabels[i]]++;

	// 정렬 키: 크기 desc, 동률이면 첫 feature 중심 오름차순, 완전 동률이면 원 인덱스(안정 정렬).
	std::vector<int> order(k);
	for (int i = 0; i < k; ++i)
		order[i] = i;
	std::sort(order.begin(), order.end(), [&](int a, int b) {
		if (rawSizes[a] != rawSizes[b])
			return rawSizes[a] > rawSizes[b];
		const double fa = scaledCentroids[a].empty() ? 0.0 : scaledCentroids[a][0];
		const double fb = scaledCentroids[b].empty() ? 0.0 : scaledCentroids[b][0];
		if (fa != fb)
			return fa < fb;
		return a < b;
	});

	std::vector<int> rawToCanonical(k, 0);
	for (int newId = 0; newId < k; ++newId)
		rawToCanonical[order[newId]] = newId;

	std::vector<int> labelsOut(rawLabels.size(), -1);
	for (size_t i = 0; i < rawLabels.size(); ++i)
		if (rawLabels[i] >= 0 && rawLabels[i] < k)
			labelsOut[i] = rawToCanonical[rawLabels[i]];

	centroidsOut.assign(k, std::vector<double>());
	sizesOut.assign(k, 0);
	for (int oldId = 0; oldId < k; ++oldId)
	{
		const int newId = rawToCanonical[oldId];
		centroidsOut[newId] = scaledCentroids[oldId];
		sizesOut[newId] = rawSizes[oldId];
	}
	return labelsOut;
}

ClusterResult ClusterEngine::Run(const FeatureMatrix& matrix, const ClusterParams& params) const
{
	ClusterResult result;
	try
	{
		if (matrix.Empty())
		{
			result.m_error = "빈 FeatureMatrix";
			return result;
		}
		if (params.m_k <= 0)
		{
			result.m_error = "K 는 1 이상이어야 합니다(자동 K 는 KSelector 로 먼저 결정할 것)";
			return result;
		}
		const int n = static_cast<int>(matrix.RowCount());
		if (params.m_k > n)
		{
			result.m_error = "K(" + std::to_string(params.m_k) + ") 가 샘플 수(" +
				std::to_string(n) + ") 보다 큽니다";
			return result;
		}

		FeatureScaler scaler;
		ScalerParams sp = scaler.Fit(matrix, params.m_features, params.m_scaleMode, params.m_logAreaLike);
		if (sp.m_features.empty())
		{
			result.m_error = "유효한 feature 가 없습니다(이름/CSV 헤더 확인 필요)";
			return result;
		}
		std::vector<std::vector<double> > scaled = scaler.Apply(matrix, sp);

		const int dims = static_cast<int>(sp.m_features.size());
		cv::Mat samples(n, dims, CV_32F);
		for (int i = 0; i < n; ++i)
			for (int j = 0; j < dims; ++j)
				samples.at<float>(i, j) = static_cast<float>(scaled[i][j]);

		// 결정성 규칙 1): 고정 seed + 고정 attempts + KMEANS_PP_CENTERS + 고정 TermCriteria
		cv::setRNGSeed(params.m_seed);
		cv::Mat labelsMat, centersMat;
		cv::TermCriteria criteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 100, 1e-4);
		const int attempts = (params.m_attempts > 0) ? params.m_attempts : 5;
		cv::kmeans(samples, params.m_k, labelsMat, criteria, attempts, cv::KMEANS_PP_CENTERS, centersMat);

		std::vector<int> rawLabels(n, 0);
		for (int i = 0; i < n; ++i)
			rawLabels[i] = labelsMat.at<int>(i, 0);

		std::vector<std::vector<double> > scaledCentroids(params.m_k, std::vector<double>(dims, 0.0));
		for (int c = 0; c < params.m_k; ++c)
			for (int j = 0; j < dims; ++j)
				scaledCentroids[c][j] = static_cast<double>(centersMat.at<float>(c, j));

		// 결정성 규칙 4): 정준 라벨 재매핑
		std::vector<std::vector<double> > canonCentroidsScaled;
		std::vector<int> sizes;
		std::vector<int> canonLabels = CanonicalRelabel(rawLabels, scaledCentroids, canonCentroidsScaled, sizes);

		// centroid 원 스케일 복원(해석용)
		std::vector<std::vector<double> > centroidsOrig(params.m_k);
		for (int c = 0; c < params.m_k; ++c)
			centroidsOrig[c] = scaler.InverseTransform(canonCentroidsScaled[c], sp);

		const double silhouette = ComputeSilhouette(scaled, canonLabels, params.m_k);

		result.m_k = params.m_k;
		result.m_labels = canonLabels;
		result.m_centroids = centroidsOrig;
		result.m_silhouette = silhouette;
		result.m_scaler = sp;
		result.m_clusterSizes = sizes;
		result.m_ok = true;
	}
	catch (const cv::Exception& e)
	{
		result.m_ok = false;
		result.m_error = std::string("cv::Exception: ") + e.what();
	}
	catch (const std::exception& e)
	{
		result.m_ok = false;
		result.m_error = std::string("std::exception: ") + e.what();
	}
	catch (...)
	{
		result.m_ok = false;
		result.m_error = "알 수 없는 예외";
	}
	return result;
}

} // namespace Grf
