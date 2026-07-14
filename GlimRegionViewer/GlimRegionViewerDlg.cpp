// GlimRegionViewerDlg.cpp : 메인 다이얼로그(탭 기반 분석 대시보드) 구현
#include "stdafx.h"
#include "GlimRegionViewer.h"
#include "GlimRegionViewerDlg.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <map>
#include <shlobj.h>   // SHBrowseForFolder

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ------------------------------------------------------------------
// 레이아웃 상수(픽셀). 클라이언트 1180 x 780 기준.
// ------------------------------------------------------------------
namespace {
	const int kClientW = 1180;
	const int kClientH = 780;

	const int kTabX = 8;
	const int kTabY = 6;
	const int kTabR = kClientW - 8;   // 1172
	const int kTabB = kClientH - 26;  // 754

	// 탭 내부 콘텐츠 영역
	const int kCX = 18;
	const int kCY = 44;               // 탭 버튼(상단) 아래
	const int kCR = kClientW - 18;    // 1162
	const int kCB = kClientH - 40;    // 740

	CString ToCStr(const std::string& s) { return CString(s.c_str()); }
	std::string ToStd(const CString& s) { return std::string((LPCTSTR)s); }

	CString GetExeDir()
	{
		TCHAR path[MAX_PATH] = { 0 };
		::GetModuleFileName(NULL, path, MAX_PATH);
		CString p(path);
		int pos = p.ReverseFind(_T('\\'));
		if (pos >= 0)
			p = p.Left(pos);
		return p;
	}
}

// ------------------------------------------------------------------
CGlimRegionViewerDlg::CGlimRegionViewerDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_GLIMREGIONVIEWER_DIALOG, pParent)
	, m_curTab(TAB_SETTINGS)
	, m_curIndex(-1)
	, m_thumbCache(96)
	, m_profileLoaded(false)
	, m_xScale(1.0)
	, m_yScale(1.0)
	, m_zoom(4)
	, m_showOverlay(true)
	, m_highlightRegion(-1)
{
	m_hIcon = AfxGetApp()->LoadStandardIcon(IDI_APPLICATION);
	// 결과 탭 상세 이미지 영역(좌:카드 / 우:상세)
	m_imgFrameRect = CRect(712, 76, kCR, 376);
}

void CGlimRegionViewerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CGlimRegionViewerDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_OPEN_IMAGE, &CGlimRegionViewerDlg::OnBnClickedOpenImage)
	ON_BN_CLICKED(IDC_BTN_OPEN_FOLDER, &CGlimRegionViewerDlg::OnBnClickedOpenFolder)
	ON_BN_CLICKED(IDC_BTN_EXPORT_CSV, &CGlimRegionViewerDlg::OnBnClickedExportCsv)
	ON_BN_CLICKED(IDC_CHECK_OVERLAY, &CGlimRegionViewerDlg::OnBnClickedOverlay)
	ON_BN_CLICKED(IDC_BTN_ANALYZE, &CGlimRegionViewerDlg::OnBnClickedAnalyze)
	ON_CBN_SELCHANGE(IDC_COMBO_ZOOM, &CGlimRegionViewerDlg::OnZoomChanged)
	ON_CBN_SELCHANGE(IDC_COMBO_PROFILE, &CGlimRegionViewerDlg::OnProfileChanged)
	ON_CBN_SELCHANGE(IDC_COMBO_BINARIZE, &CGlimRegionViewerDlg::OnBinarizeChanged)
	ON_CBN_SELCHANGE(IDC_COMBO_HISTFEAT, &CGlimRegionViewerDlg::OnHistFeatChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_TH, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_OFFSET, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_WK_KERNEL, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_WK_BLUR, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_WK_RESP, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_XSCALE, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_YSCALE, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FILES, &CGlimRegionViewerDlg::OnFileListItemChanged)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FEATURES, &CGlimRegionViewerDlg::OnFeatureListItemChanged)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_MAIN, &CGlimRegionViewerDlg::OnTabSelChange)
	ON_MESSAGE(WM_GRFVIEW_CARD_SEL, &CGlimRegionViewerDlg::OnCardSelected)
END_MESSAGE_MAP()

// ------------------------------------------------------------------
BOOL CGlimRegionViewerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	CRect wr, cr;
	GetWindowRect(&wr);
	GetClientRect(&cr);
	const int frameW = wr.Width() - cr.Width();
	const int frameH = wr.Height() - cr.Height();
	SetWindowPos(NULL, 0, 0, kClientW + frameW, kClientH + frameH,
		SWP_NOMOVE | SWP_NOZORDER);
	CenterWindow();

	try
	{
		SetupTabs();
		SetupControls();
		PopulateProfileCombo();
		PopulateHistFeatCombo();
		BuildPageControlLists();
		ShowPage(TAB_SETTINGS);
	}
	catch (...)
	{
		SetStatus(_T("초기화 중 오류가 발생했습니다."));
	}

	SetStatus(_T("Ready. [설정] 탭에서 폴더/이미지를 열고 [분석 실행] 하세요."));
	return TRUE;
}

void CGlimRegionViewerDlg::SetupTabs()
{
	m_tab.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_TABS,
		CRect(kTabX, kTabY, kTabR, kTabB), this, IDC_TAB_MAIN);
	m_tab.SetFont(GetFont());
	m_tab.InsertItem(TAB_HOME, _T("홈"));
	m_tab.InsertItem(TAB_SETTINGS, _T("설정"));
	m_tab.InsertItem(TAB_RESULTS, _T("결과"));
	m_tab.InsertItem(TAB_ANALYSIS, _T("분석"));
	m_tab.SetCurSel(TAB_SETTINGS);
}

