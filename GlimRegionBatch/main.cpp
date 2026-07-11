// main.cpp - GlimRegionBatch
// 폴더 단위 배치 검사 콘솔.
//  사용법: GlimRegionBatch.exe <입력폴더> <출력.csv> [프로파일.ini]
//   - 입력폴더의 *.bmp/*.png/*.jpg/*.jpeg/*.tif 를 전부 로드(IMREAD_GRAYSCALE)
//   - threshold(127,255,BINARY) 로 0/255 이진화 보장
//   - 이미지별 Region 추출 → Region 마다 전체 특징값 계산
//   - 프로파일 INI 주어지면 항목별 Score + select_shape 분류코드도 계산
//   - CSV: 1행 = Region 1개. Region 없는 이미지도 RegionIndex=-1 로 1행 기록.
//  128x128 전제지만 크기 하드코딩 없음(임의 크기 동작). 파일 단위 try-catch 로 배치 지속.

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <filesystem>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include "GlimRegionFeatures.h"

using namespace Grf;
namespace fs = std::filesystem;

namespace {

// 소문자 변환
std::string ToLower(const std::string& s)
{
	std::string out(s);
	std::transform(out.begin(), out.end(), out.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return out;
}

// 지원 확장자 판정
bool IsSupportedImage(const std::string& ext)
{
	std::string e = ToLower(ext);
	return (e == ".bmp" || e == ".png" || e == ".jpg" ||
		e == ".jpeg" || e == ".tif" || e == ".tiff");
}

// CSV 필드 escape (콤마/따옴표 대비): 항상 따옴표로 감싸고 내부 따옴표는 2배.
std::string CsvQuote(const std::string& s)
{
	std::string out = "\"";
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] == '"')
			out += "\"\"";
		else
			out += s[i];
	}
	out += "\"";
	return out;
}

