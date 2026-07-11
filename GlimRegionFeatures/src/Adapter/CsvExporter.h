#pragma once
// CsvExporter.h
// 폴더/이미지 배치 검사 결과를 CSV 로 내보내는 공용 어댑터(Batch/Viewer 공유).
// CSV 컬럼 구성은 이 한 곳에서 관리하여 헤더/행 순서 정합을 보장한다.
//  컬럼: FileName, RegionIndex, FeatureVector::CsvHeader()
//        [프로파일 사용 시] score_<feature>..., ClassifiedCode
//  Region 없는 이미지도 RegionIndex=-1 로 1행 기록(누락 추적).

#include <string>
#include <vector>
#include "Domain/FeatureVector.h"
#include "Domain/ScoreResult.h"
#include "Profile/ProfileLoader.h"
#include "Adapter/IPreprocessor.h"

namespace Grf {

// 배치 처리 통계
struct BatchStat {
	size_t m_totalFiles;
	size_t m_processedFiles;
	long long m_totalRegions;
	long long m_elapsedMs;   // 총 소요시간(ms)
	std::vector<std::string> m_failedFiles;

	BatchStat()
		: m_totalFiles(0), m_processedFiles(0), m_totalRegions(0), m_elapsedMs(0) {}
};

class CsvExporter {
public:
	// 지원 확장자 판정(.bmp/.png/.jpg/.jpeg/.tif/.tiff, 대소문자 무시)
	static bool IsSupportedImage(const std::string& ext);

	// CSV 헤더 라인(개행 없음). profile 이 null 이면 특징값 컬럼까지만.
	static std::string BuildHeader(const ProfileLoader* profile);

	// Region 1개 → CSV 행(개행 없음).
	static std::string BuildRegionRow(const std::string& fileName, int regionIndex,
		const FeatureVector& fv, const ProfileLoader* profile);

	// Region 없는 이미지 행(RegionIndex=-1).
	static std::string BuildEmptyRow(const std::string& fileName, const ProfileLoader* profile);

	// 폴더 전체를 CSV 파일로 저장. profile 이 null 이면 특징값만.
	// preprocessor 가 null 이면 기본 CpuPreprocessor(threshold 127) 사용.
	// CSV 행 순서는 파일 정렬 순서를 그대로 유지한다.
	// 성공(파일 오픈)하면 true, 통계는 statOut 에 채운다.
	static bool ExportFolder(const std::string& inputDir, const std::string& outputCsv,
		const ProfileLoader* profile, BatchStat& statOut,
		const IPreprocessor* preprocessor = NULL);

	// 프로파일의 Score 특징값 이름 순서(컬럼 고정용).
	static std::vector<std::string> ScoreFeatureNames(const ProfileLoader* profile);

private:
	static int FeatureColumnCount();
	static std::string CsvQuote(const std::string& s);
};

} // namespace Grf
