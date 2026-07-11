#pragma once
// GlimRegionViewer.h : 애플리케이션 클래스
#include "resource.h"

class CGlimRegionViewerApp : public CWinApp
{
public:
	CGlimRegionViewerApp();

	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};

extern CGlimRegionViewerApp theApp;
