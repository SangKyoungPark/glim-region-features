// main.cpp - GlimRegionBatch
// 폴더 단위 배치 검사 콘솔(파일 단위 멀티쓰레드).
//  사용법: GlimRegionBatch.exe <입력폴더> <출력.csv> [프로파일.ini] [--threads N]
//   --threads N : 워커 쓰레드 수(기본 0=자동=hardware_concurrency). 0 도 자동.
//  실제 배치/CSV 생성 로직은 라이브러리의 Grf::CsvExporter 로 공용화되어 있다.
//  (Viewer 의 CSV 내보내기와 동일 포맷 보장, 쓰레드 수와 무관하게 결과 바이트 동일)
//
//  다차원 feature 공간 Blob 군집화(신규, docs/CLUSTERING.md 참조):
//   --cluster <result.csv> <clusters.json>              : 기존 result.csv 로 군집화
//   --cluster-folder <입력폴더> <clusters.json>          : 이진화 안 된 원본 이미지 폴더에서
//                                                          "이진화->feature 추출->군집화" 원샷 실행
//   --to-profile <clusters.json> <result.csv> <out.ini>  : 군집 결과 + 사용자 라벨 -> 프로파일 INI 초안

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>

#include <opencv2/imgcodecs.hpp>   // --preview: imread/imwrite

#include "GlimRegionFeatures.h"

using namespace Grf;

namespace {

// "a,b,c" -> ["a","b","c"] (trim 적용, 빈 항목 스킵)
std::vector<std::string> SplitCommaTrim(const std::string& csv)
{
	std::vector<std::string> out;
	std::stringstream ss(csv);
	std::string item;
	while (std::getline(ss, item, ','))
	{
		size_t b = item.find_first_not_of(" \t");
		size_t e = item.find_last_not_of(" \t");
		if (b != std::string::npos)
			out.push_back(item.substr(b, e - b + 1));
	}
	return out;
}

int ScaleModeFromString(const std::string& s)
{
	if (s == "minmax") return SCALE_MINMAX;
	if (s == "robust") return SCALE_ROBUST;
	return SCALE_ZSCORE;
}

// FeatureMatrix -> (자동/지정 K) -> ClusterResult + (있으면) KSelector::Curve.
//  --cluster / --cluster-folder 가 공유하는 실행 단계(설계: "군집 계산 단일 소스").
bool RunClusterOnMatrix(const FeatureMatrix& matrix, const std::string& kStr, int kMin, int kMax,
	int scaleMode, bool logArea, int seed, int attempts,
	ClusterParams& paramsOut, ClusterResult& resultOut, KSelector::Curve& curveOut, bool& hasCurveOut)
{
	paramsOut = ClusterParams();
	paramsOut.m_features = matrix.Columns();
	paramsOut.m_scaleMode = scaleMode;
	paramsOut.m_logAreaLike = logArea;
	paramsOut.m_kMax = kMax;
	paramsOut.m_seed = seed;
	paramsOut.m_attempts = attempts;

	hasCurveOut = false;
	int kFinal = 0;

	if (kStr == "auto" || kStr.empty())
	{
		KSelector selector;
		curveOut = selector.Sweep(matrix, paramsOut, kMin, kMax);
		if (!curveOut.m_ok)
		{
			std::cout << "ERROR: K 자동 결정 실패: " << curveOut.m_error << std::endl;
			return false;
		}
		kFinal = curveOut.m_recommendedK;
		hasCurveOut = true;
		std::cout << "auto K sweep: recommended K=" << kFinal << std::endl;
	}
	else
	{
		kFinal = std::atoi(kStr.c_str());
		// 사용자가 K 를 지정해도 곡선은 참고용으로 함께 계산(웹 표시용). 실패해도 치명적이지 않음.
		KSelector selector;
		curveOut = selector.Sweep(matrix, paramsOut, kMin, kMax);
		hasCurveOut = curveOut.m_ok;
	}

	paramsOut.m_k = kFinal;

	ClusterEngine engine;
	resultOut = engine.Run(matrix, paramsOut);
	if (!resultOut.m_ok)
	{
		std::cout << "ERROR: 군집화 실패: " << resultOut.m_error << std::endl;
		return false;
	}
	std::cout << "cluster done: k=" << resultOut.m_k << " silhouette=" << resultOut.m_silhouette << std::endl;
	for (size_t c = 0; c < resultOut.m_clusterSizes.size(); ++c)
		std::cout << "  cluster " << c << ": " << resultOut.m_clusterSizes[c] << " samples" << std::endl;
	return true;
}

// --cluster <result.csv> <clusters.json> [--features a,b,c] [--k N|auto] [--kmin N] [--kmax M]
//  [--scale zscore|minmax|robust] [--seed S] [--attempts N] [--log-area] [--cluster-inplace]
int RunClusterMode(int argc, char** argv)
{
	if (argc < 4)
	{
		std::cout << "usage: GlimRegionBatch.exe --cluster <result.csv> <clusters.json>"
			<< " [--features a,b,c] [--k N|auto] [--kmin N] [--kmax M]"
			<< " [--scale zscore|minmax|robust] [--seed S] [--attempts N]"
			<< " [--log-area] [--cluster-inplace]" << std::endl;
		return 1;
	}
	const std::string csvPath = argv[2];
	const std::string jsonPath = argv[3];

	std::vector<std::string> features;
	std::string kStr = "auto";
	int kMin = 2, kMax = 8, seed = 12345, attempts = 5;
	std::string scaleStr = "zscore";
	bool logArea = false;
	bool inplace = false;

	for (int i = 4; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "--features" && i + 1 < argc) features = SplitCommaTrim(argv[++i]);
		else if (a == "--k" && i + 1 < argc) kStr = argv[++i];
		else if (a == "--kmin" && i + 1 < argc) kMin = std::atoi(argv[++i]);
		else if (a == "--kmax" && i + 1 < argc) kMax = std::atoi(argv[++i]);
		else if (a == "--scale" && i + 1 < argc) scaleStr = argv[++i];
		else if (a == "--seed" && i + 1 < argc) seed = std::atoi(argv[++i]);
		else if (a == "--attempts" && i + 1 < argc) attempts = std::atoi(argv[++i]);
		else if (a == "--log-area") logArea = true;
		else if (a == "--cluster-inplace") inplace = true;
	}

