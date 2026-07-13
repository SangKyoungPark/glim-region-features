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
#include <vector>

namespace Grf {

// 이진화 채널 하나(값 + 태그). PROJECTION 은 흑/백 2채널, 그 외 모드는 1채널.
struct BinChannel {
	cv::Mat image; // 8UC1 0/255
	char tag;      // 'B'(흑/어두운 불량) 또는 'W'(백/밝은 불량)
	BinChannel() : tag('W') {}
	BinChannel(const cv::Mat& img, char t) : image(img), tag(t) {}
};

class IPreprocessor {
public:
	virtual ~IPreprocessor() {}

	// 원본(그레이 또는 컬러) → 8UC1 0/255 이진 이미지.
	// 구현은 반드시 무상태/재진입 안전이어야 한다(여러 쓰레드가 동시 호출 가능).
	// 입력은 const 참조로만 읽고, 출력은 새 Mat 로 반환한다.
	virtual cv::Mat Binarize(const cv::Mat& src) const = 0;

	// 채널 단위 이진화. 기본 구현은 Binarize() 결과 1장을 'W' 태그로 반환한다.
	//  PROJECTION 처럼 흑/백 2장이 필요한 모드는 이 메서드를 오버라이드한다.
	//  (CsvExporter/미리보기/덤프가 이 메서드로 채널을 순회한다)
	virtual std::vector<BinChannel> BinarizeMulti(const cv::Mat& src) const
	{
		std::vector<BinChannel> out;
		cv::Mat b = Binarize(src);
		if (!b.empty())
			out.push_back(BinChannel(b, 'W'));
		return out;
	}
};

// 이진화 모드
enum BinarizeMode {
	BINMODE_FIXED = 0,       // 고정 임계값(m_threshold)
	BINMODE_OTSU = 1,        // Otsu 자동 임계값
	BINMODE_MEAN_OFFSET = 2, // 배경 평균 기반: thresh = mean ± offset
	BINMODE_BINARY = 3,      // 이미 이진화됨: threshold 0 (0 초과 = Region), 극성만 반영
	BINMODE_WRINKLE = 4,     // 주름(밝은 무지부 위 미세 어두운 선): 밝은영역 마스크 ∩ BLACKHAT 응답
	BINMODE_PROJECTION = 5   // 검사기 방식: 열평균 프로파일 대비 국소평균 차 → 흑/백 2채널
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

	// WRINKLE 용 (다른 모드에서는 무시). 실측 검증 파이프라인(60/60):
	int m_kernelSize;        // BLACKHAT 수평 RECT 커널 폭(가로 N x 세로 1). 기본 15
	int m_blurH;             // 세로 누적 blur 길이(가로 1 x 세로 N). SNR 확보 핵심. 기본 31
	double m_responseThresh; // 누적 응답 이진화 임계값. 기본 4

	// PROJECTION 용 (검사기 방식, 다른 모드에서는 무시)
	//  주의: 크롭 내 열평균 프로파일은 "프레임 전체 프로파일"의 근사다. 검사기는 프레임 폭 전체로
	//  열평균을 구하지만 여기서는 128 크롭 폭만으로 구하므로, 무지/코팅 경계가 프로파일에 섞이면
	//  차이가 달라질 수 있다(단일 크롭 단독 처리의 한계).
	double m_projBlackTh;    // 흑 임계: proj[x] - 국소평균 > blackTh (기본 25)
	double m_projWhiteTh;    // 백 임계: 국소평균 - proj[x] > whiteTh (기본 25)
	int m_projKernel;        // 국소평균 boxFilter 커널 크기(검사기 4x4 근사). 기본 4

	// 기본값 = 기존 동작(FIXED 127, BRIGHT) 100% 동일
	BinarizeParams()
		: m_mode(BINMODE_FIXED), m_polarity(POLARITY_BRIGHT)
		, m_threshold(127.0), m_offset(20.0)
		, m_kernelSize(15), m_blurH(31), m_responseThresh(4.0)
		, m_projBlackTh(25.0), m_projWhiteTh(25.0), m_projKernel(4) {}
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

	// PROJECTION 은 흑/백 2채널, 그 외 모드는 극성 태그 1채널을 반환한다.
	virtual std::vector<BinChannel> BinarizeMulti(const cv::Mat& src) const;

	const BinarizeParams& Params() const { return m_params; }
	double Threshold() const { return m_params.m_threshold; }

private:
	BinarizeParams m_params; // 로드 후 불변(설정값). 계산 중 변경 금지.
};

} // namespace Grf
