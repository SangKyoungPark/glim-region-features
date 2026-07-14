// ImageViewCtrl.cpp : 결과 탭 상세 이미지 뷰 구현
#include "stdafx.h"
#include "ImageViewCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CImageViewCtrl::CImageViewCtrl()
	: m_zoom(4)
{
}

CImageViewCtrl::~CImageViewCtrl()
{
}

BEGIN_MESSAGE_MAP(CImageViewCtrl, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CImageViewCtrl::CreateCtrl(CWnd* pParent, const CRect& rc, UINT id)
{
	LPCTSTR cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
		::LoadCursor(NULL, IDC_ARROW), NULL, NULL);
	return CWnd::Create(cls, _T(""), WS_CHILD | WS_VISIBLE | WS_BORDER, rc, pParent, id);
}

void CImageViewCtrl::SetImage(const cv::Mat& bgr)
{
	if (bgr.empty())
		m_img = cv::Mat();
	else if (bgr.type() == CV_8UC3)
		m_img = bgr.clone(); // 소유 복사(호출부 수명과 분리)
	else
		m_img = cv::Mat();
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

void CImageViewCtrl::SetZoom(int zoom)
{
	m_zoom = zoom;
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

void CImageViewCtrl::Clear()
{
	m_img = cv::Mat();
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

BOOL CImageViewCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}

void CImageViewCtrl::OnPaint()
{
	CPaintDC dc(this);
	CRect client;
	GetClientRect(&client);
	const int fw = client.Width();
	const int fh = client.Height();
	if (fw <= 0 || fh <= 0)
		return;

	CDC mem;
	mem.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, fw, fh);
	CBitmap* pOld = mem.SelectObject(&bmp);

	mem.FillSolidRect(0, 0, fw, fh, RGB(40, 40, 40));

	try
	{
		if (!m_img.empty())
		{
			const int w = m_img.cols;
			const int h = m_img.rows;
			const double sx = static_cast<double>(fw) / w;
			const double sy = static_cast<double>(fh) / h;
			double fit = (sx < sy) ? sx : sy;
			double scale = (m_zoom > 0) ? static_cast<double>(m_zoom) : fit;
			if (scale > fit)
				scale = fit;

			int dw = static_cast<int>(w * scale);
			int dh = static_cast<int>(h * scale);
			if (dw < 1) dw = 1;
			if (dh < 1) dh = 1;
			const int dx = (fw - dw) / 2;
			const int dy = (fh - dh) / 2;

			const int stride = ((w * 3 + 3) / 4) * 4;
			std::vector<BYTE> buf(static_cast<size_t>(stride) * h);
			for (int y = 0; y < h; ++y)
			{
				const unsigned char* src = m_img.ptr<unsigned char>(y);
				memcpy(&buf[static_cast<size_t>(y) * stride], src, static_cast<size_t>(w) * 3);
			}

			BITMAPINFO bmi;
			::ZeroMemory(&bmi, sizeof(bmi));
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = w;
			bmi.bmiHeader.biHeight = -h;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 24;
			bmi.bmiHeader.biCompression = BI_RGB;

			const int oldMode = mem.SetStretchBltMode(COLORONCOLOR);
			::StretchDIBits(mem.GetSafeHdc(),
				dx, dy, dw, dh,
				0, 0, w, h,
				&buf[0], &bmi, DIB_RGB_COLORS, SRCCOPY);
			mem.SetStretchBltMode(oldMode);
		}
		else
		{
			mem.SetBkMode(TRANSPARENT);
			mem.SetTextColor(RGB(200, 200, 200));
			CRect r(0, 0, fw, fh);
			mem.DrawText(_T("카드를 선택하면 상세 이미지가 표시됩니다."),
				&r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}
	}
	catch (...)
	{
	}

	dc.BitBlt(0, 0, fw, fh, &mem, 0, 0, SRCCOPY);
	mem.SelectObject(pOld);
}