	FeatureMatrix matrix;
	std::string err;
	if (!ClusterCsvIO::LoadFeatureMatrix(csvPath, features, matrix, err))
	{
		std::cout << "ERROR: FeatureMatrix 로드 실패: " << err << std::endl;
		return 2;
	}
	std::cout << "loaded: " << matrix.RowCount() << " rows, " << matrix.ColCount() << " features" << std::endl;

	ClusterParams params; ClusterResult result; KSelector::Curve curve; bool hasCurve = false;
	if (!RunClusterOnMatrix(matrix, kStr, kMin, kMax, ScaleModeFromString(scaleStr), logArea, seed, attempts,
		params, result, curve, hasCurve))
		return 2;

	if (!ClusterCsvIO::SaveClusters(jsonPath, matrix, result, params, hasCurve ? &curve : NULL, err))
	{
		std::cout << "ERROR: clusters.json 저장 실패: " << err << std::endl;
		return 2;
	}
	std::cout << "clusters.json saved: " << jsonPath << std::endl;

	if (inplace)
	{
		if (!ClusterCsvIO::WriteClusterIdInplace(csvPath, matrix, result, err))
			std::cout << "WARN: cluster-inplace 갱신 실패: " << err << std::endl;
		else
			std::cout << "cluster_id inplace 갱신 완료: " << csvPath << std::endl;
	}
	return 0;
}

