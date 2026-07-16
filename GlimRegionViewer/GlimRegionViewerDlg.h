#pragma once
// GlimRegionViewerDlg.h : 메인 다이얼로그(탭 기반 분석 대시보드)
//  탭 4개: 홈 / 설정 / 결과 / 분석. CTabCtrl + 컨트롤 그룹 Show/Hide 방식.
//  컨트롤은 전부 동적 생성(.rc 는 상태바 static 만 유지).
//  좌표계: 화면 표시는 픽셀. Region 컨투어는 OpenCV (x=col,y=row).

#include <vector>
#include <string>
#include <thread>
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

	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
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
	// m_files[index] 로드(상세 뷰용). params!=NULL 이면 그 파라미터로,
	//  NULL 이면 현재 UI 값(CurrentBinarizeParams)으로 이진화한다.
	void LoadImageAt(int index, const Grf::BinarizeParams* params = NULL);
	// 현재 이미지 Region 추출 + 특징값(상세 뷰, 채널별). params 로 이진화.
	void AnalyzeCurrent(const cv::Mat& gray, const Grf::BinarizeParams& params);
	void UpdateFeatureList();       // 결과 탭 상세 특징값 패널 갱신
	void UpdatePreview();           // 설정 탭 미리보기 재계산
	void RunAnalysis();             // 폴더 전체 분석 → 결과/분석 탭 채우기
	void UpdateHomeSummary();
	void LoadProfileSelection();
	void RefreshDetailView();      // 상세 이미지 뷰 갱신(오버레이 반영)
	void FinalizeAnalysis();       // 워커 스레드 분석 완료 후 결과/차트/홈/타이틀 반영

	void SetAnalysisUIEnabled(bool enabled); // 분석 중 관련 컨트롤 잠금/해제

	// --- 설정 영속화(INI) ---
	void ResolveIniPath();             // 쓰기 가능 INI 경로 결정(exe 옆 우선, 불가 시 %APPDATA%)
	CString SettingsIniPath() const;   // 결정된 INI 경로
	void LoadSettings();               // 시작 시 복원(부재/손상 시 기본값)
	void SaveSettings();               // 종료 시 저장
	void UpdateWindowTitle();          // 타이틀바 버전 + 분석 상태 반영

	// --- 렌더링(결과 탭 상세 이미지) ---
	cv::Mat BuildOverlayMat();

	// --- 상태 ---
	Grf::BinarizeParams CurrentBinarizeParams() const;
	bool IsProjectionMode() const;      // 이진화 콤보가 Projection(모드 6) 선택 상태인지
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
	CEdit m_editPBlackTh;   // PROJECTION 흑 임계
	CEdit m_editPWhiteTh;   // PROJECTION 백 임계
	CEdit m_editPKernel;    // PROJECTION 국소평균 커널

	CPreviewPanelCtrl m_preview;
	CResultCardCtrl m_cards;
	CChartPanelCtrl m_chart;
	CImageViewCtrl m_detailView;
	CStatic m_staticHome;

	// 데이터
	std::vector<std::string> m_files;   // 폴더 내 이미지 전체 경로
	int m_curIndex;                     // 현재 선택 인덱스(-1 없음)

	cv::Mat m_binImage;                 // 현재 이미지(0/255) — 상세 뷰(채널 합집합)
	std::vector<Grf::Region> m_regions;
	std::vector<Grf::FeatureVector> m_features;
	std::vector<char> m_regionChannels; // m_regions 병렬: 각 Region 의 채널('B'/'W')
	bool m_activeProjection;            // m_regions 를 만든 파라미터가 PROJECTION 이었는지(오버레이/뱃지 색 판정)
	bool m_lastAnalysisProjection;      // 마지막 폴더 분석이 PROJECTION 모드였는지(홈 요약용)
	Grf::BinarizeParams m_analysisParams; // RunAnalysis 시점 파라미터 스냅샷(카드 클릭 재분석에 재사용)
	bool m_hasAnalysisParams;           // 스냅샷 유효 여부

	std::vector<GrfView::RegionResult> m_results;        // 폴더 전체 분석 결과(카드/차트/홈) — UI 스레드 전용
	std::vector<GrfView::RegionResult> m_pendingResults; // 워커 스레드 전용 출력 버퍼(완료 후 swap → m_results)
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

	// --- 분석 진행 표시(워커 스레드 + 타이머 폴링) ---
	std::thread m_analysisThread;     // 폴더 분석 워커
	volatile long m_progress;         // 완료 파일 수(InterlockedIncrement, UI는 읽기만)
	volatile bool m_analysisDone;     // 워커 완료 플래그
	volatile bool m_analysisFailed;   // 워커 예외 플래그
	bool m_analyzing;                 // 분석 진행 중(재진입 방지, UI 스레드 전용)
	int m_analysisTotal;              // 전체 파일 수(진행률 표시)

	CString m_iniPath;                // 결정된 설정 파일 경로(ResolveIniPath)
};
