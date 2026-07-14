// ResultCardCtrl.cpp : 결과 카드 리스트 구현
#include "stdafx.h"
#include "ResultCardCtrl.h"

#include <opencv2/imgproc.hpp>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace {
	const int kCardH = 104;   // 카드 높이
	const int kThumb = 88;    // 썸네일 한 변
	const int kPad = 8;
}

CResultCardCtrl::CResultCardCtrl()
	: m_results(NULL)
	, m_cache(NULL)
	, m_profileLoaded(false)
	, m_scrollY(0)
	, m_cardH(kCardH)
	, m_sel(-1)
{
}

CResultCardCtrl::~CResultCardCtrl()
{
}

BEGIN_MESSAGE_MAP(CResultCardCtrl, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_VSCROLL()
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONDOWN()
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CResultCardCtrl::CreateCtrl(CWnd* pParent, const CRect& rc, UINT id)
{
	LPCTSTR cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
		::LoadCursor(NULL, IDC_ARROW), NULL, NULL);
	return CWnd::Create(cls, _T(""),
		WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL,
		rc, pParent, id);
}

void CResultCardCtrl::SetData(const std::vector<GrfView::RegionResult>* results,
	GrfView::ThumbCache* cache, bool profileLoaded)
{
	m_results = results;
	m_cache = cache;
	m_profileLoaded = profileLoaded;
	m_scrollY = 0;
	m_sel = -1;
	if (GetSafeHwnd())
	{
		UpdateScrollbar();
		Invalidate(FALSE);
	}
}

void CResultCardCtrl::Clear()
{
	m_results = NULL;
	m_scrollY = 0;
	m_sel = -1;
	if (GetSafeHwnd())
	{
		UpdateScrollbar();
		Invalidate(FALSE);
	}
}

int CResultCardCtrl::ContentHeight() const
{
	const int n = m_results ? static_cast<int>(m_results->size()) : 0;
	return n * m_cardH;
}

void CResultCardCtrl::UpdateScrollbar()
{
	CRect client;
	GetClientRect(&client);
	const int content = ContentHeight();

	SCROLLINFO si;
	::ZeroMemory(&si, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	si.nMin = 0;
	si.nMax = (content > 0) ? content - 1 : 0;
	si.nPage = client.Height();
	if (m_scrollY > content - client.Height())
		m_scrollY = content - client.Height();
	if (m_scrollY < 0)
		m_scrollY = 0;
	si.nPos = m_scrollY;
	SetScrollInfo(SB_VERT, &si, TRUE);
}

void CResultCardCtrl::ScrollTo(int y)
{
	CRect client;
	GetClientRect(&client);
	const int content = ContentHeight();
	int maxY = content - client.Height();
	if (maxY < 0) maxY = 0;
	if (y < 0) y = 0;
	if (y > maxY) y = maxY;
	if (y == m_scrollY)
		return;
	m_scrollY = y;
	SetScrollPos(SB_VERT, m_scrollY, TRUE);
	Invalidate(FALSE);
}

void CResultCardCtrl::SetSelection(int idx, bool ensureVisible)
{
	m_sel = idx;
	if (ensureVisible && idx >= 0)
	{
		CRect client;
		GetClientRect(&client);
		const int top = idx * m_cardH;
		const int bottom = top + m_cardH;
		if (top < m_scrollY)
			ScrollTo(top);
		else if (bottom > m_scrollY + client.Height())
			ScrollTo(bottom - client.Height());
	}
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

BOOL CResultCardCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}

void CResultCardCtrl::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	UpdateScrollbar();
}

void CResultCardCtrl::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CRect client;
	GetClientRect(&client);
	int y = m_scrollY;
	switch (nSBCode)
	{
	case SB_LINEUP:      y -= m_cardH / 3; break;
	case SB_LINEDOWN:    y += m_cardH / 3; break;
	case SB_PAGEUP:      y -= client.Height(); break;
	case SB_PAGEDOWN:    y += client.Height(); break;
	case SB_TOP:         y = 0; break;
	case SB_BOTTOM:      y = ContentHeight(); break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
	{
		SCROLLINFO si;
		::ZeroMemory(&si, sizeof(si));
		si.cbSize = sizeof(si);
		si.fMask = SIF_TRACKPOS;
		if (GetScrollInfo(SB_VERT, &si))
			y = si.nTrackPos;
		else
			y = nPos;
		break;
	}
	default:
		break;
	}
	ScrollTo(y);
	CWnd::OnVScroll(nSBCode, nPos, pScrollBar);
}