// 동적 컨트롤 생성
void CGlimRegionViewerDlg::SetupControls()
{
	CFont* pFont = GetFont();

	// ========== 홈 탭 ==========
	m_staticHome.Create(_T(""), WS_CHILD | SS_LEFT,
		CRect(kCX, kCY, kCR, kCB), this, IDC_STATIC_HOME);
	m_staticHome.SetFont(pFont);

	// ========== 설정 탭 : Row1 ==========
	struct { int id; LPCTSTR text; int x; int w; } btns[] = {
		{ IDC_BTN_OPEN_IMAGE,  _T("Open Image"),  18,  95 },
		{ IDC_BTN_OPEN_FOLDER, _T("Open Folder"), 118, 95 },
	};
	for (int i = 0; i < 2; ++i)
	{
		CButton* b = new CButton();
		b->Create(btns[i].text, WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
			CRect(btns[i].x, kCY, btns[i].x + btns[i].w, kCY + 24), this, btns[i].id);
		b->SetFont(pFont);
	}

	// Profile 라벨 + 콤보
	{
		CStatic* lbl = new CStatic();
		lbl->Create(_T("Profile"), WS_CHILD | SS_CENTERIMAGE,
			CRect(222, kCY, 264, kCY + 24), this, IDC_STATIC_PROFILE_LABEL);
		lbl->SetFont(pFont);
		m_comboProfile.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
			CRect(266, kCY, 416, kCY + 200), this, IDC_COMBO_PROFILE);
		m_comboProfile.SetFont(pFont);
	}

	// Threads 라벨 + 에딧
	{
		CStatic* lbl = new CStatic();
		lbl->Create(_T("Threads"), WS_CHILD | SS_CENTERIMAGE,
			CRect(426, kCY, 478, kCY + 24), this, IDC_STATIC_THREADS_LABEL);
		lbl->SetFont(pFont);
		m_editThreads.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER,
			CRect(480, kCY, 520, kCY + 22), this, IDC_EDIT_THREADS);
		m_editThreads.SetFont(pFont);
		m_editThreads.SetWindowText(_T("0"));
	}

	// Binarize 라벨 + 콤보
	{
		CStatic* lbl = new CStatic();
		lbl->Create(_T("Binarize"), WS_CHILD | SS_CENTERIMAGE,
			CRect(532, kCY, 586, kCY + 24), this, IDC_STATIC_BINARIZE_LABEL);
		lbl->SetFont(pFont);
		m_comboBinarize.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
			CRect(588, kCY, 728, kCY + 220), this, IDC_COMBO_BINARIZE);
		m_comboBinarize.SetFont(pFont);
		const LPCTSTR modes[] = {
			_T("Bright(127)"), _T("Dark(127)"), _T("Dark auto"), _T("Bright auto"),
			_T("Binary"), _T("Wrinkle") };
		for (int i = 0; i < 6; ++i)
			m_comboBinarize.AddString(modes[i]);
		m_comboBinarize.SetCurSel(0);
	}

	// ========== 설정 탭 : Row2 (파라미터/스케일/분석) ==========
	const int r2 = 74;
	// 고정 임계값(TH) — 모드 0,1
	{
		CStatic* lbl = new CStatic();
		lbl->Create(_T("TH"), WS_CHILD | SS_CENTERIMAGE, CRect(18, r2, 44, r2 + 22), this, IDC_STATIC_TH_LABEL);
		lbl->SetFont(pFont);
		m_editTh.Create(WS_CHILD | WS_TABSTOP | WS_BORDER, CRect(46, r2, 96, r2 + 22), this, IDC_EDIT_TH);
		m_editTh.SetFont(pFont);
		m_editTh.SetWindowText(_T("127"));
	}
	// auto offset — 모드 2,3
	{
		CStatic* lbl = new CStatic();
		lbl->Create(_T("Offset"), WS_CHILD | SS_CENTERIMAGE, CRect(18, r2, 58, r2 + 22), this, IDC_STATIC_OFFSET_LABEL);
		lbl->SetFont(pFont);
		m_editOffset.Create(WS_CHILD | WS_TABSTOP | WS_BORDER, CRect(60, r2, 110, r2 + 22), this, IDC_EDIT_OFFSET);
		m_editOffset.SetFont(pFont);
		m_editOffset.SetWindowText(_T("20"));
	}
	// wrinkle kernel / blur / response — 모드 5
	{
		CStatic* lk = new CStatic();
		lk->Create(_T("Kernel"), WS_CHILD | SS_CENTERIMAGE, CRect(18, r2, 56, r2 + 22), this, IDC_STATIC_WK_LABEL);
		lk->SetFont(pFont);
		m_editWkKernel.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, CRect(58, r2, 98, r2 + 22), this, IDC_EDIT_WK_KERNEL);
		m_editWkKernel.SetFont(pFont);
		m_editWkKernel.SetWindowText(_T("15"));

		CStatic* lb = new CStatic();
		lb->Create(_T("Blur"), WS_CHILD | SS_CENTERIMAGE, CRect(104, r2, 136, r2 + 22), this, IDC_STATIC_WB_LABEL);
		lb->SetFont(pFont);
		m_editWkBlur.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, CRect(138, r2, 178, r2 + 22), this, IDC_EDIT_WK_BLUR);
		m_editWkBlur.SetFont(pFont);
		m_editWkBlur.SetWindowText(_T("31"));

		CStatic* lr = new CStatic();
		lr->Create(_T("Resp"), WS_CHILD | SS_CENTERIMAGE, CRect(184, r2, 216, r2 + 22), this, IDC_STATIC_WR_LABEL);
		lr->SetFont(pFont);
		m_editWkResp.Create(WS_CHILD | WS_TABSTOP | WS_BORDER, CRect(218, r2, 262, r2 + 22), this, IDC_EDIT_WK_RESP);
		m_editWkResp.SetFont(pFont);
		m_editWkResp.SetWindowText(_T("4"));
	}
	// X/Y Scale
	{
		CStatic* lx = new CStatic();
		lx->Create(_T("X Scale"), WS_CHILD | SS_CENTERIMAGE, CRect(360, r2, 412, r2 + 22), this, IDC_STATIC_XSCALE_LABEL);
		lx->SetFont(pFont);
		m_editXScale.Create(WS_CHILD | WS_TABSTOP | WS_BORDER, CRect(414, r2, 464, r2 + 22), this, IDC_EDIT_XSCALE);
		m_editXScale.SetFont(pFont);
		m_editXScale.SetWindowText(_T("1.0"));

		CStatic* ly = new CStatic();
		ly->Create(_T("Y Scale"), WS_CHILD | SS_CENTERIMAGE, CRect(470, r2, 522, r2 + 22), this, IDC_STATIC_YSCALE_LABEL);
		ly->SetFont(pFont);
		m_editYScale.Create(WS_CHILD | WS_TABSTOP | WS_BORDER, CRect(524, r2, 574, r2 + 22), this, IDC_EDIT_YSCALE);
		m_editYScale.SetFont(pFont);
		m_editYScale.SetWindowText(_T("1.0"));
	}
	// 분석 실행 버튼
	{
		CButton* b = new CButton();
		b->Create(_T("분석 실행"), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
			CRect(kCR - 150, r2, kCR, r2 + 24), this, IDC_BTN_ANALYZE);
		b->SetFont(pFont);
	}

	// ========== 설정 탭 : 파일 리스트 + 미리보기 ==========
	m_listFiles.Create(WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
		CRect(18, 104, 258, kCB), this, IDC_LIST_FILES);
	m_listFiles.SetFont(pFont);
	m_listFiles.SetExtendedStyle(m_listFiles.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_listFiles.InsertColumn(0, _T("File"), LVCFMT_LEFT, 216);

	m_preview.CreatePanel(this, CRect(270, 104, kCR, kCB), IDC_PREVIEW_PANEL);

	// ========== 결과 탭 : Row1 ==========
	{
		CStatic* lbl = new CStatic();
		lbl->Create(_T("Zoom"), WS_CHILD | SS_CENTERIMAGE, CRect(18, kCY, 54, kCY + 24), this, IDC_STATIC_ZOOM_LABEL);
		lbl->SetFont(pFont);
		m_comboZoom.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
			CRect(56, kCY, 122, kCY + 200), this, IDC_COMBO_ZOOM);
		m_comboZoom.SetFont(pFont);
		const LPCTSTR zooms[] = { _T("1x"), _T("2x"), _T("4x"), _T("8x"), _T("16x") };
		for (int i = 0; i < 5; ++i)
			m_comboZoom.AddString(zooms[i]);
		m_comboZoom.SetCurSel(2);

		CButton* chk = new CButton();
		chk->Create(_T("Show overlay"), WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
			CRect(132, kCY, 242, kCY + 24), this, IDC_CHECK_OVERLAY);
		chk->SetFont(pFont);
		chk->SetCheck(BST_CHECKED);

		CButton* b = new CButton();
		b->Create(_T("Export CSV"), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
			CRect(250, kCY, 360, kCY + 24), this, IDC_BTN_EXPORT_CSV);
		b->SetFont(pFont);
	}

	// 결과 탭 : 카드 리스트(좌) + 상세 이미지(우 상단) + 상세 특징값(우 하단)
	m_cards.CreateCtrl(this, CRect(18, 76, 700, kCB), IDC_CARD_LIST);
	m_cards.SetData(&m_results, &m_thumbCache, m_profileLoaded);

	m_detailView.CreateCtrl(this, m_imgFrameRect, IDC_IMAGE_VIEW);
	m_detailView.SetZoom(m_zoom);

	m_listFeatures.Create(WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
		CRect(712, 382, kCR, kCB), this, IDC_LIST_FEATURES);
	m_listFeatures.SetFont(pFont);
	m_listFeatures.SetExtendedStyle(m_listFeatures.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_listFeatures.InsertColumn(0, _T("Idx"), LVCFMT_RIGHT, 34);
	m_listFeatures.InsertColumn(1, _T("Area"), LVCFMT_RIGHT, 60);
	m_listFeatures.InsertColumn(2, _T("Circ"), LVCFMT_RIGHT, 54);
	m_listFeatures.InsertColumn(3, _T("Conv"), LVCFMT_RIGHT, 54);
	m_listFeatures.InsertColumn(4, _T("Round"), LVCFMT_RIGHT, 56);
	m_listFeatures.InsertColumn(5, _T("Aniso"), LVCFMT_RIGHT, 56);
	m_listFeatures.InsertColumn(6, _T("Code"), LVCFMT_LEFT, 80);

	// ========== 분석 탭 ==========
	{
		CStatic* lbl = new CStatic();
		lbl->Create(_T("Histogram feature"), WS_CHILD | SS_CENTERIMAGE, CRect(18, kCY, 138, kCY + 24), this, IDC_STATIC_HISTFEAT_LABEL);
		lbl->SetFont(pFont);
		m_comboHistFeat.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
			CRect(140, kCY, 300, kCY + 240), this, IDC_COMBO_HISTFEAT);
		m_comboHistFeat.SetFont(pFont);
	}
	m_chart.CreateCtrl(this, CRect(18, 76, kCR, kCB), IDC_CHART_PANEL);
	m_chart.SetData(&m_results, m_profileLoaded, std::vector<std::string>());

	// 상태 라벨 위치 조정
	CWnd* status = GetDlgItem(IDC_STATIC_STATUS);
	if (status)
		status->MoveWindow(10, kClientH - 22, kClientW - 20, 18);
}