// --cluster-folder <입력폴더> <clusters.json> [기존 이진화 옵션 전부] [군집 옵션 전부]
//  이진화 안 된 원본 불량 크롭 이미지 폴더에서 "이진화->feature 추출->군집화" 를 한 번에 실행한다.
//  내부적으로 CsvExporter::ExportFolder 로 임시 result.csv 를 만든 뒤 --cluster 와 동일한 경로를 탄다.
//  임시 CSV 는 clusters.json 옆에 <clusters.json>.src.csv 로 남긴다(재분석/디버깅/--to-profile 용).
int RunClusterFolderMode(int argc, char** argv)
{
	if (argc < 4)
	{
		std::cout << "usage: GlimRegionBatch.exe --cluster-folder <input_folder> <clusters.json>"
			<< " [--threads N] [--dark|--bright] [--thresh N|auto] [--otsu] [--binary] [--offset N]"
			<< " [--wrinkle [--kernel N] [--blur N] [--response N]]"
			<< " [--proj [--black-th N] [--white-th N] [--proj-kernel N]]"
			<< " [--features a,b,c] [--k N|auto] [--kmin N] [--kmax M]"
			<< " [--scale zscore|minmax|robust] [--seed S] [--attempts N] [--log-area]" << std::endl;
		std::cout << "  실전 기본값(검사기 크롭 실측 검증됨): --dark --thresh auto" << std::endl;
		std::cout << "  e.g.: GlimRegionBatch.exe --cluster-folder D:\\128Crop\\BlackPoint clusters.json --dark --thresh auto" << std::endl;
		return 1;
	}
	const std::string inputDir = argv[2];
	const std::string jsonPath = argv[3];

	std::vector<std::string> features;
	std::string kStr = "auto";
	int kMin = 2, kMax = 8, seed = 12345, attempts = 5, numThreads = 0;
	std::string scaleStr = "zscore";
	bool logArea = false;
	// 실전 기본값(검사기 크롭 이미지 실측 검증): 배경 대비 어두운 불량이 많음 -> dark + 배경평균 기반 자동 임계.
	BinarizeParams binParams;
	binParams.m_polarity = POLARITY_DARK;
	binParams.m_mode = BINMODE_MEAN_OFFSET;

	for (int i = 4; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "--threads" && i + 1 < argc) numThreads = std::atoi(argv[++i]);
		else if (a == "--dark") binParams.m_polarity = POLARITY_DARK;
		else if (a == "--bright") binParams.m_polarity = POLARITY_BRIGHT;
		else if (a == "--binary") binParams.m_mode = BINMODE_BINARY;
		else if (a == "--otsu") binParams.m_mode = BINMODE_OTSU;
		else if (a == "--wrinkle") binParams.m_mode = BINMODE_WRINKLE;
		else if (a == "--kernel" && i + 1 < argc) binParams.m_kernelSize = std::atoi(argv[++i]);
		else if (a == "--blur" && i + 1 < argc) binParams.m_blurH = std::atoi(argv[++i]);
		else if (a == "--response" && i + 1 < argc) binParams.m_responseThresh = std::atof(argv[++i]);
		else if (a == "--proj" || a == "--projection") binParams.m_mode = BINMODE_PROJECTION;
		else if (a == "--black-th" && i + 1 < argc) binParams.m_projBlackTh = std::atof(argv[++i]);
		else if (a == "--white-th" && i + 1 < argc) binParams.m_projWhiteTh = std::atof(argv[++i]);
		else if (a == "--proj-kernel" && i + 1 < argc) binParams.m_projKernel = std::atoi(argv[++i]);
		else if (a == "--offset" && i + 1 < argc) binParams.m_offset = std::atof(argv[++i]);
		else if (a == "--thresh" && i + 1 < argc)
		{
			std::string v = argv[++i];
			if (v == "auto") binParams.m_mode = BINMODE_MEAN_OFFSET;
			else { binParams.m_mode = BINMODE_FIXED; binParams.m_threshold = std::atof(v.c_str()); }
		}
		else if (a == "--features" && i + 1 < argc) features = SplitCommaTrim(argv[++i]);
		else if (a == "--k" && i + 1 < argc) kStr = argv[++i];
		else if (a == "--kmin" && i + 1 < argc) kMin = std::atoi(argv[++i]);
		else if (a == "--kmax" && i + 1 < argc) kMax = std::atoi(argv[++i]);
		else if (a == "--scale" && i + 1 < argc) scaleStr = argv[++i];
		else if (a == "--seed" && i + 1 < argc) seed = std::atoi(argv[++i]);
		else if (a == "--attempts" && i + 1 < argc) attempts = std::atoi(argv[++i]);
		else if (a == "--log-area") logArea = true;
	}

	const std::string tmpCsv = jsonPath + ".src.csv";
	std::cout << "step 1/2: folder -> csv (" << tmpCsv << ")" << std::endl;

	CpuPreprocessor preprocessor(binParams);
	BatchStat stat;
	bool ok = false;
	try
	{
		ok = CsvExporter::ExportFolder(inputDir, tmpCsv, NULL, stat, numThreads, &preprocessor);
	}
	catch (const std::exception& e)
	{
		std::cout << "ERROR: 폴더 분석 중 예외: " << e.what() << std::endl;
		return 2;
	}
	if (!ok)
	{
		std::cout << "ERROR: 입력 폴더/임시 CSV 경로를 확인하세요. (input=" << inputDir << ")" << std::endl;
		return 2;
	}
	std::cout << "  processed " << stat.m_processedFiles << "/" << stat.m_totalFiles
		<< " files, " << stat.m_totalRegions << " regions, " << stat.m_elapsedMs << " ms" << std::endl;
	if (stat.m_totalRegions == 0)
	{
		std::cout << "ERROR: 추출된 Region 이 없습니다(이진화 파라미터를 확인하세요. 기본값: --dark --thresh auto)" << std::endl;
		return 2;
	}

	std::cout << "step 2/2: csv -> cluster" << std::endl;
	FeatureMatrix matrix;
	std::string err;
	if (!ClusterCsvIO::LoadFeatureMatrix(tmpCsv, features, matrix, err))
	{
		std::cout << "ERROR: FeatureMatrix 로드 실패: " << err << std::endl;
		return 2;
	}
	std::cout << "  loaded: " << matrix.RowCount() << " rows, " << matrix.ColCount() << " features" << std::endl;

	ClusterParams params; ClusterResult result; KSelector::Curve curve; bool hasCurve = false;
	if (!RunClusterOnMatrix(matrix, kStr, kMin, kMax, ScaleModeFromString(scaleStr), logArea, seed, attempts,
		params, result, curve, hasCurve))
		return 2;

	if (!ClusterCsvIO::SaveClusters(jsonPath, matrix, result, params, hasCurve ? &curve : NULL, err))
	{
		std::cout << "ERROR: clusters.json 저장 실패: " << err << std::endl;
		return 2;
	}
	std::cout << "clusters.json saved: " << jsonPath << std::endl;
	std::cout << "source csv kept: " << tmpCsv << " (재분석/--to-profile 용)" << std::endl;
	return 0;
}

