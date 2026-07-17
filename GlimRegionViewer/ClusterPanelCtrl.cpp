// ClusterPanelCtrl.cpp : 군집 탭 산점도/범례 GDI 렌더 구현
#include "stdafx.h"
#include "ClusterPanelCtrl.h"

#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace {
	const COLORREF kBg = RGB(40, 40, 44);
	const COLORREF kAxis = RGB(140, 140, 150);
	const COLORREF kGrid = RGB(64, 64, 70);
	const COLORREF kText = RGB(220, 220, 225);
	const COLORREF kSub = RGB(160, 160, 170);

	const int kLegendW = 190;   // 우측 범례 폭
	const int kPad = 10;

	CString FmtNum(double v)
	{
		CString s;
		if (fabs(v) >= 1000.0)
			s.Format(_T("%.0f"), v);
		else if (fabs(v) >= 10.0)
			s.Format(_T("%.1f"), v);
		else
			s.Format(_T("%.3f"), v);
		return s;
	}
}

COLORREF CClusterPanelCtrl::ClusterColor(int clusterId)
{
	// 군집 팔레트(ChartPanelCtrl::FeatureColor 와 다른 톤, 시인성 우선)
	static const COLORREF pal[] = {
		RGB(80, 160, 240), RGB(240, 130, 60), RGB(110, 210, 110),
		RGB(230, 90, 100), RGB(190, 130, 230), RGB(235, 205, 70),
		RGB(80, 210, 210), RGB(230, 120, 195), RGB(150, 150, 240),
		RGB(170, 220, 90)
	};
	const int n = sizeof(pal) / sizeof(pal[0]);
	if (clusterId < 0)
		return kSub; // 미할당/노이즈
	return pal[clusterId % n];
}

CClusterPanelCtrl::CClusterPanelCtrl()
	: m_results(NULL)
	, m_labels(NULL)
	, m_clusterSizes(NULL)
	, m_k(0)
	, m_silhouette(0.0)
	, m_xFeature("area")
	, m_yFeature("circularity")
{
}

CClusterPanelCtrl::~CClusterPanelCtrl()
{
}

BEGIN_MESSAGE_MAP(CClusterPanelCtrl, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CClusterPanelCtrl::CreateCtrl(CWnd* pParent, const CRect& rc, UINT id)
{
	LPCTSTR cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
		::LoadCursor(NULL, IDC_ARROW), NULL, NULL);
	return CWnd::Create(cls, _T(""), WS_CHILD | WS_VISIBLE | WS_BORDER, rc, pParent, id);
}

void CClusterPanelCtrl::SetData(const std::vector<GrfView::RegionResult>* results,
	const std::vector<int>* labels, int k, double silhouette,
	const std::vector<int>* clusterSizes)
{
	m_results = results;
	m_labels = labels;
	m_k = k;
	m_silhouette = silhouette;
	m_clusterSizes = clusterSizes;
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

void CClusterPanelCtrl::SetAxes(const std::string& xFeature, const std::string& yFeature)
{
	m_xFeature = xFeature;
	m_yFeature = yFeature;
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

void CClusterPanelCtrl::Clear()
{
	m_results = NULL;
	m_labels = NULL;
	m_clusterSizes = NULL;
	m_k = 0;
	m_silhouette = 0.0;
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

BOOL CClusterPanelCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}

void CClusterPanelCtrl::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	Invalidate(FALSE);
}

void CClusterPanelCtrl::OnPaint()
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
		DrawAll(&mem, client);
	}
	catch (...)
	{
	}

	dc.BitBlt(0, 0, client.Width(), client.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(pOld);
}