void CGlimRegionViewerDlg::BuildPageControlLists()
{
	for (int i = 0; i < TAB_COUNT; ++i)
		m_pageCtrls[i].clear();

	// 홈
	m_pageCtrls[TAB_HOME].push_back(IDC_STATIC_HOME);

	// 설정
	int settings[] = {
		IDC_BTN_OPEN_IMAGE, IDC_BTN_OPEN_FOLDER,
		IDC_STATIC_PROFILE_LABEL, IDC_COMBO_PROFILE,
		IDC_STATIC_THREADS_LABEL, IDC_EDIT_THREADS,
		IDC_STATIC_BINARIZE_LABEL, IDC_COMBO_BINARIZE,
		IDC_STATIC_TH_LABEL, IDC_EDIT_TH,
		IDC_STATIC_OFFSET_LABEL, IDC_EDIT_OFFSET,
		IDC_STATIC_WK_LABEL, IDC_EDIT_WK_KERNEL,
		IDC_STATIC_WB_LABEL, IDC_EDIT_WK_BLUR,
		IDC_STATIC_WR_LABEL, IDC_EDIT_WK_RESP,
		IDC_STATIC_XSCALE_LABEL, IDC_EDIT_XSCALE,
		IDC_STATIC_YSCALE_LABEL, IDC_EDIT_YSCALE,
		IDC_BTN_ANALYZE, IDC_LIST_FILES, IDC_PREVIEW_PANEL
	};
	for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); ++i)
		m_pageCtrls[TAB_SETTINGS].push_back(settings[i]);

	// 결과
	int results[] = {
		IDC_STATIC_ZOOM_LABEL, IDC_COMBO_ZOOM, IDC_CHECK_OVERLAY, IDC_BTN_EXPORT_CSV,
		IDC_CARD_LIST, IDC_IMAGE_VIEW, IDC_LIST_FEATURES
	};
	for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); ++i)
		m_pageCtrls[TAB_RESULTS].push_back(results[i]);

	// 분석
	int analysis[] = { IDC_STATIC_HISTFEAT_LABEL, IDC_COMBO_HISTFEAT, IDC_CHART_PANEL };
	for (size_t i = 0; i < sizeof(analysis) / sizeof(analysis[0]); ++i)
		m_pageCtrls[TAB_ANALYSIS].push_back(analysis[i]);
}

