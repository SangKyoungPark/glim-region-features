#pragma once
// ChartPanelCtrl.h
// 분석 탭 GDI 직접 렌더 차트 패널(외부 라이브러리 없음).
//  ① 분류코드별 카운트 가로막대
//  ② 코드별 score 평균 묶음막대(코드 × score 특징)
//  ③ 특징값 히스토그램(선택 특징, 코드별 색 누적)
// 축/레이블/범례 포함. 더블버퍼링.

#include <vector>
#include <string>
#include "GrfViewSupport.h"

class CChartPanelCtrl : public CWnd
{
public:
	CChartPanelCtrl();
	virtual ~CChartPanelCtrl();

	BOOL CreateCtrl(CWnd* pParent, const CRect& rc, UINT id);

	// 데이터 바인딩(소유권 없음). scoreFeatureNames = 프로파일 Score 특징 순서(묶음막대 축).
	void SetData(const std::vector<GrfView::RegionResult>* results,
		bool profileLoaded, const std::vector<std::string>& scoreFeatureNames);
	// 히스토그램 대상 특징명(예: "area", "circularity").
	void SetHistogramFeature(const std::string& featureName);
	void Clear();

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()

private:
	void DrawAll(CDC* pDC, const CRect& client);
	void DrawCountBar(CDC* pDC, const CRect& area);       // ①
	void DrawScoreGroupBar(CDC* pDC, const CRect& area);  // ②
	void DrawHistogram(CDC* pDC, const CRect& area);      // ③
	void DrawTitle(CDC* pDC, const CRect& area, LPCTSTR title);

	// 코드 목록/카운트 집계(그리기 시점에 계산)
	void CollectCodes(std::vector<std::string>& codesOut, std::vector<int>& countsOut) const;

	const std::vector<GrfView::RegionResult>* m_results;
	bool m_profileLoaded;
	std::vector<std::string> m_scoreFeatures;
	std::string m_histFeature;
};
