#pragma once
// IPreprocessor.h
// 전처리(원본 이미지 → 라벨링 입력용 0/255 이진 이미지) 인터페이스.
//
// [GPU 삽입 지점]
//  이 PC 는 NVIDIA GPU 미탑재 + OpenCV 프리빌트가 CUDA 미포함이라 CUDA 코드는 넣지 않는다.
//  다만 GPU 가 있는 PC 에서 "대형 원본"을 다룰 때는 전처리(그레이 변환/threshold/모폴로지 등)를
//  GPU 로 수행하는 것이 유리하다. 그 경우 이 인터페이스를 구현한 CUDA 전처리기(cv::cuda 빌드 또는
//  자체 커널)를 만들어 RegionExtractor::ExtractFromGray / CsvExporter::ExportFolder 에 주입하면
//  나머지 파이프라인(라벨링/특징값/Score/CSV)은 무수정으로 재사용된다.
//  주의: 128x128 크롭 단위 계산은 GPU 전송 오버헤드가 이득을 상쇄하므로 GPU 대상이 아니다.

#include <opencv2/core.hpp>

namespace Grf {

class IPreprocessor {
public:
	virtual ~IPreprocessor() {}

	// 원본(그레이 또는 컬러) → 8UC1 0/255 이진 이미지.
	// 구현은 반드시 무상태/재진입 안전이어야 한다(여러 쓰레드가 동시 호출 가능).
	// 입력은 const 참조로만 읽고, 출력은 새 Mat 로 반환한다.
	virtual cv::Mat Binarize(const cv::Mat& src) const = 0;
};

// 이진화 모드
enum BinarizeMode {
	BINMODE_FIXED = 0,      // 고정 임계값(m_threshold)
	BINMODE_OTSU = 1,       // Otsu 자동 임계값
	BINMODE_MEAN_OFFSET = 2 // 배경 평균 기반: thresh = mean ± offset
};

// 극성: Region 으로 잡을 밝기 방향
enum BinarizePolarity {
	POLARITY_BRIGHT = 0, // 밝은 쪽 = Region (THRESH_BINARY, 기존 동작)
	POLARITY_DARK = 1    // 어두운 쪽 = Region (THRESH_BINARY_INV, 흑점 등)
};

struct BinarizeParams {
	BinarizeMode m_mode;
	BinarizePolarity m_polarity;
	double m_threshold; // FIXED 용
	double m_offset;    // MEAN_OFFSET 용 (DARK: mean-offset 미만이 Region, BRIGHT: mean+offset 초과가 Region)

	// 기본값 = 기존 동작(FIXED 127, BRIGHT) 100% 동일
	BinarizeParams()
		: m_mode(BINMODE_FIXED), m_polarity(POLARITY_BRIGHT)
		, m_threshold(127.0), m_offset(20.0) {}
};

// 기본 CPU 구현: (필요 시 그레이 변환) → 모드/극성에 따른 threshold.
class CpuPreprocessor : public IPreprocessor {
public:
	CpuPreprocessor() : m_params() {}
	explicit CpuPreprocessor(const BinarizeParams& params) : m_params(params) {}
	// 레거시 호환: 고정 임계값(FIXED/BRIGHT)
	explicit CpuPreprocessor(double thresholdValue) : m_params()
	{
		m_params.m_threshold = thresholdValue;
	}

	virtual cv::Mat Binarize(const cv::Mat& src) const;

	const BinarizeParams& Params() const { return m_params; }
	double Threshold() const { return m_params.m_threshold; }

private:
	BinarizeParams m_params; // 로드 후 불변(설정값). 계산 중 변경 금지.
};

} // namespace Grf