void CGlimRegionViewerDlg::ShowPage(int tab)
{
	if (tab < 0 || tab >= TAB_COUNT)
		return;
	m_curTab = tab;

	for (int p = 0; p < TAB_COUNT; ++p)
	{
		const int show = (p == tab) ? SW_SHOW : SW_HIDE;
		for (size_t i = 0; i < m_pageCtrls[p].size(); ++i)
		{
			CWnd* w = GetDlgItem(m_pageCtrls[p][i]);
			if (w)
				w->ShowWindow(show);
		}
	}

	if (tab == TAB_SETTINGS)
		UpdateBinarizeParamVisibility();

	// 결과 탭 상세 이미지 영역 재그리기
	Invalidate();
	UpdateWindow();
}

void CGlimRegionViewerDlg::UpdateBinarizeParamVisibility()
{
	if (m_curTab != TAB_SETTINGS)
		return;
	const int sel = m_comboBinarize.GetCurSel();
	// 모드: 0 Bright(127) 1 Dark(127) 2 Dark auto 3 Bright auto 4 Binary 5 Wrinkle
	const bool fixed = (sel == 0 || sel == 1);
	const bool autoOff = (sel == 2 || sel == 3);
	const bool wrinkle = (sel == 5);

	struct { UINT id; bool vis; } vis[] = {
		{ IDC_STATIC_TH_LABEL, fixed }, { IDC_EDIT_TH, fixed },
		{ IDC_STATIC_OFFSET_LABEL, autoOff }, { IDC_EDIT_OFFSET, autoOff },
		{ IDC_STATIC_WK_LABEL, wrinkle }, { IDC_EDIT_WK_KERNEL, wrinkle },
		{ IDC_STATIC_WB_LABEL, wrinkle }, { IDC_EDIT_WK_BLUR, wrinkle },
		{ IDC_STATIC_WR_LABEL, wrinkle }, { IDC_EDIT_WK_RESP, wrinkle },
	};
	for (size_t i = 0; i < sizeof(vis) / sizeof(vis[0]); ++i)
	{
		CWnd* w = GetDlgItem(vis[i].id);
		if (w)
			w->ShowWindow(vis[i].vis ? SW_SHOW : SW_HIDE);
	}
}

void CGlimRegionViewerDlg::PopulateProfileCombo()
{
	m_comboProfile.ResetContent();
	m_profilePaths.clear();

	m_comboProfile.AddString(_T("(none)"));
	m_profilePaths.push_back(std::string());

	CString exeDir = GetExeDir();
	CString candidates[] = {
		exeDir + _T("\\profiles"),
		exeDir + _T("\\..\\..\\..\\profiles"),
		exeDir + _T("\\..\\..\\profiles"),
	};

	for (int c = 0; c < 3; ++c)
	{
		CString pattern = candidates[c] + _T("\\*.ini");
		CFileFind finder;
		BOOL found = finder.FindFile(pattern);
		bool any = false;
		while (found)
		{
			found = finder.FindNextFile();
			if (finder.IsDots() || finder.IsDirectory())
				continue;
			CString full = finder.GetFilePath();
			CString name = finder.GetFileName();
			m_comboProfile.AddString(name);
			m_profilePaths.push_back(ToStd(full));
			any = true;
		}
		if (any)
			break;
	}
	m_comboProfile.SetCurSel(0);
}

