#pragma once
// GrfViewSupport.h
// 뷰어 탭 대시보드 공용 지원 모듈.
//  - RegionResult : 분석 결과 1건(= Region 1개) 도메인 뷰 모델
//  - DefectCodeColor : 분류코드 → 고정색(뱃지/차트 공용)
//  - ThumbCache : 파일 그레이 원본 LRU 캐시(카드 스크롤 시 가시 영역만 디코드)
//  - AnalyzeFiles : 폴더 파일들을 병렬 분석 → RegionResult 평탄화 목록
// 좌표계: bbox/contours 는 OpenCV (x=col, y=row) 이미지 좌표.

#include <string>
#include <vector>
#include <list>
#include <map>
#include <opencv2/core.hpp>
// (map 은 per-feature score 보관에 사용)

#include <windows.h>
#include "GlimRegionFeatures.h"

namespace GrfView {

// 분석 결과 1건 = Region 1개
struct RegionResult {
	int fileIndex;        // 원본 파일 목록 인덱스
	int regionIndex;      // 파일 내 Region 인덱스(0-based)
	std::string filePath; // 이미지 절대경로
	std::string fileName; // 파일명만
	char channel;         // 'W'(밝음) / 'B'(어두움)

	Grf::FeatureVector fv;
	double overallScore;  // 0~100, 프로파일 없으면 -1
	std::map<std::string, double> scores; // featureName → 0~100 (프로파일 있을 때, 묶음막대용)
	std::string code;     // 분류 코드(프로파일 없으면 "")

	cv::Rect bbox;        // 이미지 좌표계 bbox
	std::vector<std::vector<cv::Point> > contours; // 오버레이 썸네일용(이미지 좌표)

	RegionResult()
		: fileIndex(-1), regionIndex(-1), channel('W')
		, overallScore(-1.0)
	{
	}
};

// 분류코드 → 고정 COLORREF. 알 수 없는 코드는 문자열 해시로 안정적 색 생성.
COLORREF DefectCodeColor(const std::string& code);

// 이진화 채널 태그('B' 흑/어두운 불량, 'W' 백/밝은 불량) → 뱃지 색.
//  PROJECTION 등 흑/백 2채널 결과의 시각 구분에 카드/상세 뷰가 공용으로 쓴다.
COLORREF ChannelColor(char channel);

// ────────────────────────────────────────────────────────────────
// 파일 그레이 원본 LRU 캐시. 카드/미리보기가 썸네일 렌더 시 조회.
//  128x128 크롭 기준. 수백 장 폴더에서도 상주 메모리를 capacity 로 제한.
//  UI 스레드 전용이지만 방어적으로 CriticalSection 보호.
// ────────────────────────────────────────────────────────────────
class ThumbCache {
public:
	explicit ThumbCache(size_t capacity = 64);
	~ThumbCache();

	// 경로의 8UC1 그레이 원본 반환(미보유 시 디코드 후 캐시). 실패 시 empty.
	cv::Mat GetGray(const std::string& path);
	void Clear();
	void SetCapacity(size_t capacity);

private:
	struct Entry {
		std::string path;
		cv::Mat gray;
	};

	std::list<Entry> m_lru;                                       // front = 최근 사용
	std::map<std::string, std::list<Entry>::iterator> m_index;   // path → 리스트 노드
	size_t m_capacity;
	CRITICAL_SECTION m_cs;

	// 복사 금지(CS 소유)
	ThumbCache(const ThumbCache&);
	ThumbCache& operator=(const ThumbCache&);
};

// ────────────────────────────────────────────────────────────────
// 폴더 파일들을 병렬 분석. preprocessor/profile 은 로드 후 읽기전용(스레드 안전).
//  결과는 파일 정렬 순서를 유지한 평탄화 목록으로 out 에 채운다.
//  numThreads <= 0 이면 hardware_concurrency. minArea 미만 성분은 버린다.
//  progress != NULL 이면 처리 완료 파일 수를 원자적으로 증가(취소는 미지원).
// ────────────────────────────────────────────────────────────────
void AnalyzeFiles(const std::vector<std::string>& files,
	const Grf::IPreprocessor& pre,
	const Grf::ProfileLoader* profile,
	int numThreads,
	int minArea,
	std::vector<RegionResult>& out,
	volatile long* progress = NULL);

// ────────────────────────────────────────────────────────────────
// 공용 렌더 유틸(호출부는 stdafx.h 로 MFC 를 먼저 include 한 상태여야 함)
// ────────────────────────────────────────────────────────────────
// cv::Mat(8UC1 또는 8UC3, BGR) 를 dest 영역에 종횡비 유지로 중앙 정렬 렌더.
//  nearest=true 면 COLORONCOLOR(이진 픽셀 경계 유지), false 면 HALFTONE.
//  반환: 실제로 그려진 사각형. 실패/빈 이미지면 빈 CRect.
CRect DrawMatFit(CDC* pDC, const cv::Mat& img, const CRect& dest, bool nearest);

} // namespace GrfView
