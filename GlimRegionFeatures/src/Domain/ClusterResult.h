#pragma once
// ClusterResult.h
// Domain 모델: Region 집합의 다차원 feature 공간 군집화 결과.
// K-means(cv::kmeans) 실행 결과를 원본 특징값 스케일로 복원해 보관한다.
// 좌표계/스케일: m_centroids 는 항상 "원 스케일"(z-score 등 역변환 후) 값이다.
// 결정성: m_labels 는 정준(canonical) 재매핑 후의 cluster_id(0-based, 군집 크기 desc 순)이다.
//  동일 입력(FeatureMatrix + ClusterParams)이면 항상 동일한 m_labels 를 재현한다.

#include <string>
#include <vector>

namespace Grf {

// 전처리(스케일러) 파라미터. 동일 변환을 재적용(재현)하거나 원 스케일 복원에 사용.
struct ScalerParams {
	std::vector<std::string> m_features; // 컬럼 이름(순서 = 스케일된 좌표의 열 순서)
	std::vector<double> m_mean;          // zscore: 평균 / minmax: min / robust: median
	std::vector<double> m_scale;         // zscore: stddev / minmax: (max-min) / robust: IQR
	std::vector<bool> m_log1p;           // 컬럼별 log1p 선적용 여부(면적류 옵션)

	ScalerParams() {}
};

// 하나의 K 값에 대한 군집화 결과.
struct ClusterResult {
	int m_k;
	std::vector<int> m_labels;                     // 샘플별 정준 cluster_id(0-based). 실패 시 비어있음.
	std::vector<std::vector<double> > m_centroids;  // [cluster][feature] 원 스케일 복원값
	double m_silhouette;                            // 전체 평균 실루엣(-1~1), 계산 불가 시 0
	ScalerParams m_scaler;
	std::vector<int> m_clusterSizes;                // cluster_id 순서(=centroids 순서)의 샘플 수
	bool m_ok;
	std::string m_error;

	ClusterResult()
		: m_k(0), m_silhouette(0.0), m_ok(false) {}
};

} // namespace Grf