void CGlimRegionViewerDlg::PopulateHistFeatCombo()
{
	m_comboHistFeat.ResetContent();
	const LPCTSTR feats[] = {
		_T("area"), _T("circularity"), _T("convexity"), _T("roundness"),
		_T("anisometry"), _T("compactness"), _T("rectangularity"),
		_T("contlength"), _T("diameter"), _T("holes")
	};
	for (int i = 0; i < 10; ++i)
		m_comboHistFeat.AddString(feats[i]);
	m_comboHistFeat.SetCurSel(0);
	m_chart.SetHistogramFeature("area");
}

void CGlimRegionViewerDlg::SetStatus(const CString& text)
{
	CWnd* status = GetDlgItem(IDC_STATIC_STATUS);
	if (status)
		status->SetWindowText(text);
}

CString CGlimRegionViewerDlg::CurrentProfilePath()
{
	int sel = m_comboProfile.GetCurSel();
	if (sel < 0 || sel >= (int)m_profilePaths.size())
		return CString();
	return ToCStr(m_profilePaths[sel]);
}

int CGlimRegionViewerDlg::GetEditInt(UINT id, int def) const
{
	CWnd* w = const_cast<CGlimRegionViewerDlg*>(this)->GetDlgItem(id);
	if (!w)
		return def;
	CString s;
	w->GetWindowText(s);
	s.Trim();
	if (s.IsEmpty())
		return def;
	return _ttoi(s);
}

double CGlimRegionViewerDlg::GetEditDouble(UINT id, double def) const
{
	CWnd* w = const_cast<CGlimRegionViewerDlg*>(this)->GetDlgItem(id);
	if (!w)
		return def;
	CString s;
	w->GetWindowText(s);
	s.Trim();
	if (s.IsEmpty())
		return def;
	return _tstof(s);
}

// ------------------------------------------------------------------
// 렌더링(결과 탭 상세 이미지)
// ------------------------------------------------------------------
void CGlimRegionViewerDlg::RefreshDetailView()
{
	if (!m_detailView.GetSafeHwnd())
		return;
	try
	{
		m_detailView.SetImage(BuildOverlayMat());
	}
	catch (...)
	{
		m_detailView.Clear();
	}
}

cv::Mat CGlimRegionViewerDlg::BuildOverlayMat()
{
	if (m_binImage.empty())
		return cv::Mat();

	cv::Mat bgr;
	cv::cvtColor(m_binImage, bgr, cv::COLOR_GRAY2BGR);

	if (!m_showOverlay)
		return bgr;

	try
	{
		for (size_t i = 0; i < m_regions.size(); ++i)
		{
			if (static_cast<int>(i) == m_highlightRegion)
				continue;
			cv::drawContours(bgr, m_regions[i].AllContours(), -1, cv::Scalar(0, 200, 0), 1);
		}
		if (m_highlightRegion >= 0 && m_highlightRegion < static_cast<int>(m_regions.size()))
		{
			cv::drawContours(bgr, m_regions[m_highlightRegion].AllContours(), -1,
				cv::Scalar(0, 0, 255), 2);
		}
	}
	catch (...)
	{
	}
	return bgr;
}

// ------------------------------------------------------------------
// 데이터 흐름
// ------------------------------------------------------------------
void CGlimRegionViewerDlg::LoadFolder(const CString& dir)
{
	m_files.clear();
	m_listFiles.DeleteAllItems();
	m_curIndex = -1;

	CString pattern = dir + _T("\\*.*");
	CFileFind finder;
	BOOL found = finder.FindFile(pattern);
	while (found)
	{
		found = finder.FindNextFile();
		if (finder.IsDots() || finder.IsDirectory())
			continue;
		std::string path = ToStd(finder.GetFilePath());
		std::string ext;
		size_t dot = path.find_last_of('.');
		if (dot != std::string::npos)
			ext = path.substr(dot);
		if (Grf::CsvExporter::IsSupportedImage(ext))
			m_files.push_back(path);
	}
	std::sort(m_files.begin(), m_files.end());

	for (size_t i = 0; i < m_files.size(); ++i)
	{
		std::string name = m_files[i];
		size_t sl = name.find_last_of("\\/");
		if (sl != std::string::npos)
			name = name.substr(sl + 1);
		m_listFiles.InsertItem(static_cast<int>(i), ToCStr(name));
	}

	CString msg;
	msg.Format(_T("%d image(s) in folder."), static_cast<int>(m_files.size()));
	SetStatus(msg);

	if (!m_files.empty())
	{
		m_listFiles.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_listFiles.SetFocus();
		// 선택 알림 미발생 대비 명시적 로드(중복돼도 idempotent)
		LoadImageAt(0);
		UpdatePreview();
	}
	else
	{
		m_binImage = cv::Mat();
		m_regions.clear();
		m_features.clear();
		UpdateFeatureList();
		UpdatePreview();
		RefreshDetailView();
	}
}

void CGlimRegionViewerDlg::LoadImageAt(int index)
{
	if (index < 0 || index >= static_cast<int>(m_files.size()))
		return;
	m_curIndex = index;

	try
	{
		cv::Mat gray = cv::imread(m_files[index], cv::IMREAD_GRAYSCALE);
		if (gray.empty())
		{
			m_binImage = cv::Mat();
			m_regions.clear();
			m_features.clear();
			m_highlightRegion = -1;
			SetStatus(_T("이미지 로드 실패: ") + ToCStr(m_files[index]));
			UpdateFeatureList();
			RefreshDetailView();
			return;
		}

		Grf::CpuPreprocessor pre(CurrentBinarizeParams());
		m_binImage = pre.Binarize(gray);
		if (m_binImage.empty())
		{
			m_regions.clear();
			m_features.clear();
			m_highlightRegion = -1;
			SetStatus(_T("이진화 실패: ") + ToCStr(m_files[index]));
			UpdateFeatureList();
			RefreshDetailView();
			return;
		}

		AnalyzeCurrent();
		UpdateFeatureList();

		CString msg;
		msg.Format(_T("%dx%d, region(s): %d"),
			m_binImage.cols, m_binImage.rows, static_cast<int>(m_regions.size()));
		SetStatus(msg);
	}
	catch (const cv::Exception& e)
	{
		m_binImage = cv::Mat();
		m_regions.clear();
		m_features.clear();
		SetStatus(CString(_T("OpenCV 오류: ")) + CString(e.what()));
	}
	catch (...)
	{
		SetStatus(_T("이미지 처리 중 알 수 없는 오류."));
	}

	RefreshDetailView();
}