// --to-profile <clusters.json> <result.csv> <output.ini> --labels clusterId=CODE[,...] [--plow N] [--phigh N] [--name NAME]
//  result.csv 는 clusters.json 을 만들 때 사용한 것과 동일한 파일(또는 --cluster-folder 의 <json>.src.csv)이어야 한다.
int RunToProfileMode(int argc, char** argv)
{
	if (argc < 5)
	{
		std::cout << "usage: GlimRegionBatch.exe --to-profile <clusters.json> <result.csv> <output.ini>"
			<< " --labels clusterId=CODE[,clusterId=CODE...] [--plow N] [--phigh N] [--name NAME]" << std::endl;
		return 1;
	}
	const std::string jsonPath = argv[2];
	const std::string csvPath = argv[3];
	const std::string outIniPath = argv[4];

	std::string labelsStr;
	double pLow = 5.0, pHigh = 95.0;
	std::string profileName = "ClusterDraft";

	for (int i = 5; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "--labels" && i + 1 < argc) labelsStr = argv[++i];
		else if (a == "--plow" && i + 1 < argc) pLow = std::atof(argv[++i]);
		else if (a == "--phigh" && i + 1 < argc) pHigh = std::atof(argv[++i]);
		else if (a == "--name" && i + 1 < argc) profileName = argv[++i];
	}

	if (labelsStr.empty())
	{
		std::cout << "ERROR: --labels 가 필요합니다(예: --labels 0=PINHOLE,1=TEAR)" << std::endl;
		return 1;
	}

	std::map<int, std::string> labels;
	{
		std::vector<std::string> pairs = SplitCommaTrim(labelsStr);
		for (size_t i = 0; i < pairs.size(); ++i)
		{
			size_t eq = pairs[i].find('=');
			if (eq == std::string::npos) continue;
			int cid = std::atoi(pairs[i].substr(0, eq).c_str());
			std::string code = pairs[i].substr(eq + 1);
			if (!code.empty())
				labels[cid] = code;
		}
	}
	if (labels.empty())
	{
		std::cout << "ERROR: --labels 파싱 결과가 비어있습니다" << std::endl;
		return 1;
	}

	ClusterResult result;
	ClusterParams params;
	std::string err;
	if (!ClusterCsvIO::LoadClusters(jsonPath, result, params, err))
	{
		std::cout << "ERROR: clusters.json 로드 실패: " << err << std::endl;
		return 2;
	}

	FeatureMatrix matrix;
	if (!ClusterCsvIO::LoadFeatureMatrix(csvPath, params.m_features, matrix, err))
	{
		std::cout << "ERROR: FeatureMatrix 로드 실패: " << err << std::endl;
		return 2;
	}
	if (matrix.RowCount() != result.m_labels.size())
	{
		std::cout << "ERROR: result.csv 의 Region 행 수(" << matrix.RowCount()
			<< ")가 clusters.json 라벨 수(" << result.m_labels.size()
			<< ")와 다릅니다. clusters.json 을 만든 result.csv 와 동일한 파일인지 확인하세요." << std::endl;
		return 2;
	}

	ProfileWriteParams pwParams;
	pwParams.m_profileName = profileName;
	pwParams.m_labels = labels;
	pwParams.m_percentileLow = pLow;
	pwParams.m_percentileHigh = pHigh;

	ProfileWriter writer;
	bool ok = false;
	std::string iniText = writer.BuildIni(matrix, result, pwParams, params.m_features, ok, err);
	if (!ok)
	{
		std::cout << "ERROR: 프로파일 초안 생성 실패: " << err << std::endl;
		return 2;
	}

	std::ofstream ofs(outIniPath.c_str(), std::ios::binary);
	if (!ofs.is_open())
	{
		std::cout << "ERROR: 출력 INI 파일을 열 수 없습니다: " << outIniPath << std::endl;
		return 2;
	}
	ofs << iniText;
	ofs.close();
	std::cout << "profile draft saved: " << outIniPath << std::endl;
	return 0;
}

} // namespace

