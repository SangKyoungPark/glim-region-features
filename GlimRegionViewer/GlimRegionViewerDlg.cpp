// GlimRegionViewerDlg.cpp : 메인 다이얼로그(탭 기반 분석 대시보드) 구현
#include "stdafx.h"
#include "GlimRegionViewer.h"
#include "GlimRegionViewerDlg.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <map>
#include <climits>    // INT_MIN
#include <shlobj.h>   // SHBrowseForFolder

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ------------------------------------------------------------------
// 애플리케이션 버전 / 타이틀 / INI
//  버전업 시 아래 3곳을 함께 갱신할 것:
//   1) 이 매크로(GRV_APP_VERSION)  2) GlimRegionViewer.rc 의 VS_VERSION_INFO
//   3) build\make_deploy.bat 의 VERSION
// ------------------------------------------------------------------
#define GRV_APP_VERSION   _T("1.0.0")
#define GRV_APP_TITLE     _T("Glim Region Viewer")

namespace {
	const UINT_PTR kTimerProgress = 1001;   // 분석 진행 폴링 타이머
	const int      kProgressPollMs = 120;
	LPCTSTR kIniSection    = _T("Viewer");
	LPCTSTR kIniSectionWin = _T("Window");
}

// ------------------------------------------------------------------
// 레이아웃 상수(픽셀). 클라이언트 1180 x 780 기준.
// ------------------------------------------------------------------
namespace {
	// 상세 뷰(AnalyzeCurrent)와 폴더 분석(RunAnalysis→AnalyzeFiles→ProcessOne)이
	//  반드시 동일 값을 써야 채널 순회 regionIndex 매핑이 일치한다.
	const int kDefaultMinArea = 1;

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
	, m_activeProjection(false)
	, m_lastAnalysisProjection(false)
	, m_hasAnalysisParams(false)
	, m_profileLoaded(false)
	, m_xScale(1.0)
	, m_yScale(1.0)
	, m_zoom(4)
	, m_showOverlay(true)
	, m_highlightRegion(-1)
	, m_progress(0)
	, m_analysisDone(false)
	, m_analysisFailed(false)
	, m_analyzing(false)
	, m_analysisTotal(0)
	, m_clusterK(0)
	, m_clusterSilhouette(0.0)
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
	ON_BN_CLICKED(IDC_BTN_CLUSTER, &CGlimRegionViewerDlg::OnBnClickedRunCluster)
	ON_CBN_SELCHANGE(IDC_COMBO_CLX, &CGlimRegionViewerDlg::OnClusterAxisChanged)
	ON_CBN_SELCHANGE(IDC_COMBO_CLY, &CGlimRegionViewerDlg::OnClusterAxisChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_TH, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_OFFSET, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_WK_KERNEL, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_WK_BLUR, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_WK_RESP, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_PBLACK_TH, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_PWHITE_TH, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_PKERNEL, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_XSCALE, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_EN_KILLFOCUS(IDC_EDIT_YSCALE, &CGlimRegionViewerDlg::OnParamEditChanged)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FILES, &CGlimRegionViewerDlg::OnFileListItemChanged)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FEATURES, &CGlimRegionViewerDlg::OnFeatureListItemChanged)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_MAIN, &CGlimRegionViewerDlg::OnTabSelChange)
	ON_MESSAGE(WM_GRFVIEW_CARD_SEL, &CGlimRegionViewerDlg::OnCardSelected)
	ON_WM_DESTROY()
	ON_WM_TIMER()
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
		PopulateClusterCombos();
		BuildPageControlLists();
		ShowPage(TAB_SETTINGS);
	}
	catch (...)
	{
		SetStatus(_T("초기화 중 오류가 발생했습니다."));
	}

	// INI 복원(부재/손상 시 기본값 유지). 창 위치/크기까지 복원.
	try
	{
		ResolveIniPath();
		LoadSettings();
	}
	catch (...)
	{
		SetStatus(_T("설정 복원 중 오류(기본값으로 계속)."));
	}

	UpdateWindowTitle();
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
	m_tab.InsertItem(TAB_CLUSTER, _T("군집"));
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
			_T("Binary"), _T("Wrinkle"), _T("Projection(검사기 방식)") };
		for (int i = 0; i < 7; ++i)
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
	// projection 흑TH / 백TH / 커널 — 모드 6 (동일 행, 배타적 표시)
	{
		CStatic* lb = new CStatic();
		lb->Create(_T("흑TH"), WS_CHILD | SS_CENTERIMAGE, CRect(18, r2, 56, r2 + 22), this, IDC_STATIC_PBLACK_LABEL);
		lb->SetFont(pFont);
		m_editPBlackTh.Create(WS_CHILD | WS_TABSTOP | WS_BORDER, CRect(58, r2, 108, r2 + 22), this, IDC_EDIT_PBLACK_TH);
		m_editPBlackTh.SetFont(pFont);
		m_editPBlackTh.SetWindowText(_T("25"));

		CStatic* lw = new CStatic();
		lw->Create(_T("백TH"), WS_CHILD | SS_CENTERIMAGE, CRect(114, r2, 152, r2 + 22), this, IDC_STATIC_PWHITE_LABEL);
		lw->SetFont(pFont);
		m_editPWhiteTh.Create(WS_CHILD | WS_TABSTOP | WS_BORDER, CRect(154, r2, 204, r2 + 22), this, IDC_EDIT_PWHITE_TH);
		m_editPWhiteTh.SetFont(pFont);
		m_editPWhiteTh.SetWindowText(_T("25"));

		CStatic* lk = new CStatic();
		lk->Create(_T("Kernel"), WS_CHILD | SS_CENTERIMAGE, CRect(210, r2, 250, r2 + 22), this, IDC_STATIC_PKERNEL_LABEL);
		lk->SetFont(pFont);
		m_editPKernel.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER, CRect(252, r2, 296, r2 + 22), this, IDC_EDIT_PKERNEL);
		m_editPKernel.SetFont(pFont);
		m_editPKernel.SetWindowText(_T("4"));
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
	m_listFeatures.InsertColumn(6, _T("Ch"), LVCFMT_CENTER, 32);
	m_listFeatures.InsertColumn(7, _T("Code"), LVCFMT_LEFT, 80);

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

	// ========== 군집 탭 ==========
	{
		// Row1: 특징 프리셋 / 스케일 / K / Kmax / 실행 버튼
		CStatic* lf = new CStatic();
		lf->Create(_T("Features"), WS_CHILD | SS_CENTERIMAGE, CRect(18, kCY, 76, kCY + 24), this, IDC_STATIC_CLFEAT_LABEL);
		lf->SetFont(pFont);
		m_comboClFeatSet.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
			CRect(78, kCY, 248, kCY + 200), this, IDC_COMBO_CLFEATSET);
		m_comboClFeatSet.SetFont(pFont);

		CStatic* ls = new CStatic();
		ls->Create(_T("Scale"), WS_CHILD | SS_CENTERIMAGE, CRect(262, kCY, 302, kCY + 24), this, IDC_STATIC_CLSCALE_LABEL);
		ls->SetFont(pFont);
		m_comboClScale.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
			CRect(304, kCY, 414, kCY + 200), this, IDC_COMBO_CLSCALE);
		m_comboClScale.SetFont(pFont);

		CStatic* lk = new CStatic();
		lk->Create(_T("K(0=auto)"), WS_CHILD | SS_CENTERIMAGE, CRect(428, kCY, 496, kCY + 24), this, IDC_STATIC_CLK_LABEL);
		lk->SetFont(pFont);
		m_editClK.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER,
			CRect(498, kCY + 1, 540, kCY + 23), this, IDC_EDIT_CLK);
		m_editClK.SetFont(pFont);
		m_editClK.SetWindowText(_T("0"));

		CStatic* lm = new CStatic();
		lm->Create(_T("Kmax"), WS_CHILD | SS_CENTERIMAGE, CRect(554, kCY, 592, kCY + 24), this, IDC_STATIC_CLKMAX_LABEL);
		lm->SetFont(pFont);
		m_editClKMax.Create(WS_CHILD | WS_TABSTOP | WS_BORDER | ES_NUMBER,
			CRect(594, kCY + 1, 636, kCY + 23), this, IDC_EDIT_CLKMAX);
		m_editClKMax.SetFont(pFont);
		m_editClKMax.SetWindowText(_T("8"));

		CButton* b = new CButton();
		b->Create(_T("군집화 실행"), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
			CRect(kCR - 150, kCY, kCR, kCY + 24), this, IDC_BTN_CLUSTER);
		b->SetFont(pFont);

		// Row2: 산점도 축 선택
		const int cr2 = 74;
		CStatic* lx = new CStatic();
		lx->Create(_T("X"), WS_CHILD | SS_CENTERIMAGE, CRect(18, cr2, 34, cr2 + 24), this, IDC_STATIC_CLX_LABEL);
		lx->SetFont(pFont);
		m_comboClX.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
			CRect(36, cr2, 196, cr2 + 240), this, IDC_COMBO_CLX);
		m_comboClX.SetFont(pFont);

		CStatic* ly = new CStatic();
		ly->Create(_T("Y"), WS_CHILD | SS_CENTERIMAGE, CRect(210, cr2, 226, cr2 + 24), this, IDC_STATIC_CLY_LABEL);
		ly->SetFont(pFont);
		m_comboClY.Create(WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
			CRect(228, cr2, 388, cr2 + 240), this, IDC_COMBO_CLY);
		m_comboClY.SetFont(pFont);

		m_clusterPanel.CreateCtrl(this, CRect(18, 104, kCR, kCB), IDC_CLUSTER_PANEL);
		m_clusterPanel.SetData(&m_results, NULL, 0, 0.0, NULL);
	}

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
		IDC_STATIC_PBLACK_LABEL, IDC_EDIT_PBLACK_TH,
		IDC_STATIC_PWHITE_LABEL, IDC_EDIT_PWHITE_TH,
		IDC_STATIC_PKERNEL_LABEL, IDC_EDIT_PKERNEL,
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

	// 군집
	int cluster[] = {
		IDC_STATIC_CLFEAT_LABEL, IDC_COMBO_CLFEATSET,
		IDC_STATIC_CLSCALE_LABEL, IDC_COMBO_CLSCALE,
		IDC_STATIC_CLK_LABEL, IDC_EDIT_CLK,
		IDC_STATIC_CLKMAX_LABEL, IDC_EDIT_CLKMAX,
		IDC_BTN_CLUSTER,
		IDC_STATIC_CLX_LABEL, IDC_COMBO_CLX,
		IDC_STATIC_CLY_LABEL, IDC_COMBO_CLY,
		IDC_CLUSTER_PANEL
	};
	for (size_t i = 0; i < sizeof(cluster) / sizeof(cluster[0]); ++i)
		m_pageCtrls[TAB_CLUSTER].push_back(cluster[i]);
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
	// 모드: 0 Bright(127) 1 Dark(127) 2 Dark auto 3 Bright auto 4 Binary 5 Wrinkle 6 Projection
	const bool fixed = (sel == 0 || sel == 1);
	const bool autoOff = (sel == 2 || sel == 3);
	const bool wrinkle = (sel == 5);
	const bool proj = (sel == 6);

	struct { UINT id; bool vis; } vis[] = {
		{ IDC_STATIC_TH_LABEL, fixed }, { IDC_EDIT_TH, fixed },
		{ IDC_STATIC_OFFSET_LABEL, autoOff }, { IDC_EDIT_OFFSET, autoOff },
		{ IDC_STATIC_WK_LABEL, wrinkle }, { IDC_EDIT_WK_KERNEL, wrinkle },
		{ IDC_STATIC_WB_LABEL, wrinkle }, { IDC_EDIT_WK_BLUR, wrinkle },
		{ IDC_STATIC_WR_LABEL, wrinkle }, { IDC_EDIT_WK_RESP, wrinkle },
		{ IDC_STATIC_PBLACK_LABEL, proj }, { IDC_EDIT_PBLACK_TH, proj },
		{ IDC_STATIC_PWHITE_LABEL, proj }, { IDC_EDIT_PWHITE_TH, proj },
		{ IDC_STATIC_PKERNEL_LABEL, proj }, { IDC_EDIT_PKERNEL, proj },
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
	// 기본 스코어 테이블(전 스칼라, 고정 순서)에서 동적 생성 —
	//  엔진에 특징값이 추가되면 콤보에 자동 반영된다.
	m_comboHistFeat.ResetContent();
	const std::vector<Grf::ScoreConfig>& defs = Grf::ScoreNormalizer::DefaultConfigs();
	for (size_t i = 0; i < defs.size(); ++i)
		m_comboHistFeat.AddString(ToCStr(defs[i].m_featureName));
	if (m_comboHistFeat.GetCount() == 0)
		m_comboHistFeat.AddString(_T("area"));
	m_comboHistFeat.SetCurSel(0);
	m_chart.SetHistogramFeature(defs.empty() ? "area" : defs[0].m_featureName);
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

		// 선택 Region 의 흑/백 채널 뱃지(PROJECTION 등 다채널일 때만 의미)
		const bool multiCh = m_activeProjection;
		if (multiCh && m_highlightRegion >= 0 &&
			m_highlightRegion < static_cast<int>(m_regionChannels.size()))
			m_detailView.SetChannelBadge(m_regionChannels[m_highlightRegion], true);
		else
			m_detailView.SetChannelBadge('W', false);
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
		// PROJECTION 등 다채널 모드면 채널색으로, 아니면 기존 초록으로 컨투어를 그린다.
		//  현재 m_regions 를 만든 파라미터 기준(m_activeProjection). 라이브 콤보와 별개.
		const bool multiCh = m_activeProjection;
		for (size_t i = 0; i < m_regions.size(); ++i)
		{
			if (static_cast<int>(i) == m_highlightRegion)
				continue;
			cv::Scalar col(0, 200, 0);
			if (multiCh && i < m_regionChannels.size())
			{
				COLORREF cr = GrfView::ChannelColor(m_regionChannels[i]);
				col = cv::Scalar(GetBValue(cr), GetGValue(cr), GetRValue(cr)); // BGR
			}
			cv::drawContours(bgr, m_regions[i].AllContours(), -1, col, 1);
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
	UpdateWindowTitle();

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
		m_regionChannels.clear();
		UpdateFeatureList();
		UpdatePreview();
		RefreshDetailView();
	}
}