void CGlimRegionViewerDlg::AnalyzeCurrent()
{
	m_regions.clear();
	m_features.clear();
	m_highlightRegion = -1;

	if (m_binImage.empty())
		return;

	try
	{
		Grf::RegionExtractor extractor;
		m_regions = extractor.Extract(m_binImage, 1);

		Grf::FeatureCalculator calc;
		m_features.reserve(m_regions.size());
		for (size_t i = 0; i < m_regions.size(); ++i)
			m_features.push_back(calc.Compute(m_regions[i]));
	}
	catch (...)
	{
		m_regions.clear();
		m_features.clear();
	}
}

void CGlimRegionViewerDlg::UpdateFeatureList()
{
	m_listFeatures.DeleteAllItems();

	for (size_t i = 0; i < m_features.size(); ++i)
	{
		const Grf::FeatureVector& fv = m_features[i];
		CString s;
		s.Format(_T("%d"), static_cast<int>(i));
		int row = m_listFeatures.InsertItem(static_cast<int>(i), s);
		if (row < 0)
			continue;

		s.Format(_T("%.0f"), fv.area);        m_listFeatures.SetItemText(row, 1, s);
		s.Format(_T("%.3f"), fv.circularity); m_listFeatures.SetItemText(row, 2, s);
		s.Format(_T("%.3f"), fv.convexity);   m_listFeatures.SetItemText(row, 3, s);
		s.Format(_T("%.3f"), fv.roundness);   m_listFeatures.SetItemText(row, 4, s);
		s.Format(_T("%.2f"), fv.anisometry);  m_listFeatures.SetItemText(row, 5, s);

		CString code;
		if (m_profileLoaded)
		{
			try { code = ToCStr(m_profile.RuleEngine().Classify(fv, "OK")); }
			catch (...) { code = _T(""); }
		}
		m_listFeatures.SetItemText(row, 6, code);
	}
}

void CGlimRegionViewerDlg::UpdatePreview()
{
	std::vector<cv::Mat> slots;
	std::vector<CString> labels;

	if (m_curIndex >= 0 && m_curIndex < static_cast<int>(m_files.size()))
	{
		try
		{
			cv::Mat gray = m_thumbCache.GetGray(m_files[m_curIndex]);
			if (!gray.empty())
			{
				slots.push_back(gray);
				labels.push_back(_T("Original"));

				Grf::CpuPreprocessor pre(CurrentBinarizeParams());
				std::vector<Grf::BinChannel> ch = pre.BinarizeMulti(gray);
				for (size_t i = 0; i < ch.size() && slots.size() < 3; ++i)
				{
					if (ch[i].image.empty())
						continue;
					slots.push_back(ch[i].image);
					CString lb;
					lb.Format(_T("Binarized (%c)"), ch[i].tag);
					labels.push_back(lb);
				}
			}
		}
		catch (...)
		{
		}
	}
	m_preview.SetSlots(slots, labels);
}

void CGlimRegionViewerDlg::RunAnalysis()
{
	if (m_files.empty())
	{
		AfxMessageBox(_T("먼저 [Open Image] 또는 [Open Folder] 로 이미지를 여세요."));
		return;
	}

	CWaitCursor wait;
	SetStatus(_T("분석 중..."));

	const int threads = GetEditInt(IDC_EDIT_THREADS, 0);
	Grf::CpuPreprocessor pre(CurrentBinarizeParams());
	const Grf::ProfileLoader* pp = m_profileLoaded ? &m_profile : NULL;

	try
	{
		GrfView::AnalyzeFiles(m_files, pre, pp, threads, 1, m_results, NULL);
	}
	catch (...)
	{
		m_results.clear();
		SetStatus(_T("분석 중 오류가 발생했습니다."));
		return;
	}

	// 결과 탭 갱신
	m_thumbCache.SetCapacity(96);
	m_cards.SetData(&m_results, &m_thumbCache, m_profileLoaded);

	// 분석 탭 갱신(Score 특징 순서 전달)
	std::vector<std::string> scoreNames = Grf::CsvExporter::ScoreFeatureNames(pp);
	m_chart.SetData(&m_results, m_profileLoaded, scoreNames);

	UpdateHomeSummary();

	CString msg;
	msg.Format(_T("분석 완료: %d files, %d regions"),
		static_cast<int>(m_files.size()), static_cast<int>(m_results.size()));
	SetStatus(msg);

	// 결과 탭으로 자동 전환
	m_tab.SetCurSel(TAB_RESULTS);
	ShowPage(TAB_RESULTS);
}

