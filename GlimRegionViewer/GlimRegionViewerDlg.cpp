// GlimRegionViewerDlg.cpp : 메인 다이얼로그 구현
#include "stdafx.h"
#include "GlimRegionViewer.h"
#include "GlimRegionViewerDlg.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ------------------------------------------------------------------
// 레이아웃 상수(픽셀). 클라이언트 1120 x 680 기준.
// ------------------------------------------------------------------
namespace {
	const int kClientW = 1120;
	const int kClientH = 680;

	const int kTopY = 8;
	const int kTopH = 24;

	const int kContentY = 40;
	const int kContentH = 610;

	const int kFilesX = 10;
	const int kFilesW = 220;

	const int kImgX = 240;
	const int kImgW = 520;
	const int kImgH = 520;

	const int kFeatX = 772;
	const int kFeatW = 338;

	// 문자열 → CString 변환(멀티바이트)
	CString ToCStr(const std::string& s)
	{
		return CString(s.c_str());
	}

	std::string ToStd(const CString& s)
	{
		return std::string((LPCTSTR)s);
	}

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
	, m_curIndex(-1)
	, m_profileLoaded(false)
	, m_zoom(4)
	, m_showOverlay(true)
	, m_highlightRegion(-1)
{
	m_hIcon = AfxGetApp()->LoadStandardIcon(IDI_APPLICATION);
	m_imgFrameRect = CRect(kImgX, kContentY, kImgX + kImgW, kContentY + kImgH);
}

void CGlimRegionViewerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CGlimRegionViewerDlg, CDialogEx)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDC_BTN_OPEN_IMAGE, &CGlimRegionViewerDlg::OnBnClickedOpenImage)
	ON_BN_CLICKED(IDC_BTN_OPEN_FOLDER, &CGlimRegionViewerDlg::OnBnClickedOpenFolder)
	ON_BN_CLICKED(IDC_BTN_EXPORT_CSV, &CGlimRegionViewerDlg::OnBnClickedExportCsv)
	ON_BN_CLICKED(IDC_CHECK_OVERLAY, &CGlimRegionViewerDlg::OnBnClickedOverlay)
	ON_CBN_SELCHANGE(IDC_COMBO_ZOOM, &CGlimRegionViewerDlg::OnZoomChanged)
	ON_CBN_SELCHANGE(IDC_COMBO_PROFILE, &CGlimRegionViewerDlg::OnProfileChanged)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FILES, &CGlimRegionViewerDlg::OnFileListItemChanged)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FEATURES, &CGlimRegionViewerDlg::OnFeatureListItemChanged)
END_MESSAGE_MAP()

// ------------------------------------------------------------------
BOOL CGlimRegionViewerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	// 클라이언트 크기 고정 후 화면 중앙 배치
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
		SetupControls();
		PopulateProfileCombo();
	}
	catch (...)
	{
		SetStatus(_T("초기화 중 오류가 발생했습니다."));
	}

	SetStatus(_T("Ready. [Open Image] 또는 [Open Folder] 로 시작하세요."));
	return TRUE;
}

// 동적 컨트롤 생성 + 리스트 컬럼 셋업
void CGlimRegionViewerDlg::SetupControls()
{
	CFont* pFont = GetFont();

	// --- 상단 버튼/콤보/체크 ---
	struct { int id; LPCTSTR text; int x; int w; } btns[] = {
		{ IDC_BTN_OPEN_IMAGE,  _T("Open Image"),  10,  100 },
		{ IDC_BTN_OPEN_FOLDER, _T("Open Folder"), 115, 100 },
	};
	for (int i = 0; i < 2; ++i)
	{
		CButton* b = new CButton();
		b->Create(btns[i].text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
			CRect(btns[i].x, kTopY, btns[i].x + btns[i].w, kTopY + kTopH), this, btns[i].id);
		b->SetFont(pFont);
	}

	// Zoom 라벨 + 콤보
	{
		CStatic* lbl = new CStatic();
		lbl->Create(_T("Zoom"), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
			CRect(225, kTopY, 260, kTopY + kTopH), this, IDC_STATIC_ZOOM_LABEL);
		lbl->SetFont(pFont);

		m_comboZoom.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
			CRect(262, kTopY, 262 + 70, kTopY + 200), this, IDC_COMBO_ZOOM);
		m_comboZoom.SetFont(pFont);
		const LPCTSTR zooms[] = { _T("1x"), _T("2x"), _T("4x"), _T("8x"), _T("16x") };
		for (int i = 0; i < 5; ++i)
			m_comboZoom.AddString(zooms[i]);
		m_comboZoom.SetCurSel(2); // 4x
	}

	// Overlay 체크
	{
		CButton* chk = new CButton();
		chk->Create(_T("Show overlay"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
			CRect(345, kTopY, 345 + 110, kTopY + kTopH), this, IDC_CHECK_OVERLAY);
		chk->SetFont(pFont);
		chk->SetCheck(BST_CHECKED);
	}

	// Profile 라벨 + 콤보
	{
		CStatic* lbl = new CStatic();
		lbl->Create(_T("Profile"), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
			CRect(465, kTopY, 510, kTopY + kTopH), this, IDC_STATIC_PROFILE_LABEL);
		lbl->SetFont(pFont);

		m_comboProfile.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
			CRect(512, kTopY, 512 + 170, kTopY + 200), this, IDC_COMBO_PROFILE);
		m_comboProfile.SetFont(pFont);
	}

	// Export CSV 버튼
	{
		CButton* b = new CButton();
		b->Create(_T("Export CSV"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
			CRect(700, kTopY, 700 + 120, kTopY + kTopH), this, IDC_BTN_EXPORT_CSV);
		b->SetFont(pFont);
	}

	// --- 좌측 파일 리스트 ---
	m_listFiles.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
		CRect(kFilesX, kContentY, kFilesX + kFilesW, kContentY + kContentH), this, IDC_LIST_FILES);
	m_listFiles.SetFont(pFont);
	m_listFiles.SetExtendedStyle(m_listFiles.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_listFiles.InsertColumn(0, _T("File"), LVCFMT_LEFT, kFilesW - 24);

	// --- 우측 특징값 리스트 ---
	m_listFeatures.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
		CRect(kFeatX, kContentY, kFeatX + kFeatW, kContentY + kContentH), this, IDC_LIST_FEATURES);
	m_listFeatures.SetFont(pFont);
	m_listFeatures.SetExtendedStyle(m_listFeatures.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_listFeatures.InsertColumn(0, _T("Idx"), LVCFMT_RIGHT, 36);
	m_listFeatures.InsertColumn(1, _T("Area"), LVCFMT_RIGHT, 56);
	m_listFeatures.InsertColumn(2, _T("Circ"), LVCFMT_RIGHT, 48);
	m_listFeatures.InsertColumn(3, _T("Conv"), LVCFMT_RIGHT, 48);
	m_listFeatures.InsertColumn(4, _T("Round"), LVCFMT_RIGHT, 52);
	m_listFeatures.InsertColumn(5, _T("Code"), LVCFMT_LEFT, 88);

	// 상태 라벨 위치 조정
	CWnd* status = GetDlgItem(IDC_STATIC_STATUS);
	if (status)
		status->MoveWindow(10, kClientH - 24, kClientW - 20, 18);
}

