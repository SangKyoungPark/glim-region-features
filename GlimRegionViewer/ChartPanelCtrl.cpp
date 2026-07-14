// ChartPanelCtrl.cpp : 분석 탭 GDI 차트 구현
#include "stdafx.h"
#include "ChartPanelCtrl.h"

#include <algorithm>
#include <map>
#include <utility>
#include <functional>
#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace {
	const COLORREF kBg = RGB(40, 40, 44);
	const COLORREF kPanel = RGB(30, 30, 34);
	const COLORREF kAxis = RGB(140, 140, 150);
	const COLORREF kGrid = RGB(64, 64, 70);
	const COLORREF kText = RGB(220, 220, 225);
	const COLORREF kSub = RGB(160, 160, 170);

	// 묶음막대(특징별) 팔레트
	COLORREF FeatureColor(int i)
	{
		static const COLORREF pal[] = {
			RGB(90, 150, 230), RGB(240, 150, 60), RGB(120, 200, 120),
			RGB(220, 100, 110), RGB(180, 140, 220), RGB(230, 200, 80),
			RGB(90, 200, 200), RGB(220, 130, 190)
		};
		const int n = sizeof(pal) / sizeof(pal[0]);
		return pal[((i % n) + n) % n];
	}

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

CChartPanelCtrl::CChartPanelCtrl()
	: m_results(NULL)
	, m_profileLoaded(false)
	, m_histFeature("area")
{
}

CChartPanelCtrl::~CChartPanelCtrl()
{
}

BEGIN_MESSAGE_MAP(CChartPanelCtrl, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CChartPanelCtrl::CreateCtrl(CWnd* pParent, const CRect& rc, UINT id)
{
	LPCTSTR cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
		::LoadCursor(NULL, IDC_ARROW), NULL, NULL);
	return CWnd::Create(cls, _T(""), WS_CHILD | WS_VISIBLE | WS_BORDER, rc, pParent, id);
}

