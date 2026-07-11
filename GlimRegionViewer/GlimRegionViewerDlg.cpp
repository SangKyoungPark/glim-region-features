// GlimRegionViewerDlg.cpp : 메인 다이얼로그 구현
#include "stdafx.h"
#include "GlimRegionViewer.h"
#include "GlimRegionViewerDlg.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <shlobj.h>   // SHBrowseForFolder

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
	// 더블버퍼링: 프레임 크기 메모리 DC 에 그린 뒤 한 번에 BitBlt(깜빡임 방지)
	const CRect fr = m_imgFrameRect;
	const int fw = fr.Width();
	const int fh = fr.Height();
	if (fw <= 0 || fh <= 0)
		return;

	CDC mem;
	mem.CreateCompatibleDC(pDC);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(pDC, fw, fh);
	CBitmap* pOld = mem.SelectObject(&bmp);

	mem.FillSolidRect(0, 0, fw, fh, RGB(40, 40, 40));

	cv::Mat disp = BuildOverlayMat();
	if (!disp.empty())
	{
		// 정수배 확대. 이미지가 프레임보다 크면 프레임에 맞게 축소(fit).
		const double sx = static_cast<double>(fw) / disp.cols;
		const double sy = static_cast<double>(fh) / disp.rows;
		double fit = (sx < sy) ? sx : sy;
		double scale = (m_zoom > 0) ? static_cast<double>(m_zoom) : 1.0;
		if (scale > fit)
			scale = fit;

		int dw = static_cast<int>(disp.cols * scale);
		int dh = static_cast<int>(disp.rows * scale);
		if (dw < 1) dw = 1;
		if (dh < 1) dh = 1;
		const int dx = (fw - dw) / 2;
		const int dy = (fh - dh) / 2;
		CRect dest(dx, dy, dx + dw, dy + dh);
		DrawMatToDC(&mem, disp, dest);
	}
	else
	{
		mem.SetBkMode(TRANSPARENT);
		mem.SetTextColor(RGB(200, 200, 200));
		CRect r(0, 0, fw, fh);
		mem.DrawText(_T("No image loaded"), &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}

	CBrush border(RGB(120, 120, 120));
	CRect full(0, 0, fw, fh);
	mem.FrameRect(&full, &border);

	pDC->BitBlt(fr.left, fr.top, fw, fh, &mem, 0, 0, SRCCOPY);
	mem.SelectObject(pOld);
}

