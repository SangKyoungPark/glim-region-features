// CsvExporter.cpp
#include "Adapter/CsvExporter.h"
#include "Adapter/RegionExtractor.h"
#include "UseCase/FeatureCalculator.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

namespace fs = std::filesystem;

namespace Grf {

namespace {
	std::string ToLower(const std::string& s)
	{
		std::string out(s);
		std::transform(out.begin(), out.end(), out.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return out;
	}
}

bool CsvExporter::IsSupportedImage(const std::string& ext)
{
	std::string e = ToLower(ext);
	return (e == ".bmp" || e == ".png" || e == ".jpg" ||
		e == ".jpeg" || e == ".tif" || e == ".tiff");
}

std::string CsvExporter::CsvQuote(const std::string& s)
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

int CsvExporter::FeatureColumnCount()
{
	std::string h = FeatureVector::CsvHeader();
	int cols = 1;
	for (size_t i = 0; i < h.size(); ++i)
		if (h[i] == ',') ++cols;
	return cols;
}

std::vector<std::string> CsvExporter::ScoreFeatureNames(const ProfileLoader* profile)
{
	std::vector<std::string> names;
	if (profile == NULL || !profile->IsLoaded())
		return names;
	const std::vector<ScoreConfig>& cfgs = profile->Normalizer().Configs();
	for (size_t i = 0; i < cfgs.size(); ++i)
		names.push_back(cfgs[i].m_featureName);
	return names;
}

std::string CsvExporter::BuildHeader(const ProfileLoader* profile)
{
	std::string header = "FileName,RegionIndex," + FeatureVector::CsvHeader();
	if (profile != NULL && profile->IsLoaded())
	{
		std::vector<std::string> names = ScoreFeatureNames(profile);
		for (size_t i = 0; i < names.size(); ++i)
			header += ",score_" + names[i];
		header += ",ClassifiedCode";
	}
	return header;
}

std::string CsvExporter::BuildRegionRow(const std::string& fileName, int regionIndex,
	const FeatureVector& fv, const ProfileLoader* profile)
{
	std::ostringstream oss;
	oss << CsvQuote(fileName) << "," << regionIndex << "," << fv.ToCsvRow();

	if (profile != NULL && profile->IsLoaded())
	{
		std::vector<std::string> names = ScoreFeatureNames(profile);
		ScoreResult sr = profile->Normalizer().Normalize(fv);
		for (size_t i = 0; i < names.size(); ++i)
			oss << "," << sr.Get(names[i]);
		std::string code = profile->RuleEngine().Classify(fv, "OK");
		oss << "," << CsvQuote(code);
	}
	return oss.str();
}

std::string CsvExporter::BuildEmptyRow(const std::string& fileName, const ProfileLoader* profile)
{
	const int featCols = FeatureColumnCount();
	const std::string emptyFeat(featCols > 0 ? (featCols - 1) : 0, ',');

	std::ostringstream oss;
	oss << CsvQuote(fileName) << ",-1," << emptyFeat;

	if (profile != NULL && profile->IsLoaded())
	{
		std::vector<std::string> names = ScoreFeatureNames(profile);
		for (size_t i = 0; i < names.size(); ++i)
			oss << ",";
		oss << ",NO_REGION";
	}
	return oss.str();
}

bool CsvExporter::ExportFolder(const std::string& inputDir, const std::string& outputCsv,
	const ProfileLoader* profile, BatchStat& statOut)
{
	statOut = BatchStat();

	// --- 파일 수집 ---
	std::vector<std::string> files;
	try
	{
		if (!fs::exists(inputDir) || !fs::is_directory(inputDir))
			return false;
		for (fs::directory_iterator it(inputDir); it != fs::directory_iterator(); ++it)
		{
			if (!it->is_regular_file())
				continue;
			const std::string ext = it->path().extension().string();
			if (IsSupportedImage(ext))
				files.push_back(it->path().string());
		}
	}
	catch (const std::exception&)
	{
		return false;
	}
	std::sort(files.begin(), files.end());
	statOut.m_totalFiles = files.size();

	// --- CSV 열기 ---
	std::ofstream ofs(outputCsv.c_str(), std::ios::binary);
	if (!ofs.is_open())
		return false;

	ofs << BuildHeader(profile) << "\r\n";

	RegionExtractor extractor;
	FeatureCalculator calc;

	for (size_t f = 0; f < files.size(); ++f)
	{
		const std::string& path = files[f];
		std::string fileName = path;
		try { fileName = fs::path(path).filename().string(); }
		catch (...) {}

		try
		{
			cv::Mat img = cv::imread(path, cv::IMREAD_GRAYSCALE);
			if (img.empty())
			{
				statOut.m_failedFiles.push_back(fileName + " (load failed)");
				continue;
			}

			cv::Mat bin;
			cv::threshold(img, bin, 127.0, 255.0, cv::THRESH_BINARY);

			std::vector<Region> regions = extractor.Extract(bin, 1);

			if (regions.empty())
			{
				ofs << BuildEmptyRow(fileName, profile) << "\r\n";
				++statOut.m_processedFiles;
				continue;
			}

			for (size_t r = 0; r < regions.size(); ++r)
			{
				FeatureVector fv = calc.Compute(regions[r]);
				ofs << BuildRegionRow(fileName, static_cast<int>(r), fv, profile) << "\r\n";
				++statOut.m_totalRegions;
			}
			++statOut.m_processedFiles;
		}
		catch (const cv::Exception& e)
		{
			statOut.m_failedFiles.push_back(fileName + " (cv: " + e.what() + ")");
		}
		catch (const std::exception& e)
		{
			statOut.m_failedFiles.push_back(fileName + " (std: " + e.what() + ")");
		}
		catch (...)
		{
			statOut.m_failedFiles.push_back(fileName + " (unknown)");
		}
	}

	ofs.flush();
	ofs.close();
	return true;
}

} // namespace Grf
