#pragma once
// ResultCardCtrl.h
// 결과 탭 카드형 리스트. Region 1건 = 카드 1행.
//  [원본 썸네일] [오버레이 썸네일] [핵심 특징값] [score 미니 막대] [분류 뱃지]
// 스크롤 가능한 커스텀 그리기. 더블버퍼 + 가시 행만 렌더. 썸네일은 LRU 캐시로 lazy 디코드.
// 행 클릭 시 부모에게 WM_GRFVIEW_CARD_SEL(wParam=결과 인덱스) 통지.

#include <vector>
#include <opencv2/core.hpp>
#include "GrfViewSupport.h"

// 카드 선택 통지 메시지(wParam = m_results 인덱스, -1 = 없음)
#define WM_GRFVIEW_CARD_SEL (WM_APP + 21)

class CResultCardCtrl : public CWnd
{
public:
	CResultCardCtrl();
	virtual ~CResultCardCtrl();

	BOOL CreateCtrl(CWnd* pParent, const CRect& rc, UINT id);

	// 데이터 바인딩(소유권 없음 — 포인터 참조). profileLoaded 는 score 막대 표시 여부.
	void SetData(const std::vector<GrfView::RegionResult>* results,
		GrfView::ThumbCache* cache, bool profileLoaded);
	void Clear();

	int GetSelection() const { return m_sel; }
	void SetSelection(int idx, bool ensureVisible = true);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()

private:
	void DrawContent(CDC* pDC, const CRect& client);
	void DrawCard(CDC* pDC, const CRect& cardRc, int resultIdx, bool selected);
	cv::Mat MakeOverlayThumb(const GrfView::RegionResult& rr);
	int ContentHeight() const;
	void UpdateScrollbar();
	void ScrollTo(int y);

	const std::vector<GrfView::RegionResult>* m_results;
	GrfView::ThumbCache* m_cache;
	bool m_profileLoaded;

	int m_scrollY;   // 스크롤 오프셋(px)
	int m_cardH;     // 카드 1개 높이
	int m_sel;       // 선택 인덱스(-1 없음)
};