cv::Mat CGlimRegionViewerDlg::BuildOverlayMat()
{
	if (m_binImage.empty())
		return cv::Mat();

	cv::Mat bgr;
	cv::cvtColor(m_binImage, bgr, cv::COLOR_GRAY2BGR);

	if (!m_showOverlay)
		return bgr;

	// Region 외곽/구멍 컨투어 오버레이. 컨투어는 전체 이미지 좌표(OpenCV x=col,y=row).
	try
	{
		// 1) 일반 Region: 초록
		for (size_t i = 0; i < m_regions.size(); ++i)
		{
			if (static_cast<int>(i) == m_highlightRegion)
				continue;
			cv::drawContours(bgr, m_regions[i].AllContours(), -1, cv::Scalar(0, 200, 0), 1);
		}
		// 2) 강조 Region: 빨강(두껍게, 맨 위)
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

bool CGlimRegionViewerDlg::DrawMatToDC(CDC* pDC, const cv::Mat& bgr, const CRect& dest)
{
	// cv::Mat(BGR,8UC3) → 4바이트 정렬 top-down DIB → StretchDIBits(COLORONCOLOR=최근접)
	if (bgr.empty() || bgr.type() != CV_8UC3)
		return false;

	const int w = bgr.cols;
	const int h = bgr.rows;
	const int stride = ((w * 3 + 3) / 4) * 4; // 임의 폭 대응(4바이트 정렬)

	std::vector<BYTE> buf(static_cast<size_t>(stride) * h);
	for (int y = 0; y < h; ++y)
	{
		const unsigned char* src = bgr.ptr<unsigned char>(y);
		memcpy(&buf[static_cast<size_t>(y) * stride], src, static_cast<size_t>(w) * 3);
	}

	BITMAPINFO bmi;
	::ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = -h; // 음수 = top-down(cv::Mat 행순서와 일치)
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;

	const int oldMode = pDC->SetStretchBltMode(COLORONCOLOR); // 이진 픽셀 경계 유지
	::StretchDIBits(pDC->GetSafeHdc(),
		dest.left, dest.top, dest.Width(), dest.Height(),
		0, 0, w, h,
		&buf[0], &bmi, DIB_RGB_COLORS, SRCCOPY);
	pDC->SetStretchBltMode(oldMode);
	return true;
}

// ------------------------------------------------------------------
// 데이터 흐름 (스텁 — 이후 커밋에서 구현)
// ------------------------------------------------------------------
void CGlimRegionViewerDlg::LoadFolder(const CString& dir)
{
	m_files.clear();
	m_listFiles.DeleteAllItems();
	m_curIndex = -1;

	// 폴더 내 지원 이미지 수집(확장자 판정은 라이브러리와 공유)
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
		// 선택 변경이 OnFileListItemChanged → LoadImageAt 를 유발
		m_listFiles.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_listFiles.SetFocus();
	}
	else
	{
		m_binImage = cv::Mat();
		m_regions.clear();
		m_features.clear();
		UpdateFeatureList();
		InvalidateRect(m_imgFrameRect);
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
			InvalidateRect(m_imgFrameRect);
			return;
		}

		// 완전 이진이 아닐 수 있으므로 0/255 정규화
		cv::threshold(gray, m_binImage, 127.0, 255.0, cv::THRESH_BINARY);

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

	InvalidateRect(m_imgFrameRect);
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
	// 선택 → OnFileListItemChanged → LoadImageAt(0)
	m_listFiles.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

void CGlimRegionViewerDlg::OnBnClickedOpenFolder()
{
	TCHAR pathBuf[MAX_PATH] = { 0 };
	BROWSEINFO bi;
	::ZeroMemory(&bi, sizeof(bi));
	bi.hwndOwner = GetSafeHwnd();
	bi.pszDisplayName = pathBuf;
	bi.lpszTitle = _T("Select a folder containing binary images");
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
	// [feat: CSV 내보내기] 구현 예정
}

void CGlimRegionViewerDlg::OnBnClickedOverlay()
{
	m_showOverlay = (IsDlgButtonChecked(IDC_CHECK_OVERLAY) == BST_CHECKED);
	InvalidateRect(m_imgFrameRect);
}

void CGlimRegionViewerDlg::OnZoomChanged()
{
	const int sel = m_comboZoom.GetCurSel();
	const int zooms[] = { 1, 2, 4, 8, 16 };
	if (sel >= 0 && sel < 5)
		m_zoom = zooms[sel];
	InvalidateRect(m_imgFrameRect);
}

void CGlimRegionViewerDlg::OnProfileChanged()
{
	// [feat: 프로파일 연동] 구현 예정
}

void CGlimRegionViewerDlg::OnFileListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;

	// 선택이 새로 켜진 항목만 로드(위/아래 방향키 이동도 여기로 들어옴)
	if ((pnmv->uNewState & LVIS_SELECTED) && !(pnmv->uOldState & LVIS_SELECTED))
	{
		LoadImageAt(pnmv->iItem);
	}
}

void CGlimRegionViewerDlg::OnFeatureListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;

	// 특징값 행(=Region 인덱스) 선택 → 해당 Region 강조
	if ((pnmv->uNewState & LVIS_SELECTED) && !(pnmv->uOldState & LVIS_SELECTED))
	{
		m_highlightRegion = pnmv->iItem;
		InvalidateRect(m_imgFrameRect);
	}
}