// FeatureVector::CsvHeader() 의 컬럼 개수(콤마+1)
int FeatureColumnCount()
{
	std::string h = FeatureVector::CsvHeader();
	int cols = 1;
	for (size_t i = 0; i < h.size(); ++i)
		if (h[i] == ',') ++cols;
	return cols;
}

} // namespace

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

	// --- 프로파일(옵션) 로드 ---
	ProfileLoader profile;
	bool useProfile = false;
	std::vector<std::string> scoreFeatureNames; // Score 컬럼 순서 고정용
	if (!profilePath.empty())
	{
		if (profile.Load(profilePath))
		{
			useProfile = true;
			const std::vector<ScoreConfig>& cfgs = profile.Normalizer().Configs();
			for (size_t i = 0; i < cfgs.size(); ++i)
				scoreFeatureNames.push_back(cfgs[i].m_featureName);
			std::cout << "profile loaded: " << profile.ProfileName()
				<< " (scoreCfg=" << cfgs.size()
				<< ", rules=" << profile.RuleEngine().RuleCount() << ")" << std::endl;
		}
		else
		{
			std::cout << "WARN: profile load failed: " << profilePath
				<< " (계속 진행, Score/분류 컬럼 생략)" << std::endl;
		}
	}

	// --- 입력 폴더 파일 수집 ---
	std::vector<std::string> files;
	try
	{
		if (!fs::exists(inputDir) || !fs::is_directory(inputDir))
		{
			std::cout << "ERROR: 입력 폴더가 없거나 폴더가 아님: " << inputDir << std::endl;
			return 2;
		}
		for (fs::directory_iterator it(inputDir); it != fs::directory_iterator(); ++it)
		{
			if (!it->is_regular_file())
				continue;
			const std::string ext = it->path().extension().string();
			if (IsSupportedImage(ext))
				files.push_back(it->path().string());
		}
	}
	catch (const std::exception& e)
	{
		std::cout << "ERROR: 폴더 스캔 실패: " << e.what() << std::endl;
		return 2;
	}
	std::sort(files.begin(), files.end());

	if (files.empty())
	{
		std::cout << "지원 이미지가 없습니다: " << inputDir << std::endl;
		return 0;
	}
	std::cout << "found " << files.size() << " image(s)." << std::endl;

	// --- CSV 열기 ---
	std::ofstream ofs(outputCsv.c_str(), std::ios::binary);
	if (!ofs.is_open())
	{
		std::cout << "ERROR: CSV 파일을 열 수 없음: " << outputCsv << std::endl;
		return 2;
	}

	// --- 헤더 (한 곳: FeatureVector::CsvHeader() 재사용) ---
	std::string header = "FileName,RegionIndex," + FeatureVector::CsvHeader();
	if (useProfile)
	{
		for (size_t i = 0; i < scoreFeatureNames.size(); ++i)
			header += ",score_" + scoreFeatureNames[i];
		header += ",ClassifiedCode";
	}
	ofs << header << "\r\n";

	// Region 없는 이미지의 빈 특징 컬럼(콤마 개수 = 특징컬럼수-1)
	const int featCols = FeatureColumnCount();
	const std::string emptyFeat(featCols > 0 ? (featCols - 1) : 0, ',');

	// --- 배치 처리 ---
	RegionExtractor extractor;
	FeatureCalculator calc;

	size_t processed = 0;
	long long totalRegions = 0;
	std::vector<std::string> failedFiles;

	for (size_t f = 0; f < files.size(); ++f)
	{
		const std::string& path = files[f];
		const std::string fileName = fs::path(path).filename().string();

		try
		{
			cv::Mat img = cv::imread(path, cv::IMREAD_GRAYSCALE);
			if (img.empty())
			{
				failedFiles.push_back(fileName + " (load 실패)");
				continue;
			}

			// 완전 이진이 아닐 수 있으므로 0/255 로 정규화
			cv::Mat bin;
			cv::threshold(img, bin, 127.0, 255.0, cv::THRESH_BINARY);

			std::vector<Region> regions = extractor.Extract(bin, 1);

			if (regions.empty())
			{
				// 누락 추적용: RegionIndex=-1 로 1행 기록
				ofs << CsvQuote(fileName) << ",-1," << emptyFeat;
				if (useProfile)
				{
					for (size_t s = 0; s < scoreFeatureNames.size(); ++s)
						ofs << ",";
					ofs << ",NO_REGION";
				}
				ofs << "\r\n";
				++processed;
				continue;
			}

			for (size_t r = 0; r < regions.size(); ++r)
			{
				FeatureVector fv = calc.Compute(regions[r]);

				ofs << CsvQuote(fileName) << "," << static_cast<int>(r) << ","
					<< fv.ToCsvRow();

				if (useProfile)
				{
					ScoreResult sr = profile.Normalizer().Normalize(fv);
					for (size_t s = 0; s < scoreFeatureNames.size(); ++s)
						ofs << "," << sr.Get(scoreFeatureNames[s]);
					std::string code = profile.RuleEngine().Classify(fv, "OK");
					ofs << "," << CsvQuote(code);
				}
				ofs << "\r\n";
				++totalRegions;
			}
			++processed;
		}
		catch (const cv::Exception& e)
		{
			failedFiles.push_back(fileName + " (cv: " + e.what() + ")");
		}
		catch (const std::exception& e)
		{
			failedFiles.push_back(fileName + " (std: " + e.what() + ")");
		}
		catch (...)
		{
			failedFiles.push_back(fileName + " (unknown)");
		}
	}

	ofs.flush();
	ofs.close();

	// --- 콘솔 요약 ---
	std::cout << "\n==================================================" << std::endl;
	std::cout << "SUMMARY" << std::endl;
	std::cout << "  processed files : " << processed << " / " << files.size() << std::endl;
	std::cout << "  total regions   : " << totalRegions << std::endl;
	std::cout << "  output csv      : " << outputCsv << std::endl;
	std::cout << "  failed files    : " << failedFiles.size() << std::endl;
	for (size_t i = 0; i < failedFiles.size(); ++i)
		std::cout << "    - " << failedFiles[i] << std::endl;
	std::cout << "==================================================" << std::endl;

	return failedFiles.empty() ? 0 : 3;
}
