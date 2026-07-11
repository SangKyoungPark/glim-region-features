#pragma once
// GlimRegionViewerDlg.h : 메인 다이얼로그
//  128x128 이진화 이미지를 단일/폴더로 로드해 확대 표시 + Region 오버레이 +
//  특징값 패널 + 프로파일 Score/분류 + CSV 내보내기.
//  좌표계: 화면 표시는 픽셀. Region 컨투어는 OpenCV (x=col,y=row).

#include <vector>
#include <string>
#include <opencv2/core.hpp>
#include "GlimRegionFeatures.h"

class CGlimRegionViewerDlg : public CDialogEx
{
public:
	CGlimRegionViewerDlg(CWnd* pParent = NULL);

	enum { IDD = IDD_GLIMREGIONVIEWER_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

	afx_msg void OnPaint();
	afx_msg void OnBnClickedOpenImage();
	afx_msg void OnBnClickedOpenFolder();
	afx_msg void OnBnClickedExportCsv();
	afx_msg void OnBnClickedOverlay();
	afx_msg void OnZoomChanged();
	afx_msg void OnProfileChanged();
	afx_msg void OnFileListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnFeatureListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()

private:
	// --- UI 구성 ---
	void SetupControls();          // 동적 컨트롤 생성 + 리스트 컬럼 셋업
	void PopulateProfileCombo();   // profiles\*.ini 탐색
	void SetStatus(const CString& text);
	CString CurrentProfilePath();

	// --- 데이터 흐름 ---
	void LoadFolder(const CString& dir);
	void LoadImageAt(int index);   // m_files[index] 로드
	void AnalyzeCurrent();         // threshold + RegionExtractor + FeatureCalculator
	void UpdateFeatureList();       // 우측 특징값 패널 갱신
	void LoadProfileSelection();    // 콤보 선택 프로파일 로드

	// --- 렌더링 ---
	void DrawImageView(CDC* pDC);   // 더블버퍼링 이미지 뷰
	cv::Mat BuildOverlayMat();      // 표시용 BGR(오버레이 포함) 생성
	static bool DrawMatToDC(CDC* pDC, const cv::Mat& bgr, const CRect& dest); // Mat→DIB StretchDIBits

	// --- 상태 ---
	CListCtrl m_listFiles;
	CListCtrl m_listFeatures;
	CComboBox m_comboZoom;
	CComboBox m_comboProfile;

	std::vector<std::string> m_files;   // 폴더 내 이미지 전체 경로
	int m_curIndex;                     // 현재 선택 인덱스(-1 없음)

	cv::Mat m_binImage;                 // 현재 이미지(0/255)
	std::vector<Grf::Region> m_regions;
	std::vector<Grf::FeatureVector> m_features;

	Grf::ProfileLoader m_profile;
	bool m_profileLoaded;
	std::vector<std::string> m_profilePaths; // 콤보 인덱스 → ini 경로

	int m_zoom;              // 정수 배율(기본 4)
	bool m_showOverlay;      // 오버레이 on/off
	int m_highlightRegion;   // 강조할 Region 인덱스(-1 없음)

	CRect m_imgFrameRect;    // 이미지 그리는 클라이언트 영역
	HICON m_hIcon;
};