void CGlimRegionViewerDlg::LoadImageAt(int index, const Grf::BinarizeParams* params)
{
	if (index < 0 || index >= static_cast<int>(m_files.size()))
		return;
	m_curIndex = index;

	// params 지정(카드 클릭 = 분석 스냅샷) 우선, 없으면 현재 UI 값(대화형 탐색)
	const Grf::BinarizeParams bp = params ? *params : CurrentBinarizeParams();

	try
	{
		cv::Mat gray = cv::imread(m_files[index], cv::IMREAD_GRAYSCALE);
		if (gray.empty())
		{
			m_binImage = cv::Mat();
			m_regions.clear();
			m_features.clear();
			m_regionChannels.clear();
			m_highlightRegion = -1;
			m_activeProjection = (bp.m_mode == Grf::BINMODE_PROJECTION);
			SetStatus(_T("이미지 로드 실패: ") + ToCStr(m_files[index]));
			UpdateFeatureList();
			RefreshDetailView();
			return;
		}

		AnalyzeCurrent(gray, bp);
		if (m_binImage.empty())
		{
			SetStatus(_T("이진화 실패: ") + ToCStr(m_files[index]));
			UpdateFeatureList();
			RefreshDetailView();
			return;
		}

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
		m_regionChannels.clear();
		SetStatus(CString(_T("OpenCV 오류: ")) + CString(e.what()));
	}
	catch (...)
	{
		SetStatus(_T("이미지 처리 중 알 수 없는 오류."));
	}

	RefreshDetailView();
}