BOOL CResultCardCtrl::OnMouseWheel(UINT /*nFlags*/, short zDelta, CPoint /*pt*/)
{
	ScrollTo(m_scrollY - (zDelta / WHEEL_DELTA) * (m_cardH / 2));
	return TRUE;
}

void CResultCardCtrl::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
	SetFocus();
	const int n = m_results ? static_cast<int>(m_results->size()) : 0;
	const int idx = (point.y + m_scrollY) / m_cardH;
	if (idx >= 0 && idx < n)
	{
		SetSelection(idx, false);
		CWnd* p = GetParent();
		if (p)
			p->SendMessage(WM_GRFVIEW_CARD_SEL, static_cast<WPARAM>(idx), 0);
	}
}

cv::Mat CResultCardCtrl::MakeOverlayThumb(const GrfView::RegionResult& rr)
{
	if (m_cache == NULL)
		return cv::Mat();
	cv::Mat gray = m_cache->GetGray(rr.filePath);
	if (gray.empty())
		return cv::Mat();

	cv::Mat bgr;
	try
	{
		cv::cvtColor(gray, bgr, cv::COLOR_GRAY2BGR);
		COLORREF cr = GrfView::DefectCodeColor(rr.code);
		cv::Scalar color(GetBValue(cr), GetGValue(cr), GetRValue(cr)); // BGR
		if (!rr.contours.empty())
			cv::drawContours(bgr, rr.contours, -1, color, 1);
		// bbox 강조
		if (rr.bbox.width > 0 && rr.bbox.height > 0)
			cv::rectangle(bgr, rr.bbox, color, 1);
	}
	catch (...)
	{
		return cv::Mat();
	}
	return bgr;
}

void CResultCardCtrl::OnPaint()
{
	CPaintDC dc(this);
	CRect client;
	GetClientRect(&client);
	if (client.Width() <= 0 || client.Height() <= 0)
		return;

	CDC mem;
	mem.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, client.Width(), client.Height());
	CBitmap* pOld = mem.SelectObject(&bmp);

	try
	{
		DrawContent(&mem, client);
	}
	catch (...)
	{
	}

	dc.BitBlt(0, 0, client.Width(), client.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(pOld);
}

