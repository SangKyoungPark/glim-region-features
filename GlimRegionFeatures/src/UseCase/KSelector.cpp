// KSelector.cpp
#include "UseCase/KSelector.h"
#include "UseCase/FeatureScaler.h"

#include <algorithm>
#include <opencv2/core.hpp>

namespace Grf {

KSelector::KSelector()
{
}

KSelector::Curve KSelector::Sweep(const FeatureMatrix& matrix, const ClusterParams& baseParams,
	int kMin, int kMax) const
{
	Curve curve;
	try
	{
		if (matrix.Empty())
		{
			curve.m_error = "빈 FeatureMatrix";
			return curve;
		}
		const int n = static_cast<int>(matrix.RowCount());
		const int loK = std::max(2, kMin);
		const int hiK = std::min(kMax, n - 1);
		if (hiK < loK)
		{
			curve.m_error = "유효한 K 범위가 없습니다(샘플 수 부족: n=" + std::to_string(n) + ")";
			return curve;
		}

		FeatureScaler scaler;
		ScalerParams sp = scaler.Fit(matrix, baseParams.m_features, baseParams.m_scaleMode, baseParams.m_logAreaLike);
		if (sp.m_features.empty())
		{
			curve.m_error = "유효한 feature 가 없습니다(이름/CSV 헤더 확인 필요)";
			return curve;
		}
		std::vector<std::vector<double> > scaled = scaler.Apply(matrix, sp);
		const int dims = static_cast<int>(sp.m_features.size());

		cv::Mat samples(n, dims, CV_32F);
		for (int i = 0; i < n; ++i)
			for (int j = 0; j < dims; ++j)
				samples.at<float>(i, j) = static_cast<float>(scaled[i][j]);

		const int attempts = (baseParams.m_attempts > 0) ? baseParams.m_attempts : 5;
		cv::TermCriteria criteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 100, 1e-4);

		double bestSil = -2.0;
		int bestK = loK;

		for (int k = loK; k <= hiK; ++k)
		{
			cv::setRNGSeed(baseParams.m_seed);
			cv::Mat labelsMat, centersMat;
			const double compactness = cv::kmeans(samples, k, labelsMat, criteria, attempts,
				cv::KMEANS_PP_CENTERS, centersMat);

			std::vector<int> labels(n, 0);
			for (int i = 0; i < n; ++i)
				labels[i] = labelsMat.at<int>(i, 0);

			const double sil = ClusterEngine::ComputeSilhouette(scaled, labels, k);

			curve.m_k.push_back(k);
			curve.m_silhouette.push_back(sil);
			curve.m_inertia.push_back(compactness);

			if (sil > bestSil)
			{
				bestSil = sil;
				bestK = k;
			}
		}
		curve.m_recommendedK = bestK;
		curve.m_ok = true;
	}
	catch (const cv::Exception& e)
	{
		curve.m_ok = false;
		curve.m_error = std::string("cv::Exception: ") + e.what();
	}
	catch (const std::exception& e)
	{
		curve.m_ok = false;
		curve.m_error = std::string("std::exception: ") + e.what();
	}
	catch (...)
	{
		curve.m_ok = false;
		curve.m_error = "알 수 없는 예외";
	}
	return curve;
}

} // namespace Grf