void CGlimRegionViewerDlg::UpdateHomeSummary()
{
	CString s;
	s += _T("Glim Region Viewer - 분석 대시보드\r\n\r\n");

	CString folder;
	if (!m_files.empty())
	{
		std::string first = m_files[0];
		size_t sl = first.find_last_of("\\/");
		folder = ToCStr(sl != std::string::npos ? first.substr(0, sl) : first);
	}
	CString line;
	line.Format(_T("폴더: %s\r\n"), folder.IsEmpty() ? _T("(없음)") : (LPCTSTR)folder);
	s += line;
	line.Format(_T("이미지 파일: %d 개\r\n"), static_cast<int>(m_files.size()));
	s += line;
	line.Format(_T("검출 Region(카드): %d 개\r\n"), static_cast<int>(m_results.size()));
	s += line;
	line.Format(_T("프로파일: %s\r\n\r\n"),
		m_profileLoaded ? (LPCTSTR)ToCStr(m_profile.ProfileName()) : _T("(none)"));
	s += line;

	// 분류코드별 카운트
	std::map<std::string, int> counts;
	for (size_t i = 0; i < m_results.size(); ++i)
	{
		std::string c = m_results[i].code;
		if (c.empty())
			c = "-";
		counts[c] += 1;
	}
	if (!counts.empty())
	{
		s += _T("분류코드별 카운트:\r\n");
		for (std::map<std::string, int>::const_iterator it = counts.begin(); it != counts.end(); ++it)
		{
			line.Format(_T("  %s : %d\r\n"), (LPCTSTR)ToCStr(it->first), it->second);
			s += line;
		}
	}
	else
	{
		s += _T("아직 분석 결과가 없습니다. [설정] 탭에서 [분석 실행] 하세요.\r\n");
	}

	if (m_staticHome.GetSafeHwnd())
		m_staticHome.SetWindowText(s);
}

void CGlimRegionViewerDlg::LoadProfileSelection()
{
	m_profileLoaded = false;
	CString path = CurrentProfilePath();
	if (path.IsEmpty())
		return;
	try
	{
		if (m_profile.Load(ToStd(path)))
			m_profileLoaded = true;
	}
	catch (...)
	{
		m_profileLoaded = false;
	}
}

// ------------------------------------------------------------------
// 핸들러
// ------------------------------------------------------------------
void CGlimRegionViewerDlg::OnBnClickedOpenImage()
{
	CFileDialog dlg(TRUE, NULL, NULL,
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		_T("Images|*.bmp;*.png;*.jpg;*.jpeg;*.tif;*.tiff|All Files|*.*||"), this);
	if (dlg.DoModal() != IDOK)
		return;

	m_files.clear();
	m_listFiles.DeleteAllItems();
	m_curIndex = -1;

	m_files.push_back(ToStd(dlg.GetPathName()));
	m_listFiles.InsertItem(0, dlg.GetFileName());
	m_listFiles.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	// 선택 알림(LVN_ITEMCHANGED) 미발생 대비 명시적 로드(중복돼도 idempotent)
	LoadImageAt(0);
	UpdatePreview();
}

void CGlimRegionViewerDlg::OnBnClickedOpenFolder()
{
	TCHAR pathBuf[MAX_PATH] = { 0 };
	BROWSEINFO bi;
	::ZeroMemory(&bi, sizeof(bi));
	bi.hwndOwner = GetSafeHwnd();
	bi.pszDisplayName = pathBuf;
	bi.lpszTitle = _T("Select a folder containing images");
	bi.ulFlags = BIF_RETURNONLYFSDIRS;

	LPITEMIDLIST pidl = ::SHBrowseForFolder(&bi);
	if (pidl == NULL)
		return;

	TCHAR folder[MAX_PATH] = { 0 };
	if (::SHGetPathFromIDList(pidl, folder))
		LoadFolder(folder);

	::CoTaskMemFree(pidl);
}

void CGlimRegionViewerDlg::OnBnClickedExportCsv()
{
	if (m_files.empty())
	{
		AfxMessageBox(_T("먼저 이미지 또는 폴더를 여세요."));
		return;
	}

	std::string first = m_files[0];
	size_t sl = first.find_last_of("\\/");
	std::string dir = (sl != std::string::npos) ? first.substr(0, sl) : std::string(".");

	CFileDialog dlg(FALSE, _T("csv"), _T("regions.csv"),
		OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
		_T("CSV Files|*.csv|All Files|*.*||"), this);
	if (dlg.DoModal() != IDOK)
		return;

	const CString outPath = dlg.GetPathName();
	const Grf::ProfileLoader* pp = m_profileLoaded ? &m_profile : NULL;
	Grf::CpuPreprocessor pre(CurrentBinarizeParams());

	// X/Y Scale(mm/px) 을 CSV mm 파생 컬럼 계산에 연결(병행 작업으로 엔진 옵션 병합됨).
	Grf::ExportOptions opt;
	opt.m_scaleX = GetEditDouble(IDC_EDIT_XSCALE, 1.0);
	opt.m_scaleY = GetEditDouble(IDC_EDIT_YSCALE, 1.0);

	Grf::BatchStat stat;
	bool ok = false;
	try
	{
		ok = Grf::CsvExporter::ExportFolder(dir, ToStd(outPath), pp, stat, 0, &pre,
			std::string(), opt);
	}
	catch (...)
	{
		ok = false;
	}

	if (!ok)
	{
		SetStatus(_T("CSV 내보내기 실패(폴더/경로 확인)."));
		AfxMessageBox(_T("CSV 내보내기에 실패했습니다. 폴더/출력 경로를 확인하세요."));
		return;
	}

	CString msg;
	msg.Format(_T("CSV saved: %d/%d files, %I64d regions, %d failed"),
		static_cast<int>(stat.m_processedFiles),
		static_cast<int>(stat.m_totalFiles),
		static_cast<__int64>(stat.m_totalRegions),
		static_cast<int>(stat.m_failedFiles.size()));
	SetStatus(msg + _T("  -> ") + outPath);
	AfxMessageBox(msg);
}

void CGlimRegionViewerDlg::OnBnClickedOverlay()
{
	m_showOverlay = (IsDlgButtonChecked(IDC_CHECK_OVERLAY) == BST_CHECKED);
	RefreshDetailView();
}