void CClusterPanelCtrl::DrawMessage(CDC* pDC, const CRect& client, LPCTSTR text)
{
	CFont font; font.CreatePointFont(100, _T("Segoe UI"));
	CFont* pf = pDC->SelectObject(&font);
	pDC->SetTextColor(kSub);
	CRect r = client;
	pDC->DrawText(text, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	pDC->SelectObject(pf);
}

void CClusterPanelCtrl::DrawAll(CDC* pDC, const CRect& client)
{
	pDC->FillSolidRect(client, kBg);
	pDC->SetBkMode(TRANSPARENT);

	if (!m_results || m_results->empty())
	{
		DrawMessage(pDC, client,
			_T("분석 결과가 없습니다. [설정] 탭에서 분석을 실행하세요."));
		return;
	}
	if (!m_labels || m_labels->size() != m_results->size())
	{
		DrawMessage(pDC, client,
			_T("[군집화 실행] 버튼을 눌러 군집화를 수행하세요."));
		return;
	}

	CFont font; font.CreatePointFont(80, _T("Segoe UI"));
	CFont* pOldFont = pDC->SelectObject(&font);

	CRect scatter = client;
	scatter.DeflateRect(kPad, kPad);
	scatter.right -= kLegendW;

	CRect legend = client;
	legend.DeflateRect(kPad, kPad);
	legend.left = legend.right - kLegendW + kPad;

	DrawScatter(pDC, scatter);
	DrawLegend(pDC, legend);

	pDC->SelectObject(pOldFont);
}

void CClusterPanelCtrl::DrawScatter(CDC* pDC, const CRect& area)
{
	// 타이틀
	CString title;
	title.Format(_T("Scatter: %s (X) vs %s (Y)"),
		(LPCTSTR)CString(m_xFeature.c_str()), (LPCTSTR)CString(m_yFeature.c_str()));
	pDC->SetTextColor(kText);
	CRect tr(area.left, area.top, area.right, area.top + 18);
	pDC->DrawText(title, &tr, DT_LEFT | DT_SINGLELINE);

	// 플롯 영역(축 레이블 여백 확보)
	CRect plot(area.left + 62, area.top + 24, area.right - 8, area.bottom - 34);
	if (plot.Width() <= 10 || plot.Height() <= 10)
		return;

	// 값 수집 + 범위
	const size_t n = m_results->size();
	std::vector<double> xs(n), ys(n);
	double xmin = 1e300, xmax = -1e300, ymin = 1e300, ymax = -1e300;
	for (size_t i = 0; i < n; ++i)
	{
		const double x = (*m_results)[i].fv.GetByName(m_xFeature);
		const double y = (*m_results)[i].fv.GetByName(m_yFeature);
		xs[i] = x; ys[i] = y;
		if (x < xmin) xmin = x;
		if (x > xmax) xmax = x;
		if (y < ymin) ymin = y;
		if (y > ymax) ymax = y;
	}
	if (xmax <= xmin) xmax = xmin + 1.0;
	if (ymax <= ymin) ymax = ymin + 1.0;

	// 배경/그리드(4분할) + 축
	CPen gridPen(PS_SOLID, 1, kGrid);
	CPen axisPen(PS_SOLID, 1, kAxis);
	CPen* pOldPen = pDC->SelectObject(&gridPen);
	for (int g = 1; g < 4; ++g)
	{
		const int gx = plot.left + plot.Width() * g / 4;
		const int gy = plot.top + plot.Height() * g / 4;
		pDC->MoveTo(gx, plot.top);    pDC->LineTo(gx, plot.bottom);
		pDC->MoveTo(plot.left, gy);   pDC->LineTo(plot.right, gy);
	}
	pDC->SelectObject(&axisPen);
	pDC->MoveTo(plot.left, plot.top);     pDC->LineTo(plot.left, plot.bottom);
	pDC->LineTo(plot.right, plot.bottom);

	// 축 눈금 레이블(min/max)
	pDC->SetTextColor(kSub);
	CRect lx0(plot.left - 4, plot.bottom + 4, plot.left + 90, plot.bottom + 20);
	pDC->DrawText(FmtNum(xmin), &lx0, DT_LEFT | DT_SINGLELINE);
	CRect lx1(plot.right - 90, plot.bottom + 4, plot.right + 4, plot.bottom + 20);
	pDC->DrawText(FmtNum(xmax), &lx1, DT_RIGHT | DT_SINGLELINE);
	CRect ly0(area.left, plot.bottom - 14, plot.left - 6, plot.bottom + 2);
	pDC->DrawText(FmtNum(ymin), &ly0, DT_RIGHT | DT_SINGLELINE);
	CRect ly1(area.left, plot.top - 2, plot.left - 6, plot.top + 14);
	pDC->DrawText(FmtNum(ymax), &ly1, DT_RIGHT | DT_SINGLELINE);

	// 점 렌더(작은 사각형, cluster 색)
	for (size_t i = 0; i < n; ++i)
	{
		const int px = plot.left +
			static_cast<int>((xs[i] - xmin) / (xmax - xmin) * (plot.Width() - 1));
		const int py = plot.bottom -
			static_cast<int>((ys[i] - ymin) / (ymax - ymin) * (plot.Height() - 1));
		const int label = (i < m_labels->size()) ? (*m_labels)[i] : -1;
		const COLORREF c = ClusterColor(label);
		CRect pt(px - 2, py - 2, px + 3, py + 3);
		pDC->FillSolidRect(&pt, c);
	}

	pDC->SelectObject(pOldPen);
}

void CClusterPanelCtrl::DrawLegend(CDC* pDC, const CRect& area)
{
	pDC->SetTextColor(kText);
	CString head;
	head.Format(_T("K = %d"), m_k);
	CRect hr(area.left, area.top, area.right, area.top + 18);
	pDC->DrawText(head, &hr, DT_LEFT | DT_SINGLELINE);

	CString sil;
	sil.Format(_T("Silhouette = %.3f"), m_silhouette);
	CRect sr(area.left, area.top + 20, area.right, area.top + 38);
	pDC->SetTextColor(kSub);
	pDC->DrawText(sil, &sr, DT_LEFT | DT_SINGLELINE);

	// 군집별 색 상자 + 멤버 수
	int y = area.top + 48;
	for (int c = 0; c < m_k && y + 18 <= area.bottom; ++c, y += 22)
	{
		CRect box(area.left, y + 2, area.left + 14, y + 16);
		pDC->FillSolidRect(&box, ClusterColor(c));

		int size = 0;
		if (m_clusterSizes && c < static_cast<int>(m_clusterSizes->size()))
			size = (*m_clusterSizes)[c];

		CString item;
		item.Format(_T("Cluster %d  (n=%d)"), c, size);
		CRect ir(area.left + 20, y, area.right, y + 18);
		pDC->SetTextColor(kText);
		pDC->DrawText(item, &ir, DT_LEFT | DT_SINGLELINE);
	}
}
