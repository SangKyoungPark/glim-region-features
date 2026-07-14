#pragma once
// ImageViewCtrl.h
// 결과 탭 상세 이미지 뷰(자식 컨트롤). BGR Mat 을 정수 배율 확대(fit 상한)로 표시.
//  탭 컨트롤이 클라이언트를 덮으므로 상세 이미지는 자식 윈도우로 그려야 한다
//  (다이얼로그 OnPaint 직접 그리기는 탭 본문에 가려짐).
// 더블버퍼 + 최근접 확대(이진 픽셀 경계 유지).

#include <opencv2/core.hpp>

class CImageViewCtrl : public CWnd
{
public:
	CImageViewCtrl();
	virtual ~CImageViewCtrl();

	BOOL CreateCtrl(CWnd* pParent, const CRect& rc, UINT id);

	void SetImage(const cv::Mat& bgr); // 8UC3 BGR(오버레이 포함) 또는 empty
	void SetZoom(int zoom);            // 정수 배율(<=0 = fit)
	void SetChannelBadge(char channel, bool show); // 선택 Region 의 흑/백 채널 뱃지(좌상단)
	void Clear();

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	DECLARE_MESSAGE_MAP()

private:
	cv::Mat m_img; // BGR
	int m_zoom;
	char m_badgeChannel; // 'B'/'W'
	bool m_showBadge;
};
