// GlimRegionViewer.cpp : 애플리케이션 진입점
#include "stdafx.h"
#include "GlimRegionViewer.h"
#include "GlimRegionViewerDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CGlimRegionViewerApp, CWinApp)
END_MESSAGE_MAP()

CGlimRegionViewerApp::CGlimRegionViewerApp()
{
}

CGlimRegionViewerApp theApp;

BOOL CGlimRegionViewerApp::InitInstance()
{
	// 공용 컨트롤 초기화(CListCtrl 등)
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();
	AfxEnableControlContainer();

	try
	{
		CGlimRegionViewerDlg dlg;
		m_pMainWnd = &dlg;
		dlg.DoModal();
	}
	catch (...)
	{
		AfxMessageBox(_T("Unexpected error while running the dialog."));
	}

	// 다이얼로그가 닫히면 메시지 펌프 없이 종료.
	return FALSE;
}