void CResultCardCtrl::DrawContent(CDC* pDC, const CRect& client)
{
	pDC->FillSolidRect(client, RGB(44, 44, 46));

	const int n = m_results ? static_cast<int>(m_results->size()) : 0;
	if (n == 0)
	{
		pDC->SetBkMode(TRANSPARENT);
		pDC->SetTextColor(RGB(180, 180, 180));
		CRect r = client;
		pDC->DrawText(_T("분석 결과가 없습니다. [설정] 탭에서 폴더를 열고 [분석 실행] 하세요."),
			&r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		return;
	}

	// 가시 행만 렌더
	int first = m_scrollY / m_cardH;
	int last = (m_scrollY + client.Height()) / m_cardH;
	if (first < 0) first = 0;
	if (last >= n) last = n - 1;

	for (int i = first; i <= last; ++i)
	{
		const int top = i * m_cardH - m_scrollY;
		CRect cardRc(client.left + 4, top + 3, client.right - 4, top + m_cardH - 3);
		DrawCard(pDC, cardRc, i, (i == m_sel));
	}
}

void CResultCardCtrl::DrawCard(CDC* pDC, const CRect& cardRc, int resultIdx, bool selected)
{
	if (!m_results)
		return;
	const GrfView::RegionResult& rr = (*m_results)[resultIdx];

	// 카드 배경
	pDC->FillSolidRect(cardRc, selected ? RGB(64, 74, 92) : RGB(58, 58, 62));
	CBrush border(selected ? RGB(120, 160, 230) : RGB(90, 90, 96));
	pDC->FrameRect(cardRc, &border);

	pDC->SetBkMode(TRANSPARENT);
	CFont font;
	font.CreatePointFont(90, _T("Segoe UI"));
	CFont* pOldFont = pDC->SelectObject(&font);

	// 1) 원본 썸네일
	int x = cardRc.left + kPad;
	const int y = cardRc.top + (cardRc.Height() - kThumb) / 2;
	CRect thOrig(x, y, x + kThumb, y + kThumb);
	pDC->FillSolidRect(thOrig, RGB(20, 20, 20));
	if (m_cache)
	{
		cv::Mat gray = m_cache->GetGray(rr.filePath);
		if (!gray.empty())
			GrfView::DrawMatFit(pDC, gray, thOrig, false);
	}

	// 2) 오버레이 썸네일
	x = thOrig.right + kPad;
	CRect thOv(x, y, x + kThumb, y + kThumb);
	pDC->FillSolidRect(thOv, RGB(20, 20, 20));
	cv::Mat ov = MakeOverlayThumb(rr);
	if (!ov.empty())
		GrfView::DrawMatFit(pDC, ov, thOv, true);

	// 3) 핵심 특징값 텍스트
	x = thOv.right + kPad + 4;
	const int textW = 200;
	CRect txt(x, cardRc.top + 6, x + textW, cardRc.bottom - 6);
	pDC->SetTextColor(RGB(235, 235, 235));

	CString head;
	head.Format(_T("#%d  %s"), rr.regionIndex, CString(rr.fileName.c_str()));
	CRect lineRc = txt;
	lineRc.bottom = lineRc.top + 16;
	pDC->DrawText(head, &lineRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

	pDC->SetTextColor(RGB(200, 205, 215));
	CString feats;
	feats.Format(_T("area %.0f  circ %.3f  conv %.3f\nround %.3f  aniso %.2f"),
		rr.fv.area, rr.fv.circularity, rr.fv.convexity, rr.fv.roundness, rr.fv.anisometry);
	CRect featRc = txt;
	featRc.top += 18;
	pDC->DrawText(feats, &featRc, DT_LEFT | DT_WORDBREAK);

	// 4) score 미니 막대
	const int barX = x + textW + 12;
	const int barW = 120;
	CRect barBg(barX, cardRc.top + 40, barX + barW, cardRc.top + 54);
	if (m_profileLoaded && rr.overallScore >= 0.0)
	{
		pDC->FillSolidRect(barBg, RGB(30, 30, 34));
		double s = rr.overallScore;
		if (s < 0) s = 0; if (s > 100) s = 100;
		int fillW = static_cast<int>(barW * s / 100.0);
		// 점수에 따른 색(낮음=빨강 → 높음=초록)
		int rC = static_cast<int>(220 - s * 1.4);
		int gC = static_cast<int>(80 + s * 1.2);
		if (rC < 40) rC = 40; if (rC > 220) rC = 220;
		if (gC < 80) gC = 80; if (gC > 200) gC = 200;
		CRect fillRc(barBg.left, barBg.top, barBg.left + fillW, barBg.bottom);
		pDC->FillSolidRect(fillRc, RGB(rC, gC, 90));
		CBrush bb(RGB(90, 90, 96));
		pDC->FrameRect(barBg, &bb);

		CString sc;
		sc.Format(_T("score %.0f"), rr.overallScore);
		CRect scRc(barX, cardRc.top + 22, barX + barW, cardRc.top + 38);
		pDC->SetTextColor(RGB(210, 210, 210));
		pDC->DrawText(sc, &scRc, DT_LEFT | DT_SINGLELINE);
	}
	else
	{
		CRect scRc(barX, cardRc.top + 22, barX + barW, cardRc.top + 38);
		pDC->SetTextColor(RGB(140, 140, 140));
		pDC->DrawText(_T("(no profile)"), &scRc, DT_LEFT | DT_SINGLELINE);
	}

	// 5) 분류 뱃지(코드별 고정색)
	CString code = rr.code.empty() ? CString(_T("-")) : CString(rr.code.c_str());
	COLORREF badge = GrfView::DefectCodeColor(rr.code);
	CSize ts = pDC->GetTextExtent(code);
	int bw = ts.cx + 18;
	if (bw < 48) bw = 48;
	CRect badgeRc(cardRc.right - bw - 10, cardRc.top + 40, cardRc.right - 10, cardRc.top + 60);
	pDC->FillSolidRect(badgeRc, badge);
	pDC->SetTextColor(RGB(20, 20, 20));
	pDC->DrawText(code, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	pDC->SelectObject(pOldFont);
}