int main(int argc, char** argv)
{
	std::cout << "GlimRegionBatch - folder batch inspection" << std::endl;

	if (argc >= 2 && std::string(argv[1]) == "--cluster")
		return RunClusterMode(argc, argv);
	if (argc >= 2 && std::string(argv[1]) == "--cluster-folder")
		return RunClusterFolderMode(argc, argv);
	if (argc >= 2 && std::string(argv[1]) == "--to-profile")
		return RunToProfileMode(argc, argv);

	if (argc < 3)
	{
		std::cout << "usage: GlimRegionBatch.exe <input_folder> <output.csv> [profile.ini]"
			<< " [--threads N] [--dark|--bright] [--thresh N|auto] [--otsu] [--binary] [--offset N]"
			<< " [--wrinkle [--kernel N] [--blur N] [--response N]]"
			<< " [--proj [--black-th N] [--white-th N] [--proj-kernel N]]"
			<< " [--scale-x F] [--scale-y F] [--dumpbin <dir>]"
			<< " [--overlay <dir>] [--preview <file> <out.png>]"
			<< " [--cluster <result.csv> <clusters.json> ...] [--cluster-folder <input_folder> <clusters.json> ...]"
			<< " [--to-profile <clusters.json> <result.csv> <out.ini> ...]" << std::endl;
		std::cout << "  e.g.: GlimRegionBatch.exe D:\\128Crop\\BlackPoint out.csv --dark --thresh auto" << std::endl;
		return 1;
	}

	const std::string inputDir = argv[1];
	const std::string outputCsv = argv[2];

	// 위치 인자(프로파일) + 옵션 파싱
	//  --threads N | --dark | --bright | --otsu | --thresh N|auto | --offset N
	std::string profilePath;
	std::string overlayDir;     // --overlay <dir> (비면 미생성)
	std::string previewFile;    // --preview <파일> <출력png>
	std::string previewOut;
	int numThreads = 0;         // 0 = 자동
	BinarizeParams binParams;   // 기본 FIXED 127 BRIGHT(기존 동작)
	ExportOptions expOpts;      // scale-x/y=1.0, dumpbin 없음(기존 동작)

	// --- 흑/백 독립 채널(dual) 옵션: --dual 이면 흑('B')·백('W')을 각각 다른 params 로 한 번에 추출 ---
	bool dualMode = false;
	bool blackEnabled = true, whiteEnabled = true;
	BinarizeParams blackParams; blackParams.m_polarity = POLARITY_DARK;   blackParams.m_mode = BINMODE_OTSU; // 흑 기본: OTSU/DARK
	BinarizeParams whiteParams; whiteParams.m_polarity = POLARITY_BRIGHT; whiteParams.m_mode = BINMODE_OTSU; // 백 기본: OTSU/BRIGHT

	for (int i = 3; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "--threads")
		{
			if (i + 1 < argc) numThreads = std::atoi(argv[++i]);
		}
		else if (a.rfind("--threads=", 0) == 0)
		{
			numThreads = std::atoi(a.substr(10).c_str());
		}
		else if (a == "--overlay")
		{
			if (i + 1 < argc) overlayDir = argv[++i];
		}
		else if (a.rfind("--overlay=", 0) == 0)
		{
			overlayDir = a.substr(10);
		}
		else if (a == "--preview")
		{
			if (i + 1 < argc) previewFile = argv[++i];
			if (i + 1 < argc) previewOut = argv[++i];
		}
		else if (a == "--dark")
		{
			binParams.m_polarity = POLARITY_DARK;
		}
		else if (a == "--bright")
		{
			binParams.m_polarity = POLARITY_BRIGHT;
		}
		else if (a == "--binary")
		{
			binParams.m_mode = BINMODE_BINARY;
		}
		else if (a == "--otsu")
		{
			binParams.m_mode = BINMODE_OTSU;
		}
		else if (a == "--wrinkle")
		{
			binParams.m_mode = BINMODE_WRINKLE;
		}
		else if (a == "--kernel" || a.rfind("--kernel=", 0) == 0)
		{
			std::string v;
			if (a.rfind("--kernel=", 0) == 0) v = a.substr(9);
			else if (i + 1 < argc) v = argv[++i];
			binParams.m_kernelSize = std::atoi(v.c_str());
		}
		else if (a == "--blur" || a.rfind("--blur=", 0) == 0)
		{
			std::string v;
			if (a.rfind("--blur=", 0) == 0) v = a.substr(7);
			else if (i + 1 < argc) v = argv[++i];
			binParams.m_blurH = std::atoi(v.c_str());
		}
		else if (a == "--response" || a.rfind("--response=", 0) == 0)
		{
			std::string v;
			if (a.rfind("--response=", 0) == 0) v = a.substr(11);
			else if (i + 1 < argc) v = argv[++i];
			binParams.m_responseThresh = std::atof(v.c_str());
		}
		else if (a == "--proj" || a == "--projection")
		{
			binParams.m_mode = BINMODE_PROJECTION;
		}
		else if (a == "--black-th" || a.rfind("--black-th=", 0) == 0)
		{
			std::string v;
			if (a.rfind("--black-th=", 0) == 0) v = a.substr(11);
			else if (i + 1 < argc) v = argv[++i];
			binParams.m_projBlackTh = std::atof(v.c_str());
		}
		else if (a == "--white-th" || a.rfind("--white-th=", 0) == 0)
		{
			std::string v;
			if (a.rfind("--white-th=", 0) == 0) v = a.substr(11);
			else if (i + 1 < argc) v = argv[++i];
			binParams.m_projWhiteTh = std::atof(v.c_str());
		}
		else if (a == "--proj-kernel" || a.rfind("--proj-kernel=", 0) == 0)
		{
			std::string v;
			if (a.rfind("--proj-kernel=", 0) == 0) v = a.substr(14);
			else if (i + 1 < argc) v = argv[++i];
			binParams.m_projKernel = std::atoi(v.c_str());
		}
		else if (a == "--scale-x" || a.rfind("--scale-x=", 0) == 0)
		{
			std::string v;
			if (a.rfind("--scale-x=", 0) == 0) v = a.substr(10);
			else if (i + 1 < argc) v = argv[++i];
			expOpts.m_scaleX = std::atof(v.c_str());
		}
		else if (a == "--scale-y" || a.rfind("--scale-y=", 0) == 0)
		{
			std::string v;
			if (a.rfind("--scale-y=", 0) == 0) v = a.substr(10);
			else if (i + 1 < argc) v = argv[++i];
			expOpts.m_scaleY = std::atof(v.c_str());
		}
		else if (a == "--dumpbin")
		{
			if (i + 1 < argc) expOpts.m_dumpBinDir = argv[++i];
		}
		else if (a.rfind("--dumpbin=", 0) == 0)
		{
			expOpts.m_dumpBinDir = a.substr(10);
		}
		else if (a == "--thresh" || a.rfind("--thresh=", 0) == 0)
		{
			std::string v;
			if (a.rfind("--thresh=", 0) == 0) v = a.substr(9);
			else if (i + 1 < argc) v = argv[++i];

			if (v == "auto")
				binParams.m_mode = BINMODE_MEAN_OFFSET;
			else
			{
				binParams.m_mode = BINMODE_FIXED;
				binParams.m_threshold = std::atof(v.c_str());
			}
		}
		else if (a == "--offset" || a.rfind("--offset=", 0) == 0)
		{
			std::string v;
			if (a.rfind("--offset=", 0) == 0) v = a.substr(9);
			else if (i + 1 < argc) v = argv[++i];
			binParams.m_offset = std::atof(v.c_str());
		}
		else if (a == "--dual")
		{
			dualMode = true;
		}
		else if (a == "--no-bk")
		{
			blackEnabled = false;
		}
		else if (a == "--no-wt")
		{
			whiteEnabled = false;
		}
		else if (a == "--bk-th" && i + 1 < argc)
		{
			std::string v = argv[++i];
			if (v == "auto") blackParams.m_mode = BINMODE_OTSU;
			else { blackParams.m_mode = BINMODE_FIXED; blackParams.m_threshold = std::atof(v.c_str()); }
		}
		else if (a == "--wt-th" && i + 1 < argc)
		{
			std::string v = argv[++i];
			if (v == "auto") whiteParams.m_mode = BINMODE_OTSU;
			else { whiteParams.m_mode = BINMODE_FIXED; whiteParams.m_threshold = std::atof(v.c_str()); }
		}
		else if (a == "--bk-offset" && i + 1 < argc)
		{
			blackParams.m_mode = BINMODE_MEAN_OFFSET; blackParams.m_offset = std::atof(argv[++i]);
		}
		else if (a == "--wt-offset" && i + 1 < argc)
		{
			whiteParams.m_mode = BINMODE_MEAN_OFFSET; whiteParams.m_offset = std::atof(argv[++i]);
		}
		else if (profilePath.empty())
		{
			profilePath = a; // 첫 비옵션 = 프로파일 경로
		}
	}

	// --- 미리보기 모드: 파일 1장 이진화 결과만 PNG 저장 후 종료(배치 로직과 동일) ---
	if (!previewFile.empty() && !previewOut.empty())
	{
		try
		{
			cv::Mat src = cv::imread(previewFile, cv::IMREAD_GRAYSCALE);
			if (src.empty())
			{
				std::cout << "preview: load failed: " << previewFile << std::endl;
				return 2;
			}
			CpuPreprocessor pre(binParams);
			// PROJECTION 은 흑/백 2채널을 가로로 이어붙여 1장으로 반환(웹에서 나란히 표시).
			cv::Mat bin;
			std::vector<BinChannel> chans = pre.BinarizeMulti(src);
			if (chans.size() >= 2)
			{
				cv::hconcat(chans[0].image, chans[1].image, bin); // B | W
			}
			else if (chans.size() == 1)
			{
				bin = chans[0].image;
			}
			if (bin.empty())
			{
				std::cout << "preview: binarize failed" << std::endl;
				return 2;
			}
			if (!cv::imwrite(previewOut, bin))
			{
				std::cout << "preview: write failed: " << previewOut << std::endl;
				return 2;
			}
			std::cout << "preview saved: " << previewOut << std::endl;
		}
		catch (const std::exception& e)
		{
			std::cout << "preview: exception: " << e.what() << std::endl;
			return 2;
		}
		return 0;
	}

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

	std::cout << "threads: " << (numThreads > 0 ? std::to_string(numThreads) : std::string("auto")) << std::endl;

	const char* modeStr = (binParams.m_mode == BINMODE_OTSU) ? "otsu"
		: (binParams.m_mode == BINMODE_MEAN_OFFSET) ? "mean_offset"
		: (binParams.m_mode == BINMODE_BINARY) ? "binary"
		: (binParams.m_mode == BINMODE_WRINKLE) ? "wrinkle"
		: (binParams.m_mode == BINMODE_PROJECTION) ? "projection" : "fixed";
	const char* polStr = (binParams.m_polarity == POLARITY_DARK) ? "dark" : "bright";
	std::cout << "binarize: mode=" << modeStr << " polarity=" << polStr
		<< " threshold=" << binParams.m_threshold
		<< " offset=" << binParams.m_offset;
	if (binParams.m_mode == BINMODE_WRINKLE)
		std::cout << " kernel=" << binParams.m_kernelSize
			<< " blur=" << binParams.m_blurH
			<< " response=" << binParams.m_responseThresh;
	if (binParams.m_mode == BINMODE_PROJECTION)
		std::cout << " black-th=" << binParams.m_projBlackTh
			<< " white-th=" << binParams.m_projWhiteTh
			<< " proj-kernel=" << binParams.m_projKernel;
	std::cout << std::endl;

	std::cout << "scale: x=" << expOpts.m_scaleX << " y=" << expOpts.m_scaleY << std::endl;
	if (!overlayDir.empty())
		std::cout << "overlay: " << overlayDir << std::endl;
	if (!expOpts.m_dumpBinDir.empty())
		std::cout << "dumpbin: " << expOpts.m_dumpBinDir << std::endl;

	CpuPreprocessor preprocessor(binParams);
	DualChannelPreprocessor dualPre(blackParams, blackEnabled, whiteParams, whiteEnabled);
	const IPreprocessor* prep = dualMode
		? static_cast<const IPreprocessor*>(&dualPre)
		: static_cast<const IPreprocessor*>(&preprocessor);
	if (dualMode)
	{
		const char* bkMode = (blackParams.m_mode == BINMODE_OTSU) ? "otsu"
			: (blackParams.m_mode == BINMODE_MEAN_OFFSET) ? "mean_offset" : "fixed";
		const char* wtMode = (whiteParams.m_mode == BINMODE_OTSU) ? "otsu"
			: (whiteParams.m_mode == BINMODE_MEAN_OFFSET) ? "mean_offset" : "fixed";
		std::cout << "binarize: DUAL  black(" << (blackEnabled ? "on" : "off") << ",dark," << bkMode
			<< ",th=" << blackParams.m_threshold << ")  white(" << (whiteEnabled ? "on" : "off")
			<< ",bright," << wtMode << ",th=" << whiteParams.m_threshold << ")" << std::endl;
	}

	BatchStat stat;
	bool ok = false;
	try
	{
		ok = CsvExporter::ExportFolder(inputDir, outputCsv, profilePtr, stat,
			numThreads, prep, overlayDir, expOpts);
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
	std::cout << "  elapsed (ms)    : " << stat.m_elapsedMs << std::endl;
	std::cout << "  output csv      : " << outputCsv << std::endl;
	std::cout << "  failed files    : " << stat.m_failedFiles.size() << std::endl;
	for (size_t i = 0; i < stat.m_failedFiles.size(); ++i)
		std::cout << "    - " << stat.m_failedFiles[i] << std::endl;
	std::cout << "==================================================" << std::endl;

	return stat.m_failedFiles.empty() ? 0 : 3;
}
