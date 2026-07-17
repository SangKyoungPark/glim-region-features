#pragma once
// ClusterPanelCtrl.h
// 군집 탭 GDI 직접 렌더 패널(외부 라이브러리 없음).
//  - 산점도: 선택한 두 raw 특징값 축, 점 색 = cluster_id
//  - 우측 범례: 군집별 색/크기(멤버 수) + 전체 실루엣 점수
//  결과 미보유 시 안내 문구. 더블버퍼링(ChartPanelCtrl 과 동일 골격).
// 데이터는 소유하지 않는다(다이얼로그 소유 벡터를 포인터로 바인딩).

#include <vector>
#include <string>
#include "GrfViewSupport.h"

class CClusterPanelCtrl : public CWnd
{
public:
	CClusterPanelCtrl();
	virtual ~CClusterPanelCtrl();

	BOOL CreateCtrl(CWnd* pParent, const CRect& rc, UINT id);

	// 데이터 바인딩(소유권 없음).
	//  labels: results 와 병렬(같은 인덱스 = 같은 Region). NULL 이면 군집 미실행 상태.
	void SetData(const std::vector<GrfView::RegionResult>* results,
		const std::vector<int>* labels, int k, double silhouette,
		const std::vector<int>* clusterSizes);
	// 산점도 축 특징명(FeatureVector::GetByName 이름).
	void SetAxes(const std::string& xFeature, const std::string& yFeature);
	void Clear();

	// cluster_id → 고정 팔레트 색(카드/범례 공용으로 쓸 수 있게 공개 static).
	static COLORREF ClusterColor(int clusterId);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()

private:
	void DrawAll(CDC* pDC, const CRect& client);
	void DrawScatter(CDC* pDC, const CRect& area);
	void DrawLegend(CDC* pDC, const CRect& area);
	void DrawMessage(CDC* pDC, const CRect& client, LPCTSTR text);

	const std::vector<GrfView::RegionResult>* m_results;
	const std::vector<int>* m_labels;
	const std::vector<int>* m_clusterSizes;
	int m_k;
	double m_silhouette;
	std::string m_xFeature;
	std::string m_yFeature;
};
