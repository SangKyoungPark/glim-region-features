// main.cpp - GlimRegionBatch
// 폴더 단위 배치 검사 콘솔.
//  사용법: GlimRegionBatch.exe <입력폴더> <출력.csv> [프로파일.ini]
//  실제 배치/CSV 생성 로직은 라이브러리의 Grf::CsvExporter 로 공용화되어 있다.
//  (Viewer 의 CSV 내보내기와 동일 포맷 보장)

#include <iostream>
#include <string>

#include "GlimRegionFeatures.h"

using namespace Grf;

int main(int argc, char** argv)
{
	std::cout << "GlimRegionBatch - folder batch inspection" << std::endl;

	if (argc < 3)
	{
		std::cout << "usage: GlimRegionBatch.exe <input_folder> <output.csv> [profile.ini]" << std::endl;
		return 1;
	}

	const std::string inputDir = argv[1];
	const std::string outputCsv = argv[2];
	const std::string profilePath = (argc >= 4) ? argv[3] : std::string();

	// 프로파일(옵션)
	ProfileLoader profile;
	const ProfileLoader* profilePtr = NULL;
	if (!profilePath.empty())
	{
		if (profile.Load(profilePath))
		{
			profilePtr = &profile;
			std::cout << "profile loaded: " << profile.ProfileName()
				<< " (scoreCfg=" << profile.Normalizer().ConfigCount()
				<< ", rules=" << profile.RuleEngine().RuleCount() << ")" << std::endl;
		}
		else
		{
			std::cout << "WARN: profile load failed: " << profilePath
				<< " (계속 진행, Score/분류 컬럼 생략)" << std::endl;
		}
	}

	BatchStat stat;
	bool ok = false;
	try
	{
		ok = CsvExporter::ExportFolder(inputDir, outputCsv, profilePtr, stat);
	}
	catch (const std::exception& e)
	{
		std::cout << "ERROR: 배치 처리 중 예외: " << e.what() << std::endl;
		return 2;
	}

	if (!ok)
	{
		std::cout << "ERROR: 입력 폴더/출력 CSV 경로를 확인하세요. (input=" << inputDir
			<< ", output=" << outputCsv << ")" << std::endl;
		return 2;
	}

	std::cout << "\n==================================================" << std::endl;
	std::cout << "SUMMARY" << std::endl;
	std::cout << "  processed files : " << stat.m_processedFiles << " / " << stat.m_totalFiles << std::endl;
	std::cout << "  total regions   : " << stat.m_totalRegions << std::endl;
	std::cout << "  output csv      : " << outputCsv << std::endl;
	std::cout << "  failed files    : " << stat.m_failedFiles.size() << std::endl;
	for (size_t i = 0; i < stat.m_failedFiles.size(); ++i)
		std::cout << "    - " << stat.m_failedFiles[i] << std::endl;
	std::cout << "==================================================" << std::endl;

	return stat.m_failedFiles.empty() ? 0 : 3;
}
