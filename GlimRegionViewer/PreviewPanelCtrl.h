#pragma once
// PreviewPanelCtrl.h
// 설정 탭 미리보기 패널. 이미지 슬롯을 최대 3개까지 가로로 나란히 표시한다.
//  현재: [원본] | [이진화 결과] 2슬롯 사용.
//  향후: PROJECTION(흑|백 2장) 대비 3슬롯까지 확장 가능한 구조.
// 더블버퍼링으로 깜빡임 방지. 이미지는 종횡비 유지 fit.

#include <vector>
#include <opencv2/core.hpp>

class CPreviewPanelCtrl : public CWnd
{
public:
	CPreviewPanelCtrl();
	virtual ~CPreviewPanelCtrl();

	BOOL CreatePanel(CWnd* pParent, const CRect& rc, UINT id);

	// 슬롯 이미지/라벨 설정(최대 3). 이미지는 8UC1/8UC3 모두 허용.
	void SetSlots(const std::vector<cv::Mat>& images, const std::vector<CString>& labels);
	void ClearSlots();

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	DECLARE_MESSAGE_MAP()

private:
	void DrawContent(CDC* pDC, const CRect& client);

	std::vector<cv::Mat> m_images;
	std::vector<CString> m_labels;

	enum { kMaxSlots = 3 };
};
