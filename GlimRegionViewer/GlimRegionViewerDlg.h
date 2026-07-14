#pragma once
// GlimRegionViewerDlg.h : 메인 다이얼로그(탭 기반 분석 대시보드)
//  탭 4개: 홈 / 설정 / 결과 / 분석. CTabCtrl + 컨트롤 그룹 Show/Hide 방식.
//  컨트롤은 전부 동적 생성(.rc 는 상태바 static 만 유지).
//  좌표계: 화면 표시는 픽셀. Region 컨투어는 OpenCV (x=col,y=row).

#include <vector>
#include <string>
#include <opencv2/core.hpp>
#include "resource.h"
#include "GlimRegionFeatures.h"
#include "GrfViewSupport.h"
#include "PreviewPanelCtrl.h"
#include "ResultCardCtrl.h"
#include "ChartPanelCtrl.h"
#include "ImageViewCtrl.h"

class CGlimRegionViewerDlg : public CDialogEx
{
public:
	CGlimRegionViewerDlg(CWnd* pParent = NULL);

	enum { IDD = IDD_GLIMREGIONVIEWER_DIALOG };

	// 탭 인덱스
	enum { TAB_HOME = 0, TAB_SETTINGS = 1, TAB_RESULTS = 2, TAB_ANALYSIS = 3, TAB_COUNT = 4 };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

	afx_msg void OnBnClickedOpenImage();
	afx_msg void OnBnClickedOpenFolder();
	afx_msg void OnBnClickedExportCsv();
	afx_msg void OnBnClickedOverlay();
	afx_msg void OnBnClickedAnalyze();
	afx_msg void OnZoomChanged();
	afx_msg void OnProfileChanged();
	afx_msg void OnBinarizeChanged();
	afx_msg void OnHistFeatChanged();
	afx_msg void OnParamEditChanged();     // 이진화 파라미터/스케일 에딧 변경 → 미리보기 갱신
	afx_msg void OnFileListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnFeatureListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnTabSelChange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnCardSelected(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

private:
	// --- UI 구성 ---
	void SetupTabs();
	void SetupControls();          // 동적 컨트롤 생성 + 리스트 컬럼 셋업
	void BuildPageControlLists();  // 탭별 소속 컨트롤 ID 목록
	void ShowPage(int tab);        // 탭 전환(그룹 Show/Hide)
	void UpdateBinarizeParamVisibility(); // 모드별 파라미터 에딧 표시/숨김
	void PopulateProfileCombo();   // profiles\*.ini 탐색
	void PopulateHistFeatCombo();
	void SetStatus(const CString& text);
	CString CurrentProfilePath();

	// --- 데이터 흐름 ---
	void LoadFolder(const CString& dir);
	void LoadImageAt(int index);   // m_files[index] 로드(상세 뷰용)
	void AnalyzeCurrent();         // 현재 이미지 Region 추출 + 특징값(상세 뷰)
	void UpdateFeatureList();       // 결과 탭 상세 특징값 패널 갱신
	void UpdatePreview();           // 설정 탭 미리보기 재계산
	void RunAnalysis();             // 폴더 전체 분석 → 결과/분석 탭 채우기
	void UpdateHomeSummary();
	void LoadProfileSelection();
	void RefreshDetailView();      // 상세 이미지 뷰 갱신(오버레이 반영)

	// --- 렌더링(결과 탭 상세 이미지) ---
	cv::Mat BuildOverlayMat();

	// --- 상태 ---
	Grf::BinarizeParams CurrentBinarizeParams() const;
	int  GetEditInt(UINT id, int def) const;
	double GetEditDouble(UINT id, double def) const;

	// 탭
	CTabCtrl m_tab;
	int m_curTab;
	std::vector<int> m_pageCtrls[TAB_COUNT];

	// 컨트롤
	CListCtrl m_listFiles;
	CListCtrl m_listFeatures;
	CComboBox m_comboZoom;
	CComboBox m_comboProfile;
	CComboBox m_comboBinarize;
	CComboBox m_comboHistFeat;
	CEdit m_editThreads;
	CEdit m_editXScale;
	CEdit m_editYScale;
	CEdit m_editTh;
	CEdit m_editOffset;
	CEdit m_editWkKernel;
	CEdit m_editWkBlur;
	CEdit m_editWkResp;

	CPreviewPanelCtrl m_preview;
	CResultCardCtrl m_cards;
	CChartPanelCtrl m_chart;
	CImageViewCtrl m_detailView;
	CStatic m_staticHome;

	// 데이터
	std::vector<std::string> m_files;   // 폴더 내 이미지 전체 경로
	int m_curIndex;                     // 현재 선택 인덱스(-1 없음)

	cv::Mat m_binImage;                 // 현재 이미지(0/255) — 상세 뷰
	std::vector<Grf::Region> m_regions;
	std::vector<Grf::FeatureVector> m_features;

	std::vector<GrfView::RegionResult> m_results; // 폴더 전체 분석 결과(카드/차트/홈)
	GrfView::ThumbCache m_thumbCache;

	Grf::ProfileLoader m_profile;
	bool m_profileLoaded;
	std::vector<std::string> m_profilePaths; // 콤보 인덱스 → ini 경로

	// TODO(Projection 통합): 엔진에 X/Y Scale 옵션이 추가되면 아래 값을
	//  BinarizeParams/CsvExporter 로 전달하여 mm 환산에 사용한다(현재는 UI 보관만).
	double m_xScale;
	double m_yScale;

	int m_zoom;              // 정수 배율(기본 4)
	bool m_showOverlay;      // 오버레이 on/off
	int m_highlightRegion;   // 강조할 Region 인덱스(-1 없음)

	CRect m_imgFrameRect;    // 결과 탭 상세 이미지 영역
	HICON m_hIcon;
};