void CGlimRegionViewerDlg::AnalyzeCurrent(const cv::Mat& gray, const Grf::BinarizeParams& params)
{
	m_regions.clear();
	m_features.clear();
	m_regionChannels.clear();
	m_binImage = cv::Mat();
	m_highlightRegion = -1;
	m_activeProjection = (params.m_mode == Grf::BINMODE_PROJECTION);

	if (gray.empty())
		return;

	try
	{
		// 채널 순회(PROJECTION 은 흑 B → 백 W, 그 외 모드는 극성 채널 1장).
		//  regionIndex 를 채널을 가로질러 연속 부여하여 결과 카드(ProcessOne)와 순서를 일치시킨다.
		Grf::CpuPreprocessor pre(params);
		std::vector<Grf::BinChannel> channels = pre.BinarizeMulti(gray);

		Grf::RegionExtractor extractor;
		Grf::FeatureCalculator calc;

		for (size_t c = 0; c < channels.size(); ++c)
		{
			const cv::Mat& chImg = channels[c].image;
			if (chImg.empty())
				continue;

			// 상세 뷰 배경용 합집합(흑|백 채널을 한 장으로)
			if (m_binImage.empty())
				m_binImage = chImg.clone();
			else
				cv::bitwise_or(m_binImage, chImg, m_binImage);

			std::vector<Grf::Region> regions = extractor.Extract(chImg, kDefaultMinArea);
			for (size_t r = 0; r < regions.size(); ++r)
			{
				m_regions.push_back(regions[r]);
				m_features.push_back(calc.Compute(regions[r]));
				m_regionChannels.push_back(channels[c].tag);
			}
		}
	}
	catch (...)
	{
		m_regions.clear();
		m_features.clear();
		m_regionChannels.clear();
		m_binImage = cv::Mat();
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

		// 채널(B/W). m_regionChannels 는 m_features 와 병렬.
		CString ch;
		if (i < m_regionChannels.size())
			ch = CString(m_regionChannels[i]);
		m_listFeatures.SetItemText(row, 6, ch);

		CString code;
		if (m_profileLoaded)
		{
			try { code = ToCStr(m_profile.RuleEngine().Classify(fv, "OK")); }
			catch (...) { code = _T(""); }
		}
		m_listFeatures.SetItemText(row, 7, code);
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
				const bool proj = IsProjectionMode();
				std::vector<Grf::BinChannel> ch = pre.BinarizeMulti(gray);
				for (size_t i = 0; i < ch.size() && slots.size() < 3; ++i)
				{
					if (ch[i].image.empty())
						continue;
					slots.push_back(ch[i].image);
					CString lb;
					if (proj)
						lb.Format(_T("%s 이진화 (%c)"),
							ch[i].tag == 'B' ? _T("흑") : _T("백"), ch[i].tag);
					else
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
	if (m_analyzing)
		return; // 재진입 방지(버튼도 비활성이지만 방어적)

	if (m_files.empty())
	{
		AfxMessageBox(_T("먼저 [Open Image] 또는 [Open Folder] 로 이미지를 여세요."));
		return;
	}

	// 분석 파라미터 스냅샷: 이후 사용자가 UI 값을 바꿔도 카드↔상세뷰 재분석이
	//  분석 시점과 동일한 이진화/채널 구성을 재현하도록 보관.
	m_analysisParams = CurrentBinarizeParams();
	m_hasAnalysisParams = true;
	m_lastAnalysisProjection = (m_analysisParams.m_mode == Grf::BINMODE_PROJECTION); // 홈 요약 채널 카운트 노출 조건

	const int threads = GetEditInt(IDC_EDIT_THREADS, 0);
	m_analysisTotal = static_cast<int>(m_files.size());
	m_progress = 0;
	m_analysisDone = false;
	m_analysisFailed = false;
	m_analyzing = true;

	// 진행 표시: 관련 컨트롤 비활성 + 상태 텍스트
	SetAnalysisUIEnabled(false);
	CString msg;
	msg.Format(_T("0/%d 처리 중..."), m_analysisTotal);
	SetStatus(msg);

	// 워커 스레드에서 병렬 분석 실행(UI 스레드는 타이머로 진행률 폴링).
	//  워커는 전용 출력 버퍼 m_pendingResults 와 m_progress/완료플래그만 접근한다.
	//  m_results 는 손대지 않으므로(카드/차트가 캐시한 &m_results 를 UI 스레드가 재그려도 안전),
	//  완료 시 FinalizeAnalysis 가 swap 으로 교체한다.
	const Grf::ProfileLoader* pp = m_profileLoaded ? &m_profile : NULL;
	const std::vector<std::string> files = m_files;   // 분석 중 UI가 m_files 를 바꿔도 무관하게 복사
	const Grf::BinarizeParams params = m_analysisParams;
	try
	{
		m_analysisThread = std::thread([this, files, params, pp, threads]()
		{
			try
			{
				Grf::CpuPreprocessor pre(params);
				GrfView::AnalyzeFiles(files, pre, pp, threads, kDefaultMinArea,
					this->m_pendingResults, &this->m_progress);
			}
			catch (...)
			{
				this->m_analysisFailed = true;
			}
			this->m_analysisDone = true;
		});
	}
	catch (...)
	{
		// 스레드 생성 실패 시 동기 폴백(m_pendingResults 로 받아 swap)
		m_analyzing = false;
		SetAnalysisUIEnabled(true);
		try
		{
			Grf::CpuPreprocessor pre(m_analysisParams);
			GrfView::AnalyzeFiles(m_files, pre, pp, threads, kDefaultMinArea, m_pendingResults, NULL);
			FinalizeAnalysis();
		}
		catch (...)
		{
			m_pendingResults.clear();
			SetStatus(_T("분석 중 오류가 발생했습니다."));
		}
		return;
	}

	SetTimer(kTimerProgress, kProgressPollMs, NULL);
}

// 분석 진행 중 결과 정합을 해칠 수 있는 컨트롤을 잠근다(폴더/파일 열기, CSV, 프로파일, 탭 전환).
void CGlimRegionViewerDlg::SetAnalysisUIEnabled(bool enabled)
{
	const UINT ids[] = {
		IDC_BTN_ANALYZE, IDC_BTN_OPEN_IMAGE, IDC_BTN_OPEN_FOLDER,
		IDC_BTN_EXPORT_CSV, IDC_COMBO_PROFILE, IDC_TAB_MAIN, IDC_LIST_FILES
	};
	for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
	{
		CWnd* w = GetDlgItem(ids[i]);
		if (w)
			w->EnableWindow(enabled ? TRUE : FALSE);
	}
}

// 워커 스레드 완료 후 UI 스레드에서 호출: 결과/차트/홈/타이틀 반영.
void CGlimRegionViewerDlg::FinalizeAnalysis()
{
	const Grf::ProfileLoader* pp = m_profileLoaded ? &m_profile : NULL;

	// 워커 전용 버퍼를 UI 스레드 소유 벡터로 교체(이 시점에는 워커가 join 됨 = 배타적 접근).
	m_results.swap(m_pendingResults);
	m_pendingResults.clear();

	// 결과 탭 갱신
	m_thumbCache.SetCapacity(96);
	m_cards.SetData(&m_results, &m_thumbCache, m_profileLoaded);

	// 분석 탭 갱신(Score 특징 순서 전달)
	std::vector<std::string> scoreNames = Grf::CsvExporter::ScoreFeatureNames(pp);
	m_chart.SetData(&m_results, m_profileLoaded, scoreNames);

	// 군집 탭: 결과가 바뀌었으므로 이전 군집 라벨 무효화
	ResetClusterState();

	UpdateHomeSummary();
	UpdateWindowTitle();

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
	line.Format(_T("프로파일: %s\r\n"),
		m_profileLoaded ? (LPCTSTR)ToCStr(m_profile.ProfileName()) : _T("(none)"));
	s += line;

	// 채널별 카운트(PROJECTION 실행 시만) — 흑(B)/백(W)
	if (m_lastAnalysisProjection && !m_results.empty())
	{
		int nB = 0, nW = 0;
		for (size_t i = 0; i < m_results.size(); ++i)
		{
			if (m_results[i].channel == 'B') ++nB;
			else if (m_results[i].channel == 'W') ++nW;
		}
		line.Format(_T("채널별 카운트 (Projection):  B: %d  /  W: %d\r\n"), nB, nW);
		s += line;
	}
	s += _T("\r\n");

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
	UpdateWindowTitle();
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
	case 6: // Projection(검사기 방식): 흑/백 2채널
		p.m_mode = Grf::BINMODE_PROJECTION;
		p.m_polarity = Grf::POLARITY_DARK; // 단일 Binarize() 시 흑 채널 우선(BinarizeMulti 는 흑/백 모두)
		p.m_projBlackTh = GetEditDouble(IDC_EDIT_PBLACK_TH, 25.0);
		p.m_projWhiteTh = GetEditDouble(IDC_EDIT_PWHITE_TH, 25.0);
		p.m_projKernel = GetEditInt(IDC_EDIT_PKERNEL, 4);
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

bool CGlimRegionViewerDlg::IsProjectionMode() const
{
	return const_cast<CGlimRegionViewerDlg*>(this)->m_comboBinarize.GetCurSel() == 6;
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

// ------------------------------------------------------------------
// 군집 탭
// ------------------------------------------------------------------
void CGlimRegionViewerDlg::PopulateClusterCombos()
{
	// 특징 프리셋
	m_comboClFeatSet.ResetContent();
	m_comboClFeatSet.AddString(_T("Shape 12종(권장)"));
	m_comboClFeatSet.AddString(_T("전체 스칼라"));
	m_comboClFeatSet.SetCurSel(0);

	// 스케일 모드(엔진 ClusterParams.m_scaleMode 순서와 일치: 0=zscore 1=minmax 2=robust)
	m_comboClScale.ResetContent();
	m_comboClScale.AddString(_T("z-score"));
	m_comboClScale.AddString(_T("min-max"));
	m_comboClScale.AddString(_T("robust"));
	m_comboClScale.SetCurSel(0);

	// 산점도 축(전 스칼라 목록). 기본 X=area, Y=circularity.
	m_comboClX.ResetContent();
	m_comboClY.ResetContent();
	const std::vector<Grf::ScoreConfig>& defs = Grf::ScoreNormalizer::DefaultConfigs();
	int selX = 0, selY = 0;
	for (size_t i = 0; i < defs.size(); ++i)
	{
		m_comboClX.AddString(ToCStr(defs[i].m_featureName));
		m_comboClY.AddString(ToCStr(defs[i].m_featureName));
		if (defs[i].m_featureName == "area")
			selX = static_cast<int>(i);
		if (defs[i].m_featureName == "circularity")
			selY = static_cast<int>(i);
	}
	if (m_comboClX.GetCount() == 0)
	{
		m_comboClX.AddString(_T("area"));
		m_comboClY.AddString(_T("circularity"));
	}
	m_comboClX.SetCurSel(selX);
	m_comboClY.SetCurSel(selY);
	OnClusterAxisChanged();
}

std::vector<std::string> CGlimRegionViewerDlg::ClusterFeatureNames() const
{
	std::vector<std::string> names;
	const int preset = m_comboClFeatSet.GetCurSel();
	if (preset == 1)
	{
		// 전체 스칼라(기본 스코어 테이블 순서)
		const std::vector<Grf::ScoreConfig>& defs = Grf::ScoreNormalizer::DefaultConfigs();
		for (size_t i = 0; i < defs.size(); ++i)
			names.push_back(defs[i].m_featureName);
	}
	else
	{
		// Shape 12종: 스케일 불변 형상계수 큐레이션(설계 문서 프리셋).
		//  위치성(row/col)·각도(phi) 값은 군집 입력에서 제외한다.
		static const char* kShape[] = {
			"circularity", "compactness", "convexity", "rectangularity",
			"roundness", "anisometry", "bulkiness", "structure_factor",
			"aspect_ratio", "fill_ratio", "inner_outer_ratio", "k_factor"
		};
		for (size_t i = 0; i < sizeof(kShape) / sizeof(kShape[0]); ++i)
			names.push_back(kShape[i]);
	}
	return names;
}

void CGlimRegionViewerDlg::ResetClusterState()
{
	m_clusterLabels.clear();
	m_clusterSizes.clear();
	m_clusterK = 0;
	m_clusterSilhouette = 0.0;
	m_clusterPanel.SetData(&m_results, NULL, 0, 0.0, NULL);
}

void CGlimRegionViewerDlg::OnBnClickedRunCluster()
{
	if (m_analyzing)
		return;
	if (m_results.empty())
	{
		SetStatus(_T("군집화할 결과가 없습니다. [설정] 탭에서 분석을 먼저 실행하세요."));
		return;
	}

	try
	{
		std::vector<std::string> names = ClusterFeatureNames();
		if (names.empty())
		{
			SetStatus(_T("군집 입력 특징이 없습니다."));
			return;
		}

		// FeatureVector → FeatureMatrix (행 순서 = m_results 순서 = 라벨 순서)
		Grf::FeatureMatrix mat;
		mat.SetColumns(names);
		std::vector<double> vals(names.size());
		for (size_t i = 0; i < m_results.size(); ++i)
		{
			for (size_t f = 0; f < names.size(); ++f)
				vals[f] = m_results[i].fv.GetByName(names[f]);
			mat.AddSample(m_results[i].fileName, m_results[i].regionIndex, vals);
		}

		Grf::ClusterParams cp;
		cp.m_features = names;
		const int scaleSel = m_comboClScale.GetCurSel();
		cp.m_scaleMode = (scaleSel < 0) ? 0 : scaleSel;
		cp.m_k = GetEditInt(IDC_EDIT_CLK, 0);          // 0 이하 = 자동(실루엣 추천)
		cp.m_kMax = GetEditInt(IDC_EDIT_CLKMAX, 8);

		// 자동 K: 엔진 Run 은 m_k>0 을 요구한다(규약). K<=0 이면 KSelector 로 실루엣 최대 K 를
		//  먼저 결정한 뒤 엔진에 넘긴다("군집 계산 단일 소스" 유지).
		const bool autoK = (cp.m_k <= 0);
		if (autoK)
		{
			const int kMax = (cp.m_kMax >= 2) ? cp.m_kMax : 8;
			Grf::KSelector::Curve curve = Grf::KSelector().Sweep(mat, cp, 2, kMax);
			if (!curve.m_ok || curve.m_recommendedK < 2)
			{
				SetStatus(_T("자동 K 결정 실패 (샘플 수 부족 등). K 값을 직접 지정하세요."));
				return;
			}
			cp.m_k = curve.m_recommendedK;
		}

		SetStatus(_T("군집화 계산 중..."));
		Grf::ClusterResult cr = Grf::ClusterEngine().Run(mat, cp);
		if (!cr.m_ok)
		{
			CString msg;
			msg.Format(_T("군집화 실패: %s"), (LPCTSTR)ToCStr(cr.m_error));
			SetStatus(msg);
			return;
		}

		m_clusterLabels = cr.m_labels;
		m_clusterSizes = cr.m_clusterSizes;
		m_clusterK = cr.m_k;
		m_clusterSilhouette = cr.m_silhouette;

		m_clusterPanel.SetData(&m_results, &m_clusterLabels,
			m_clusterK, m_clusterSilhouette, &m_clusterSizes);

		CString msg;
		msg.Format(_T("군집화 완료: %d regions → K=%d%s, silhouette=%.3f"),
			static_cast<int>(m_results.size()), m_clusterK,
			autoK ? _T(" (auto)") : _T(""), m_clusterSilhouette);
		SetStatus(msg);
	}
	catch (...)
	{
		SetStatus(_T("군집화 중 오류가 발생했습니다."));
	}
}

void CGlimRegionViewerDlg::OnClusterAxisChanged()
{
	CString sx, sy;
	const int ix = m_comboClX.GetCurSel();
	const int iy = m_comboClY.GetCurSel();
	if (ix >= 0)
		m_comboClX.GetLBText(ix, sx);
	if (iy >= 0)
		m_comboClY.GetLBText(iy, sy);
	if (!sx.IsEmpty() && !sy.IsEmpty())
		m_clusterPanel.SetAxes(ToStd(sx), ToStd(sy));
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
	// 해당 파일을 상세 뷰로 로드 후 Region 강조.
	//  분석 시점 파라미터 스냅샷으로 재분석해야 rr.regionIndex/채널 매핑이 일치한다
	//  (분석 후 사용자가 UI 이진화 설정을 바꿔도 카드 정합 유지).
	if (rr.fileIndex >= 0 && rr.fileIndex < static_cast<int>(m_files.size()))
	{
		const Grf::BinarizeParams* snap = m_hasAnalysisParams ? &m_analysisParams : NULL;
		LoadImageAt(rr.fileIndex, snap);
		m_highlightRegion = rr.regionIndex;
		// 상세 특징값 리스트에서 해당 행 선택
		if (rr.regionIndex >= 0 && rr.regionIndex < m_listFeatures.GetItemCount())
			m_listFeatures.SetItemState(rr.regionIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		RefreshDetailView();
	}
	return 0;
}

// ------------------------------------------------------------------
// 분석 진행 폴링 / 종료
// ------------------------------------------------------------------
void CGlimRegionViewerDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == kTimerProgress)
	{
		if (m_analyzing)
		{
			if (m_analysisDone)
			{
				KillTimer(kTimerProgress);
				if (m_analysisThread.joinable())
					m_analysisThread.join();
				m_analyzing = false;

				SetAnalysisUIEnabled(true);

				if (m_analysisFailed)
				{
					m_pendingResults.clear();
					SetStatus(_T("분석 중 오류가 발생했습니다."));
				}
				else
				{
					FinalizeAnalysis();
				}
			}
			else
			{
				CString msg;
				msg.Format(_T("%d/%d 처리 중..."),
					static_cast<int>(m_progress), m_analysisTotal);
				SetStatus(msg);
			}
		}
		else
		{
			KillTimer(kTimerProgress);
		}
		return;
	}
	CDialogEx::OnTimer(nIDEvent);
}

void CGlimRegionViewerDlg::OnDestroy()
{
	// 진행 중 워커가 있으면 this 소멸 전에 반드시 회수(댕글링 방지).
	KillTimer(kTimerProgress);
	if (m_analysisThread.joinable())
	{
		try { m_analysisThread.join(); }
		catch (...) {}
	}
	m_analyzing = false;

	// 종료 시 설정 저장(창 위치/크기 포함 — 파괴 전이라 유효).
	try { SaveSettings(); }
	catch (...) {}

	CDialogEx::OnDestroy();
}

// ------------------------------------------------------------------
// 타이틀바(버전 + 분석 상태)
// ------------------------------------------------------------------
void CGlimRegionViewerDlg::UpdateWindowTitle()
{
	CString title;
	title.Format(_T("%s v%s"), GRV_APP_TITLE, GRV_APP_VERSION);

	if (!m_files.empty())
	{
		std::string first = m_files[0];
		size_t sl = first.find_last_of("\\/");
		CString folder = ToCStr(sl != std::string::npos ? first.substr(0, sl) : first);
		// 폴더명만(마지막 세그먼트) 짧게
		int p = folder.ReverseFind(_T('\\'));
		CString leaf = (p >= 0) ? folder.Mid(p + 1) : folder;
		if (leaf.IsEmpty())
			leaf = folder;

		CString ext;
		if (!m_results.empty())
			ext.Format(_T("  -  %s (%d regions)"), (LPCTSTR)leaf, static_cast<int>(m_results.size()));
		else
			ext.Format(_T("  -  %s (%d files)"), (LPCTSTR)leaf, static_cast<int>(m_files.size()));
		title += ext;
	}
	SetWindowText(title);
}

// ------------------------------------------------------------------
// 설정 영속화(INI)
// ------------------------------------------------------------------
CString CGlimRegionViewerDlg::SettingsIniPath() const
{
	if (!m_iniPath.IsEmpty())
		return m_iniPath;
	// 미결정 상태(방어): exe 옆 기본 경로.
	return GetExeDir() + _T("\\GlimRegionViewer.ini");
}

// 쓰기 가능한 INI 경로를 한 번 결정한다.
//  1순위: exe 옆(GlimRegionViewer.ini) — 쓰기 가능하면 사용.
//  2순위: %APPDATA%\GlimRegionViewer\GlimRegionViewer.ini — Program Files 등 보호 경로 대비.
void CGlimRegionViewerDlg::ResolveIniPath()
{
	CString exeIni = GetExeDir() + _T("\\GlimRegionViewer.ini");

	// 프로브 쓰기로 exe 폴더 쓰기 가능 여부 확인.
	if (::WritePrivateProfileString(_T("__probe"), _T("w"), _T("1"), exeIni))
	{
		::WritePrivateProfileString(_T("__probe"), NULL, NULL, exeIni); // 프로브 섹션 제거
		m_iniPath = exeIni;
		return;
	}

	// 폴백: APPDATA 폴더 하위 GlimRegionViewer
	TCHAR appdata[MAX_PATH] = { 0 };
	if (SUCCEEDED(::SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdata)))
	{
		CString dir(appdata);
		dir += _T("\\GlimRegionViewer");
		::CreateDirectory(dir, NULL); // 이미 있으면 무시
		m_iniPath = dir + _T("\\GlimRegionViewer.ini");
	}
	else
	{
		// SHGetFolderPath 실패 시에도 exe 경로 유지(최선 노력).
		m_iniPath = exeIni;
	}
}

void CGlimRegionViewerDlg::SaveSettings()
{
	const CString ini = SettingsIniPath();

	// 마지막 폴더 경로(파일 목록의 첫 항목이 있는 디렉터리)
	CString lastFolder;
	if (!m_files.empty())
	{
		std::string first = m_files[0];
		size_t sl = first.find_last_of("\\/");
		lastFolder = ToCStr(sl != std::string::npos ? first.substr(0, sl) : std::string());
	}
	::WritePrivateProfileString(kIniSection, _T("LastFolder"), lastFolder, ini);

	// 프로파일(콤보 텍스트로 저장 → 복원 시 이름 매칭)
	{
		CString prof;
		int sel = m_comboProfile.GetCurSel();
		if (sel >= 0)
			m_comboProfile.GetLBText(sel, prof);
		::WritePrivateProfileString(kIniSection, _T("Profile"), prof, ini);
	}

	// 이진화 모드 + 모드별 파라미터(에딧 텍스트 그대로)
	{
		CString v;
		v.Format(_T("%d"), m_comboBinarize.GetCurSel());
		::WritePrivateProfileString(kIniSection, _T("BinarizeMode"), v, ini);
	}
	struct { LPCTSTR key; UINT id; } edits[] = {
		{ _T("ParamTH"),       IDC_EDIT_TH },
		{ _T("ParamOffset"),   IDC_EDIT_OFFSET },
		{ _T("ParamWkKernel"), IDC_EDIT_WK_KERNEL },
		{ _T("ParamWkBlur"),   IDC_EDIT_WK_BLUR },
		{ _T("ParamWkResp"),   IDC_EDIT_WK_RESP },
		{ _T("ParamPBlackTh"), IDC_EDIT_PBLACK_TH },
		{ _T("ParamPWhiteTh"), IDC_EDIT_PWHITE_TH },
		{ _T("ParamPKernel"),  IDC_EDIT_PKERNEL },
		{ _T("XScale"),        IDC_EDIT_XSCALE },
		{ _T("YScale"),        IDC_EDIT_YSCALE },
		{ _T("Threads"),       IDC_EDIT_THREADS },
	};
	for (size_t i = 0; i < sizeof(edits) / sizeof(edits[0]); ++i)
	{
		CWnd* w = GetDlgItem(edits[i].id);
		if (w)
		{
			CString s;
			w->GetWindowText(s);
			::WritePrivateProfileString(kIniSection, edits[i].key, s, ini);
		}
	}

	// 오버레이 토글 / 줌
	{
		CString v;
		v.Format(_T("%d"), m_showOverlay ? 1 : 0);
		::WritePrivateProfileString(kIniSection, _T("Overlay"), v, ini);
		v.Format(_T("%d"), m_zoom);
		::WritePrivateProfileString(kIniSection, _T("Zoom"), v, ini);
	}

	// 창 위치/크기(WINDOWPLACEMENT)
	WINDOWPLACEMENT wp;
	::ZeroMemory(&wp, sizeof(wp));
	wp.length = sizeof(wp);
	if (GetWindowPlacement(&wp))
	{
		struct { LPCTSTR key; int val; } wpv[] = {
			{ _T("Flags"),     (int)wp.flags },
			{ _T("ShowCmd"),   (int)wp.showCmd },
			{ _T("NormLeft"),  wp.rcNormalPosition.left },
			{ _T("NormTop"),   wp.rcNormalPosition.top },
			{ _T("NormRight"), wp.rcNormalPosition.right },
			{ _T("NormBottom"),wp.rcNormalPosition.bottom },
		};
		for (size_t i = 0; i < sizeof(wpv) / sizeof(wpv[0]); ++i)
		{
			CString v;
			v.Format(_T("%d"), wpv[i].val);
			::WritePrivateProfileString(kIniSectionWin, wpv[i].key, v, ini);
		}
	}
}

void CGlimRegionViewerDlg::LoadSettings()
{
	const CString ini = SettingsIniPath();
	if (::GetFileAttributes(ini) == INVALID_FILE_ATTRIBUTES)
		return; // INI 부재 → 기본값 유지

	TCHAR buf[1024];

	// 이진화 모드 먼저(에딧 가시성/값 반영 순서)
	int mode = ::GetPrivateProfileInt(kIniSection, _T("BinarizeMode"), 0, ini);
	if (mode >= 0 && mode < m_comboBinarize.GetCount())
	{
		m_comboBinarize.SetCurSel(mode);
		UpdateBinarizeParamVisibility();
	}

	// 파라미터 에딧(텍스트 그대로 복원)
	struct { LPCTSTR key; UINT id; } edits[] = {
		{ _T("ParamTH"),       IDC_EDIT_TH },
		{ _T("ParamOffset"),   IDC_EDIT_OFFSET },
		{ _T("ParamWkKernel"), IDC_EDIT_WK_KERNEL },
		{ _T("ParamWkBlur"),   IDC_EDIT_WK_BLUR },
		{ _T("ParamWkResp"),   IDC_EDIT_WK_RESP },
		{ _T("ParamPBlackTh"), IDC_EDIT_PBLACK_TH },
		{ _T("ParamPWhiteTh"), IDC_EDIT_PWHITE_TH },
		{ _T("ParamPKernel"),  IDC_EDIT_PKERNEL },
		{ _T("XScale"),        IDC_EDIT_XSCALE },
		{ _T("YScale"),        IDC_EDIT_YSCALE },
		{ _T("Threads"),       IDC_EDIT_THREADS },
	};
	for (size_t i = 0; i < sizeof(edits) / sizeof(edits[0]); ++i)
	{
		buf[0] = 0;
		::GetPrivateProfileString(kIniSection, edits[i].key, _T(""), buf, 1024, ini);
		if (buf[0] != 0)
		{
			CWnd* w = GetDlgItem(edits[i].id);
			if (w)
				w->SetWindowText(buf);
		}
	}
	m_xScale = GetEditDouble(IDC_EDIT_XSCALE, 1.0);
	m_yScale = GetEditDouble(IDC_EDIT_YSCALE, 1.0);

	// 오버레이 / 줌
	m_showOverlay = (::GetPrivateProfileInt(kIniSection, _T("Overlay"), 1, ini) != 0);
	CheckDlgButton(IDC_CHECK_OVERLAY, m_showOverlay ? BST_CHECKED : BST_UNCHECKED);
	m_zoom = ::GetPrivateProfileInt(kIniSection, _T("Zoom"), 4, ini);
	{
		const int zooms[] = { 1, 2, 4, 8, 16 };
		for (int i = 0; i < 5; ++i)
			if (zooms[i] == m_zoom) { m_comboZoom.SetCurSel(i); break; }
		m_detailView.SetZoom(m_zoom);
	}

	// 프로파일(이름 매칭 후 선택 + 로드)
	buf[0] = 0;
	::GetPrivateProfileString(kIniSection, _T("Profile"), _T(""), buf, 1024, ini);
	if (buf[0] != 0)
	{
		int idx = m_comboProfile.FindStringExact(-1, buf);
		if (idx >= 0)
		{
			m_comboProfile.SetCurSel(idx);
			LoadProfileSelection();
			m_cards.SetData(&m_results, &m_thumbCache, m_profileLoaded);
		}
	}

	// 창 위치/크기 복원(WINDOWPLACEMENT)
	{
		int nl = ::GetPrivateProfileInt(kIniSectionWin, _T("NormLeft"), INT_MIN, ini);
		if (nl != INT_MIN)
		{
			WINDOWPLACEMENT wp;
			::ZeroMemory(&wp, sizeof(wp));
			wp.length = sizeof(wp);
			wp.flags = (UINT)::GetPrivateProfileInt(kIniSectionWin, _T("Flags"), 0, ini);
			wp.showCmd = (UINT)::GetPrivateProfileInt(kIniSectionWin, _T("ShowCmd"), SW_SHOWNORMAL, ini);
			// 최소화 상태로는 복원하지 않음(정상 크기로 기동)
			if (wp.showCmd == SW_SHOWMINIMIZED)
				wp.showCmd = SW_SHOWNORMAL;
			wp.rcNormalPosition.left = nl;
			wp.rcNormalPosition.top = ::GetPrivateProfileInt(kIniSectionWin, _T("NormTop"), 0, ini);
			wp.rcNormalPosition.right = ::GetPrivateProfileInt(kIniSectionWin, _T("NormRight"), nl + 900, ini);
			wp.rcNormalPosition.bottom = ::GetPrivateProfileInt(kIniSectionWin, _T("NormBottom"), 0, ini);

			// 화면 밖으로 완전히 벗어난 경우는 무시(안전)
			CRect rc(wp.rcNormalPosition);
			if (rc.Width() > 200 && rc.Height() > 150)
				SetWindowPlacement(&wp);
		}
	}

	// 마지막 폴더 복원(존재하면 목록 로드)
	buf[0] = 0;
	::GetPrivateProfileString(kIniSection, _T("LastFolder"), _T(""), buf, 1024, ini);
	if (buf[0] != 0)
	{
		CString folder(buf);
		if (::GetFileAttributes(folder) != INVALID_FILE_ATTRIBUTES)
		{
			try { LoadFolder(folder); }
			catch (...) {}
		}
	}
}
