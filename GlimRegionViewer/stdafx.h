#pragma once
// stdafx.h : 자주 쓰는 시스템/MFC 헤더 미리 컴파일용.

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

#ifndef _AFX_ALL_WARNINGS
#define _AFX_ALL_WARNINGS
#endif

#include <afxwin.h>         // MFC 코어 및 표준 구성 요소
#include <afxext.h>         // MFC 확장
#include <afxdisp.h>        // MFC 자동화

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>       // 인터넷 익스플로러 4 공용 컨트롤
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>         // MFC 공용 컨트롤(CListCtrl 등)
#endif

#include <afxdialogex.h>    // CDialogEx
