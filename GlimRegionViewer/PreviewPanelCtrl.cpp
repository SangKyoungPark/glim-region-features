// PreviewPanelCtrl.cpp : 설정 탭 미리보기 패널 구현
#include "stdafx.h"
#include "PreviewPanelCtrl.h"
#include "GrfViewSupport.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CPreviewPanelCtrl::CPreviewPanelCtrl()
{
}

CPreviewPanelCtrl::~CPreviewPanelCtrl()
{
}

BEGIN_MESSAGE_MAP(CPreviewPanelCtrl, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CPreviewPanelCtrl::CreatePanel(CWnd* pParent, const CRect& rc, UINT id)
{
	// 커스텀 클래스 등록(더블버퍼 그리므로 배경 브러시 없음)
	LPCTSTR cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
		::LoadCursor(NULL, IDC_ARROW), NULL, NULL);
	return CWnd::Create(cls, _T(""), WS_CHILD | WS_VISIBLE | WS_BORDER,
		rc, pParent, id);
}

void CPreviewPanelCtrl::SetSlots(const std::vector<cv::Mat>& images, const std::vector<CString>& labels)
{
	m_images = images;
	m_labels = labels;
	if (static_cast<int>(m_images.size()) > kMaxSlots)
		m_images.resize(kMaxSlots);
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

void CPreviewPanelCtrl::ClearSlots()
{
	m_images.clear();
	m_labels.clear();
	if (GetSafeHwnd())
		Invalidate(FALSE);
}

BOOL CPreviewPanelCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE; // OnPaint 에서 전체를 다시 그림(깜빡임 방지)
}

void CPreviewPanelCtrl::OnPaint()
{
	CPaintDC dc(this);
	CRect client;
	GetClientRect(&client);
	if (client.Width() <= 0 || client.Height() <= 0)
		return;

	// 더블버퍼
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

void CPreviewPanelCtrl::DrawContent(CDC* pDC, const CRect& client)
{
	pDC->FillSolidRect(client, RGB(32, 32, 32));

	const int slots = (m_images.empty()) ? 2 : static_cast<int>(m_images.size());
	const int shown = (slots < 1) ? 1 : (slots > kMaxSlots ? kMaxSlots : slots);

	const int pad = 8;
	const int labelH = 18;
	const int cellW = (client.Width() - pad * (shown + 1)) / shown;

	CFont font;
	font.CreatePointFont(90, _T("Segoe UI"));
	CFont* pOldFont = pDC->SelectObject(&font);
	pDC->SetBkMode(TRANSPARENT);

	for (int i = 0; i < shown; ++i)
	{
		const int x0 = pad + i * (cellW + pad);
		CRect cell(x0, pad, x0 + cellW, client.bottom - pad);

		// 셀 배경/테두리
		pDC->FillSolidRect(cell, RGB(20, 20, 20));
		CBrush border(RGB(90, 90, 90));
		pDC->FrameRect(cell, &border);

		// 라벨
		CString label;
		if (i < static_cast<int>(m_labels.size()))
			label = m_labels[i];
		else if (i == 0)
			label = _T("Original");
		else if (i == 1)
			label = _T("Binarized");
		else
			label = _T("Slot");

		CRect labelRc(cell.left, cell.top, cell.right, cell.top + labelH);
		pDC->SetTextColor(RGB(210, 210, 210));
		pDC->DrawText(label, &labelRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		// 이미지 영역
		CRect imgRc(cell.left + 3, cell.top + labelH, cell.right - 3, cell.bottom - 3);
		if (i < static_cast<int>(m_images.size()) && !m_images[i].empty())
		{
			// 원본(슬롯0)은 부드럽게, 이진화는 픽셀 경계 유지
			const bool nearest = (i >= 1);
			GrfView::DrawMatFit(pDC, m_images[i], imgRc, nearest);
		}
		else
		{
			pDC->SetTextColor(RGB(120, 120, 120));
			pDC->DrawText(_T("(no image)"), &imgRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}
	}

	pDC->SelectObject(pOldFont);
}