void CGlimRegionViewerDlg::PopulateProfileCombo()
{
	m_comboProfile.ResetContent();
	m_profilePaths.clear();

	// (none) 항목
	m_comboProfile.AddString(_T("(none)"));
	m_profilePaths.push_back(std::string());

	// 후보 폴더에서 *.ini 탐색
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
			break; // 첫 유효 폴더만 사용
	}

	m_comboProfile.SetCurSel(0);
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

// ------------------------------------------------------------------
// 렌더링
// ------------------------------------------------------------------
void CGlimRegionViewerDlg::OnPaint()
{
	CPaintDC dc(this);
	try
	{
		DrawImageView(&dc);
	}
	catch (...)
	{
	}
}

void CGlimRegionViewerDlg::DrawImageView(CDC* pDC)
{
	// [feat: 이미지 뷰 렌더링] 구현 예정 — 지금은 테두리 + 안내만
	pDC->FillSolidRect(m_imgFrameRect, RGB(40, 40, 40));
	CBrush border(RGB(120, 120, 120));
	pDC->FrameRect(m_imgFrameRect, &border);

	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(RGB(200, 200, 200));
	CRect r = m_imgFrameRect;
	pDC->DrawText(_T("No image loaded"), &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

cv::Mat CGlimRegionViewerDlg::BuildOverlayMat()
{
	// [feat: 오버레이] 구현 예정
	return cv::Mat();
}

bool CGlimRegionViewerDlg::DrawMatToDC(CDC* pDC, const cv::Mat& bgr, const CRect& dest)
{
	// [feat: 이미지 뷰 렌더링] 구현 예정
	(void)pDC; (void)bgr; (void)dest;
	return false;
}

// ------------------------------------------------------------------
// 데이터 흐름 (스텁 — 이후 커밋에서 구현)
// ------------------------------------------------------------------
void CGlimRegionViewerDlg::LoadFolder(const CString& dir)
{
	(void)dir; // [feat: 폴더 로드] 구현 예정
}

void CGlimRegionViewerDlg::LoadImageAt(int index)
{
	(void)index; // [feat: 이미지 로드] 구현 예정
}

void CGlimRegionViewerDlg::AnalyzeCurrent()
{
	// [feat: 이미지 로드] 구현 예정
}

void CGlimRegionViewerDlg::UpdateFeatureList()
{
	// [feat: 특징값 패널] 구현 예정
}

void CGlimRegionViewerDlg::LoadProfileSelection()
{
	// [feat: 프로파일 연동] 구현 예정
}

// ------------------------------------------------------------------
// 핸들러 (스텁)
// ------------------------------------------------------------------
void CGlimRegionViewerDlg::OnBnClickedOpenImage()
{
	// [feat: 단일 이미지 로드] 구현 예정
}

void CGlimRegionViewerDlg::OnBnClickedOpenFolder()
{
	// [feat: 폴더 로드] 구현 예정
}

void CGlimRegionViewerDlg::OnBnClickedExportCsv()
{
	// [feat: CSV 내보내기] 구현 예정
}

void CGlimRegionViewerDlg::OnBnClickedOverlay()
{
	// [feat: 오버레이] 구현 예정
}

void CGlimRegionViewerDlg::OnZoomChanged()
{
	// [feat: 이미지 뷰 렌더링] 구현 예정
}

void CGlimRegionViewerDlg::OnProfileChanged()
{
	// [feat: 프로파일 연동] 구현 예정
}

void CGlimRegionViewerDlg::OnFileListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	(void)pNMHDR;
	*pResult = 0;
	// [feat: 폴더 로드] 구현 예정
}

void CGlimRegionViewerDlg::OnFeatureListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	(void)pNMHDR;
	*pResult = 0;
	// [feat: 오버레이] 구현 예정
}