void CChartPanelCtrl::SetData(const std::vector<GrfView::RegionResult>* results,
	bool profileLoaded, const std::vector<std::string>& scoreFeatureNames)
{
	m_results = results;
	m_profileLoaded = profileLoaded;
	m_scoreFeatures = scoreFeatureNames;
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

void CChartPanelCtrl::SetHistogramFeature(const std::string& featureName)
{
	m_histFeature = featureName;
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

void CChartPanelCtrl::Clear()
{
	m_results = NULL;
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

BOOL CChartPanelCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}

void CChartPanelCtrl::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	Invalidate(FALSE);
}

void CChartPanelCtrl::CollectCodes(std::vector<std::string>& codesOut, std::vector<int>& countsOut) const
{
	codesOut.clear();
	countsOut.clear();
	if (!m_results)
		return;
	std::map<std::string, int> counts;
	for (size_t i = 0; i < m_results->size(); ++i)
	{
		std::string c = (*m_results)[i].code;
		if (c.empty())
			c = "-";
		counts[c] += 1;
	}
	// 카운트 내림차순 정렬
	std::vector<std::pair<int, std::string> > v;
	for (std::map<std::string, int>::const_iterator it = counts.begin(); it != counts.end(); ++it)
		v.push_back(std::make_pair(it->second, it->first));
	std::sort(v.begin(), v.end(), std::greater<std::pair<int, std::string> >());
	for (size_t i = 0; i < v.size(); ++i)
	{
		codesOut.push_back(v[i].second);
		countsOut.push_back(v[i].first);
	}
}

void CChartPanelCtrl::OnPaint()
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

void CChartPanelCtrl::DrawAll(CDC* pDC, const CRect& client)
{
	pDC->FillSolidRect(client, kBg);
	pDC->SetBkMode(TRANSPARENT);

	const int n = m_results ? static_cast<int>(m_results->size()) : 0;
	if (n == 0)
	{
		CFont font; font.CreatePointFont(100, _T("Segoe UI"));
		CFont* pf = pDC->SelectObject(&font);
		pDC->SetTextColor(kSub);
		CRect r = client;
		pDC->DrawText(_T("분석 결과가 없습니다. [설정] 탭에서 분석을 실행하세요."),
			&r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		pDC->SelectObject(pf);
		return;
	}

	CFont font; font.CreatePointFont(80, _T("Segoe UI"));
	CFont* pOldFont = pDC->SelectObject(&font);

	const int m = 6;
	const int topH = static_cast<int>(client.Height() * 0.46);

	CRect topLeft(client.left + m, client.top + m,
		client.left + client.Width() / 2 - m / 2, client.top + topH - m / 2);
	CRect topRight(client.left + client.Width() / 2 + m / 2, client.top + m,
		client.right - m, client.top + topH - m / 2);
	CRect bottom(client.left + m, client.top + topH + m / 2,
		client.right - m, client.bottom - m);

	// 패널 배경
	pDC->FillSolidRect(topLeft, kPanel);
	pDC->FillSolidRect(topRight, kPanel);
	pDC->FillSolidRect(bottom, kPanel);

	DrawCountBar(pDC, topLeft);
	DrawScoreGroupBar(pDC, topRight);
	DrawHistogram(pDC, bottom);

	pDC->SelectObject(pOldFont);
}

void CChartPanelCtrl::DrawTitle(CDC* pDC, const CRect& area, LPCTSTR title)
{
	CRect t(area.left + 6, area.top + 4, area.right - 6, area.top + 20);
	pDC->SetTextColor(kText);
	pDC->DrawText(title, &t, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
}

// ① 분류코드별 카운트 가로막대
void CChartPanelCtrl::DrawCountBar(CDC* pDC, const CRect& area)
{
	DrawTitle(pDC, area, _T("Count by code"));

	std::vector<std::string> codes;
	std::vector<int> counts;
	CollectCodes(codes, counts);
	if (codes.empty())
		return;

	const int plotL = area.left + 78;   // 좌측 라벨 공간
	const int plotR = area.right - 40;   // 우측 값 공간
	const int plotT = area.top + 26;
	const int plotB = area.bottom - 12;
	if (plotR <= plotL || plotB <= plotT)
		return;

	int maxC = 1;
	for (size_t i = 0; i < counts.size(); ++i)
		maxC = (counts[i] > maxC) ? counts[i] : maxC;

	const int rows = static_cast<int>(codes.size());
	const int rowH = (plotB - plotT) / rows;
	const int barH = (rowH > 8) ? rowH - 6 : rowH;

	CPen axisPen(PS_SOLID, 1, kAxis);
	CPen* pOldPen = pDC->SelectObject(&axisPen);
	pDC->MoveTo(plotL, plotT); pDC->LineTo(plotL, plotB); // y축
	pDC->SelectObject(pOldPen);

	for (int i = 0; i < rows; ++i)
	{
		const int y = plotT + i * rowH;
		CRect labelRc(area.left + 4, y, plotL - 4, y + rowH);
		pDC->SetTextColor(kSub);
		pDC->DrawText(CString(codes[i].c_str()), &labelRc,
			DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

		int bw = static_cast<int>((double)(plotR - plotL) * counts[i] / maxC);
		if (bw < 1) bw = 1;
		CRect bar(plotL + 1, y + 3, plotL + 1 + bw, y + 3 + barH);
		pDC->FillSolidRect(bar, GrfView::DefectCodeColor(codes[i] == "-" ? std::string() : codes[i]));

		CString cnt; cnt.Format(_T("%d"), counts[i]);
		CRect cntRc(bar.right + 4, y, plotR + 38, y + rowH);
		pDC->SetTextColor(kText);
		pDC->DrawText(cnt, &cntRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	}
}

// ② 코드별 score 평균 묶음막대(코드 × score 특징)
void CChartPanelCtrl::DrawScoreGroupBar(CDC* pDC, const CRect& area)
{
	DrawTitle(pDC, area, _T("Avg score by code (grouped)"));

	if (!m_profileLoaded || m_scoreFeatures.empty())
	{
		CRect r(area.left + 6, area.top + 24, area.right - 6, area.bottom - 6);
		pDC->SetTextColor(kSub);
		pDC->DrawText(_T("프로파일을 선택하면 표시됩니다."), &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_WORDBREAK);
		return;
	}

	// 코드 목록(카운트순)
	std::vector<std::string> codes;
	std::vector<int> counts;
	CollectCodes(codes, counts);
	if (codes.empty())
		return;
	if (codes.size() > 6)
		codes.resize(6); // 가독성 상한

	const size_t nf = m_scoreFeatures.size();

	// code × feature 평균 집계
	// sum[codeIdx][featIdx], cnt[codeIdx]
	std::vector<std::vector<double> > sum(codes.size(), std::vector<double>(nf, 0.0));
	std::vector<int> cnt(codes.size(), 0);
	std::map<std::string, int> codeIndex;
	for (size_t i = 0; i < codes.size(); ++i)
		codeIndex[codes[i]] = static_cast<int>(i);

	for (size_t i = 0; i < m_results->size(); ++i)
	{
		std::string c = (*m_results)[i].code;
		if (c.empty()) c = "-";
		std::map<std::string, int>::iterator it = codeIndex.find(c);
		if (it == codeIndex.end())
			continue;
		const int ci = it->second;
		cnt[ci] += 1;
		const std::map<std::string, double>& sc = (*m_results)[i].scores;
		for (size_t f = 0; f < nf; ++f)
		{
			std::map<std::string, double>::const_iterator sit = sc.find(m_scoreFeatures[f]);
			if (sit != sc.end())
				sum[ci][f] += sit->second;
		}
	}

	const int plotL = area.left + 30;
	const int plotR = area.right - 8;
	const int plotT = area.top + 26;
	const int plotB = area.bottom - 42; // 하단 코드 라벨 + 범례
	if (plotR <= plotL || plotB <= plotT)
		return;

	// y축 0..100 그리드
	CPen gridPen(PS_SOLID, 1, kGrid);
	CPen axisPen(PS_SOLID, 1, kAxis);
	CPen* pOldPen = pDC->SelectObject(&axisPen);
	pDC->MoveTo(plotL, plotT); pDC->LineTo(plotL, plotB);
	pDC->MoveTo(plotL, plotB); pDC->LineTo(plotR, plotB);
	pDC->SelectObject(&gridPen);
	pDC->SetTextColor(kSub);
	for (int g = 0; g <= 100; g += 25)
	{
		int y = plotB - static_cast<int>((plotB - plotT) * g / 100.0);
		pDC->MoveTo(plotL, y); pDC->LineTo(plotR, y);
		CString lab; lab.Format(_T("%d"), g);
		CRect lr(area.left + 2, y - 7, plotL - 2, y + 7);
		pDC->DrawText(lab, &lr, DT_RIGHT | DT_SINGLELINE);
	}
	pDC->SelectObject(pOldPen);

	const int groups = static_cast<int>(codes.size());
	const int groupW = (plotR - plotL) / groups;
	const int barW = (groupW - 6) / static_cast<int>(nf);
	const int bw = (barW < 3) ? 3 : barW;

	for (int gi = 0; gi < groups; ++gi)
	{
		const int gx = plotL + gi * groupW + 3;
		for (size_t f = 0; f < nf; ++f)
		{
			double avg = (cnt[gi] > 0) ? (sum[gi][f] / cnt[gi]) : 0.0;
			if (avg < 0) avg = 0; if (avg > 100) avg = 100;
			int h = static_cast<int>((plotB - plotT) * avg / 100.0);
			int x = gx + static_cast<int>(f) * bw;
			CRect bar(x, plotB - h, x + bw - 1, plotB);
			pDC->FillSolidRect(bar, FeatureColor(static_cast<int>(f)));
		}
		// 코드 라벨
		CRect lr(plotL + gi * groupW, plotB + 2, plotL + (gi + 1) * groupW, plotB + 16);
		pDC->SetTextColor(kSub);
		pDC->DrawText(CString(codes[gi].c_str()), &lr, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
	}

	// 범례(특징별 색)
	int lx = plotL;
	const int ly = area.bottom - 22;
	for (size_t f = 0; f < nf; ++f)
	{
		CRect sw(lx, ly + 2, lx + 10, ly + 12);
		pDC->FillSolidRect(sw, FeatureColor(static_cast<int>(f)));
		CString name(m_scoreFeatures[f].c_str());
		CSize ts = pDC->GetTextExtent(name);
		CRect tr(sw.right + 3, ly, sw.right + 3 + ts.cx + 4, ly + 14);
		pDC->SetTextColor(kSub);
		pDC->DrawText(name, &tr, DT_LEFT | DT_SINGLELINE);
		lx = tr.right + 8;
		if (lx > plotR - 40)
			break;
	}
}

// ③ 특징값 히스토그램(선택 특징, 코드별 색 누적)
void CChartPanelCtrl::DrawHistogram(CDC* pDC, const CRect& area)
{
	CString title;
	title.Format(_T("Histogram: %s (stacked by code)"), CString(m_histFeature.c_str()));
	DrawTitle(pDC, area, title);

	if (!m_results || m_results->empty())
		return;

	// 값 수집 + min/max
	std::vector<double> vals(m_results->size());
	double vmin = 1e300, vmax = -1e300;
	for (size_t i = 0; i < m_results->size(); ++i)
	{
		double v = (*m_results)[i].fv.GetByName(m_histFeature);
		vals[i] = v;
		if (v < vmin) vmin = v;
		if (v > vmax) vmax = v;
	}
	if (vmax <= vmin)
		vmax = vmin + 1.0;

	// 코드 목록(색 범례)
	std::vector<std::string> codes;
	std::vector<int> counts;
	CollectCodes(codes, counts);
	std::map<std::string, int> codeIndex;
	for (size_t i = 0; i < codes.size(); ++i)
		codeIndex[codes[i]] = static_cast<int>(i);

	const int bins = 24;
	// bins[bin][codeIdx] = count
	std::vector<std::vector<int> > hist(bins, std::vector<int>(codes.size(), 0));
	std::vector<int> binTotal(bins, 0);
	for (size_t i = 0; i < m_results->size(); ++i)
	{
		int b = static_cast<int>((vals[i] - vmin) / (vmax - vmin) * bins);
		if (b < 0) b = 0; if (b >= bins) b = bins - 1;
		std::string c = (*m_results)[i].code;
		if (c.empty()) c = "-";
		int ci = codeIndex.count(c) ? codeIndex[c] : 0;
		hist[b][ci] += 1;
		binTotal[b] += 1;
	}
	int maxBin = 1;
	for (int b = 0; b < bins; ++b)
		maxBin = (binTotal[b] > maxBin) ? binTotal[b] : maxBin;

	const int plotL = area.left + 34;
	const int plotR = area.right - 10;
	const int plotT = area.top + 26;
	const int plotB = area.bottom - 40;
	if (plotR <= plotL || plotB <= plotT)
		return;

	// 축 + y 그리드
	CPen gridPen(PS_SOLID, 1, kGrid);
	CPen axisPen(PS_SOLID, 1, kAxis);
	CPen* pOldPen = pDC->SelectObject(&axisPen);
	pDC->MoveTo(plotL, plotT); pDC->LineTo(plotL, plotB);
	pDC->MoveTo(plotL, plotB); pDC->LineTo(plotR, plotB);
	pDC->SelectObject(&gridPen);
	pDC->SetTextColor(kSub);
	for (int g = 0; g <= 4; ++g)
	{
		int val = maxBin * g / 4;
		int y = plotB - (plotB - plotT) * g / 4;
		pDC->MoveTo(plotL, y); pDC->LineTo(plotR, y);
		CString lab; lab.Format(_T("%d"), val);
		CRect lr(area.left + 2, y - 7, plotL - 2, y + 7);
		pDC->DrawText(lab, &lr, DT_RIGHT | DT_SINGLELINE);
	}
	pDC->SelectObject(pOldPen);

	const int barW = (plotR - plotL) / bins;
	for (int b = 0; b < bins; ++b)
	{
		int x = plotL + b * barW;
		int yBase = plotB;
		for (size_t ci = 0; ci < codes.size(); ++ci)
		{
			int cnt = hist[b][ci];
			if (cnt <= 0)
				continue;
			int h = static_cast<int>((double)(plotB - plotT) * cnt / maxBin);
			CRect seg(x + 1, yBase - h, x + barW - 1, yBase);
			pDC->FillSolidRect(seg, GrfView::DefectCodeColor(codes[ci] == "-" ? std::string() : codes[ci]));
			yBase -= h;
		}
	}

	// x축 min/max 라벨
	pDC->SetTextColor(kSub);
	CRect xl(plotL, plotB + 2, plotL + 80, plotB + 16);
	pDC->DrawText(FmtNum(vmin), &xl, DT_LEFT | DT_SINGLELINE);
	CRect xr(plotR - 80, plotB + 2, plotR, plotB + 16);
	pDC->DrawText(FmtNum(vmax), &xr, DT_RIGHT | DT_SINGLELINE);

	// 범례(코드별 색)
	int lx = plotL;
	const int ly = area.bottom - 20;
	for (size_t ci = 0; ci < codes.size(); ++ci)
	{
		CRect sw(lx, ly + 2, lx + 10, ly + 12);
		pDC->FillSolidRect(sw, GrfView::DefectCodeColor(codes[ci] == "-" ? std::string() : codes[ci]));
		CString name(codes[ci].c_str());
		CSize ts = pDC->GetTextExtent(name);
		CRect tr(sw.right + 3, ly, sw.right + 3 + ts.cx + 4, ly + 14);
		pDC->SetTextColor(kSub);
		pDC->DrawText(name, &tr, DT_LEFT | DT_SINGLELINE);
		lx = tr.right + 8;
		if (lx > plotR - 40)
			break;
	}
}