void CGlimRegionViewerDlg::OnBnClickedAnalyze()
{
	RunAnalysis();
}

void CGlimRegionViewerDlg::OnZoomChanged()
{
	const int sel = m_comboZoom.GetCurSel();
	const int zooms[] = { 1, 2, 4, 8, 16 };
	if (sel >= 0 && sel < 5)
		m_zoom = zooms[sel];
	m_detailView.SetZoom(m_zoom);
}

void CGlimRegionViewerDlg::OnProfileChanged()
{
	LoadProfileSelection();
	m_cards.SetData(&m_results, &m_thumbCache, m_profileLoaded);
	UpdateFeatureList();

	CString msg;
	if (m_profileLoaded)
		msg.Format(_T("Profile loaded: %s (rules=%d)"),
			ToCStr(m_profile.ProfileName()),
			static_cast<int>(m_profile.RuleEngine().RuleCount()));
	else
		msg = _T("Profile: (none)");
	SetStatus(msg);
}

Grf::BinarizeParams CGlimRegionViewerDlg::CurrentBinarizeParams() const
{
	Grf::BinarizeParams p;
	const int sel = const_cast<CGlimRegionViewerDlg*>(this)->m_comboBinarize.GetCurSel();
	switch (sel)
	{
	case 1: // Dark 고정
		p.m_mode = Grf::BINMODE_FIXED;
		p.m_polarity = Grf::POLARITY_DARK;
		p.m_threshold = GetEditDouble(IDC_EDIT_TH, 127.0);
		break;
	case 2: // Dark auto
		p.m_mode = Grf::BINMODE_MEAN_OFFSET;
		p.m_polarity = Grf::POLARITY_DARK;
		p.m_offset = GetEditDouble(IDC_EDIT_OFFSET, 20.0);
		break;
	case 3: // Bright auto
		p.m_mode = Grf::BINMODE_MEAN_OFFSET;
		p.m_polarity = Grf::POLARITY_BRIGHT;
		p.m_offset = GetEditDouble(IDC_EDIT_OFFSET, 20.0);
		break;
	case 4: // Binary as-is
		p.m_mode = Grf::BINMODE_BINARY;
		p.m_polarity = Grf::POLARITY_BRIGHT;
		break;
	case 5: // Wrinkle
		p.m_mode = Grf::BINMODE_WRINKLE;
		p.m_polarity = Grf::POLARITY_DARK;
		p.m_kernelSize = GetEditInt(IDC_EDIT_WK_KERNEL, 15);
		p.m_blurH = GetEditInt(IDC_EDIT_WK_BLUR, 31);
		p.m_responseThresh = GetEditDouble(IDC_EDIT_WK_RESP, 4.0);
		break;
	case 0:
	default: // Bright 고정
		p.m_mode = Grf::BINMODE_FIXED;
		p.m_polarity = Grf::POLARITY_BRIGHT;
		p.m_threshold = GetEditDouble(IDC_EDIT_TH, 127.0);
		break;
	}
	return p;
}

void CGlimRegionViewerDlg::OnBinarizeChanged()
{
	UpdateBinarizeParamVisibility();
	if (m_curIndex >= 0 && m_curIndex < static_cast<int>(m_files.size()))
		LoadImageAt(m_curIndex);
	UpdatePreview();
}

void CGlimRegionViewerDlg::OnParamEditChanged()
{
	m_xScale = GetEditDouble(IDC_EDIT_XSCALE, 1.0);
	m_yScale = GetEditDouble(IDC_EDIT_YSCALE, 1.0);
	// 연결됨: Export CSV → CsvExporter::ExportOptions(m_scaleX/Y) 로 mm 파생 컬럼 계산.
	// TODO(Projection): 미리보기/카드/차트에 mm 단위 표기가 필요해지면 이 값을 참조.
	UpdatePreview();
}

void CGlimRegionViewerDlg::OnHistFeatChanged()
{
	int sel = m_comboHistFeat.GetCurSel();
	if (sel < 0)
		return;
	CString s;
	m_comboHistFeat.GetLBText(sel, s);
	m_chart.SetHistogramFeature(ToStd(s));
}

void CGlimRegionViewerDlg::OnFileListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;

	if ((pnmv->uNewState & LVIS_SELECTED) && !(pnmv->uOldState & LVIS_SELECTED))
	{
		LoadImageAt(pnmv->iItem);
		UpdatePreview();
	}
}

void CGlimRegionViewerDlg::OnFeatureListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;

	if ((pnmv->uNewState & LVIS_SELECTED) && !(pnmv->uOldState & LVIS_SELECTED))
	{
		m_highlightRegion = pnmv->iItem;
		RefreshDetailView();
	}
}

void CGlimRegionViewerDlg::OnTabSelChange(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	*pResult = 0;
	ShowPage(m_tab.GetCurSel());
}

LRESULT CGlimRegionViewerDlg::OnCardSelected(WPARAM wParam, LPARAM /*lParam*/)
{
	const int idx = static_cast<int>(wParam);
	if (idx < 0 || idx >= static_cast<int>(m_results.size()))
		return 0;

	const GrfView::RegionResult& rr = m_results[idx];
	// 해당 파일을 상세 뷰로 로드 후 Region 강조
	if (rr.fileIndex >= 0 && rr.fileIndex < static_cast<int>(m_files.size()))
	{
		LoadImageAt(rr.fileIndex);
		m_highlightRegion = rr.regionIndex;
		// 상세 특징값 리스트에서 해당 행 선택
		if (rr.regionIndex >= 0 && rr.regionIndex < m_listFeatures.GetItemCount())
			m_listFeatures.SetItemState(rr.regionIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		RefreshDetailView();
	}
	return 0;
}
