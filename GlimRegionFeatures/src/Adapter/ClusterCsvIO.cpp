// ClusterCsvIO.cpp
#include "Adapter/ClusterCsvIO.h"

#include <fstream>
#include <sstream>
#include <map>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace Grf {

namespace {
	// CSV 한 줄을 따옴표 규칙(CsvExporter::CsvQuote 의 역)에 맞춰 분리.
	std::vector<std::string> SplitCsvLine(const std::string& line)
	{
		std::vector<std::string> out;
		std::string cur;
		bool inQuotes = false;
		for (size_t i = 0; i < line.size(); ++i)
		{
			char c = line[i];
			if (inQuotes)
			{
				if (c == '"')
				{
					if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; }
					else inQuotes = false;
				}
				else cur += c;
			}
			else
			{
				if (c == '"') inQuotes = true;
				else if (c == ',') { out.push_back(cur); cur.clear(); }
				else cur += c;
			}
		}
		out.push_back(cur);
		return out;
	}

	double ParseDoubleSafe(const std::string& s)
	{
		if (s.empty())
			return 0.0;
		try { return std::atof(s.c_str()); }
		catch (...) { return 0.0; }
	}

	void StripEol(std::string& line)
	{
		while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
			line.pop_back();
	}

	// result.csv 헤더 중 "특징값이 아닌" 컬럼(식별자/score_*/분류/mm 파생) 판정.
	bool IsIdentifierOrDerivedColumn(const std::string& name)
	{
		if (name == "FileName" || name == "FilePath" || name == "RegionIndex" || name == "Channel"
			|| name == "ClassifiedCode")
			return true;
		if (name.rfind("score_", 0) == 0)
			return true;
		if (name == "area_mm2" || name == "width_mm" || name == "height_mm" || name == "diameter_mm")
			return true;
		return false;
	}

	// --- 최소 JSON 파서(자체 스키마 전용, clusters.json 읽기 용도) ---
	struct JsonValue {
		enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type;
		bool b;
		double num;
		std::string str;
		std::vector<JsonValue> arr;
		std::map<std::string, JsonValue> obj;

		JsonValue() : type(NUL), b(false), num(0.0) {}
	};

	class JsonParser {
	public:
		explicit JsonParser(const std::string& text) : m_s(text), m_i(0) {}

		bool Parse(JsonValue& out)
		{
			SkipWs();
			return ParseValue(out);
		}

	private:
		const std::string& m_s;
		size_t m_i;

		void SkipWs()
		{
			while (m_i < m_s.size() && std::isspace(static_cast<unsigned char>(m_s[m_i])))
				++m_i;
		}

		bool ParseValue(JsonValue& out)
		{
			SkipWs();
			if (m_i >= m_s.size())
				return false;
			const char c = m_s[m_i];
			if (c == '{') return ParseObject(out);
			if (c == '[') return ParseArray(out);
			if (c == '"') return ParseString(out);
			if (c == 't' || c == 'f') return ParseBool(out);
			if (c == 'n')
			{
				if (m_s.compare(m_i, 4, "null") == 0) { m_i += 4; out.type = JsonValue::NUL; return true; }
				return false;
			}
			return ParseNumber(out);
		}

		bool ParseObject(JsonValue& out)
		{
			out.type = JsonValue::OBJ;
			++m_i; // '{'
			SkipWs();
			if (m_i < m_s.size() && m_s[m_i] == '}') { ++m_i; return true; }
			for (;;)
			{
				SkipWs();
				JsonValue key;
				if (!ParseString(key))
					return false;
				SkipWs();
				if (m_i >= m_s.size() || m_s[m_i] != ':')
					return false;
				++m_i;
				JsonValue val;
				if (!ParseValue(val))
					return false;
				out.obj[key.str] = val;
				SkipWs();
				if (m_i < m_s.size() && m_s[m_i] == ',') { ++m_i; continue; }
				if (m_i < m_s.size() && m_s[m_i] == '}') { ++m_i; break; }
				return false;
			}
			return true;
		}

		bool ParseArray(JsonValue& out)
		{
			out.type = JsonValue::ARR;
			++m_i; // '['
			SkipWs();
			if (m_i < m_s.size() && m_s[m_i] == ']') { ++m_i; return true; }
			for (;;)
			{
				JsonValue val;
				if (!ParseValue(val))
					return false;
				out.arr.push_back(val);
				SkipWs();
				if (m_i < m_s.size() && m_s[m_i] == ',') { ++m_i; continue; }
				if (m_i < m_s.size() && m_s[m_i] == ']') { ++m_i; break; }
				return false;
			}
			return true;
		}

		bool ParseString(JsonValue& out)
		{
			if (m_i >= m_s.size() || m_s[m_i] != '"')
				return false;
			++m_i;
			std::string res;
			while (m_i < m_s.size() && m_s[m_i] != '"')
			{
				char c = m_s[m_i];
				if (c == '\\' && m_i + 1 < m_s.size())
				{
					char n = m_s[m_i + 1];
					switch (n)
					{
						case '"': res += '"'; break;
						case '\\': res += '\\'; break;
						case '/': res += '/'; break;
						case 'n': res += '\n'; break;
						case 'r': res += '\r'; break;
						case 't': res += '\t'; break;
						case 'u':
							// 유니코드 이스케이프는 이 스키마에서 생성하지 않으므로(비-ASCII 는 원문 UTF-8
							// 그대로 기록) 자리표시자로만 처리한다.
							res += '?';
							m_i += 4;
							break;
						default: res += n; break;
					}
					m_i += 2;
				}
				else { res += c; ++m_i; }
			}
			if (m_i >= m_s.size())
				return false;
			++m_i; // closing quote
			out.type = JsonValue::STR;
			out.str = res;
			return true;
		}

		bool ParseBool(JsonValue& out)
		{
			if (m_s.compare(m_i, 4, "true") == 0) { out.type = JsonValue::BOOL; out.b = true; m_i += 4; return true; }
			if (m_s.compare(m_i, 5, "false") == 0) { out.type = JsonValue::BOOL; out.b = false; m_i += 5; return true; }
			return false;
		}

		bool ParseNumber(JsonValue& out)
		{
			const size_t start = m_i;
			if (m_i < m_s.size() && (m_s[m_i] == '-' || m_s[m_i] == '+'))
				++m_i;
			while (m_i < m_s.size() &&
				(std::isdigit(static_cast<unsigned char>(m_s[m_i])) || m_s[m_i] == '.' ||
				 m_s[m_i] == 'e' || m_s[m_i] == 'E' || m_s[m_i] == '-' || m_s[m_i] == '+'))
				++m_i;
			if (m_i == start)
				return false;
			out.type = JsonValue::NUM;
			out.num = std::atof(m_s.substr(start, m_i - start).c_str());
			return true;
		}
	};
}

bool ClusterCsvIO::LoadFeatureMatrix(const std::string& csvPath,
	const std::vector<std::string>& featureNames,
	FeatureMatrix& matrixOut, std::string& errorOut)
{
	matrixOut = FeatureMatrix();
	errorOut.clear();
	try
	{
		std::ifstream in(csvPath.c_str(), std::ios::binary);
		if (!in.is_open())
		{
			errorOut = "CSV 파일을 열 수 없습니다: " + csvPath;
			return false;
		}

		std::string headerLine;
		if (!std::getline(in, headerLine))
		{
			errorOut = "CSV 가 비어있습니다: " + csvPath;
			return false;
		}
		StripEol(headerLine);

		std::vector<std::string> headers = SplitCsvLine(headerLine);
		std::map<std::string, int> colOf;
		for (size_t i = 0; i < headers.size(); ++i)
			colOf[headers[i]] = static_cast<int>(i);

		const int idxFileName = colOf.count("FileName") ? colOf["FileName"] : -1;
		const int idxRegionIndex = colOf.count("RegionIndex") ? colOf["RegionIndex"] : -1;
		const int idxChannel = colOf.count("Channel") ? colOf["Channel"] : -1;
		if (idxFileName < 0 || idxRegionIndex < 0)
		{
			errorOut = "CSV 헤더에 FileName/RegionIndex 컬럼이 없습니다(형식 확인 필요): " + csvPath;
			return false;
		}

		std::vector<std::string> useFeat = featureNames;
		if (useFeat.empty())
		{
			for (size_t i = 0; i < headers.size(); ++i)
				if (!IsIdentifierOrDerivedColumn(headers[i]))
					useFeat.push_back(headers[i]);
		}

		std::vector<int> featCol;
		std::vector<std::string> featValid;
		for (size_t i = 0; i < useFeat.size(); ++i)
		{
			std::map<std::string, int>::const_iterator it = colOf.find(useFeat[i]);
			if (it != colOf.end())
			{
				featCol.push_back(it->second);
				featValid.push_back(useFeat[i]);
			}
		}
		if (featValid.empty())
		{
			errorOut = "선택한 feature 이름을 CSV 헤더에서 찾을 수 없습니다";
			return false;
		}

		matrixOut.SetColumns(featValid);

		std::string line;
		while (std::getline(in, line))
		{
			StripEol(line);
			if (line.empty())
				continue;
			std::vector<std::string> fields = SplitCsvLine(line);
			if (static_cast<int>(fields.size()) <= idxRegionIndex)
				continue;

			const int regionIndex = static_cast<int>(ParseDoubleSafe(fields[idxRegionIndex]));
			if (regionIndex < 0)
				continue; // NO_REGION 행 제외

			std::vector<double> row(featCol.size(), 0.0);
			bool rowOk = true;
			for (size_t f = 0; f < featCol.size(); ++f)
			{
				const int c = featCol[f];
				if (c < 0 || c >= static_cast<int>(fields.size())) { rowOk = false; break; }
				row[f] = ParseDoubleSafe(fields[c]);
			}
			if (!rowOk)
				continue;

			const std::string fileName = (idxFileName < static_cast<int>(fields.size())) ? fields[idxFileName] : std::string();
			matrixOut.AddSample(fileName, regionIndex, row);
			matrixOut.m_sampleChannel.back() =
				(idxChannel >= 0 && idxChannel < static_cast<int>(fields.size())) ? fields[idxChannel] : std::string();
		}

		if (matrixOut.RowCount() == 0)
		{
			errorOut = "유효한 Region 행이 없습니다(전부 NO_REGION 이거나 CSV 가 비어있음): " + csvPath;
			return false;
		}
		return true;
	}
	catch (const std::exception& e)
	{
		errorOut = std::string("std::exception: ") + e.what();
		return false;
	}
	catch (...)
	{
		errorOut = "알 수 없는 예외";
		return false;
	}
}

std::string ClusterCsvIO::JsonEscape(const std::string& s)
{
	std::string out;
	out.reserve(s.size() + 4);
	for (size_t i = 0; i < s.size(); ++i)
	{
		const unsigned char c = static_cast<unsigned char>(s[i]);
		switch (c)
		{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20)
				{
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04x", c);
					out += buf;
				}
				else
				{
					out += static_cast<char>(c); // 비-ASCII(UTF-8 멀티바이트)는 그대로 통과
				}
		}
	}
	return out;
}

std::string ClusterCsvIO::JsonNum(double v)
{
	if (!std::isfinite(v))
		return "0"; // NaN/Inf 는 JSON 미지원 -> 방어적으로 0
	std::ostringstream oss;
	oss.precision(10);
	oss << v;
	return oss.str();
}

bool ClusterCsvIO::SaveClusters(const std::string& jsonPath, const FeatureMatrix& matrix,
	const ClusterResult& result, const ClusterParams& params,
	const KSelector::Curve* curve, std::string& errorOut)
{
	errorOut.clear();
	try
	{
		if (!result.m_ok)
		{
			errorOut = "실패한 ClusterResult 는 저장하지 않습니다: " + result.m_error;
			return false;
		}

		std::ofstream ofs(jsonPath.c_str(), std::ios::binary);
		if (!ofs.is_open())
		{
			errorOut = "clusters.json 파일을 열 수 없습니다: " + jsonPath;
			return false;
		}

		std::ostringstream oss;
		oss << "{\n";

		oss << "  \"params\": {\n";
		oss << "    \"features\": [";
		for (size_t i = 0; i < params.m_features.size(); ++i)
			oss << (i ? "," : "") << "\"" << JsonEscape(params.m_features[i]) << "\"";
		oss << "],\n";
		oss << "    \"scaleMode\": " << params.m_scaleMode << ",\n";
		oss << "    \"logAreaLike\": " << (params.m_logAreaLike ? "true" : "false") << ",\n";
		oss << "    \"k\": " << params.m_k << ",\n";
		oss << "    \"kMax\": " << params.m_kMax << ",\n";
		oss << "    \"seed\": " << params.m_seed << ",\n";
		oss << "    \"attempts\": " << params.m_attempts << "\n";
		oss << "  },\n";

		oss << "  \"k\": " << result.m_k << ",\n";
		oss << "  \"silhouette\": " << JsonNum(result.m_silhouette) << ",\n";

		oss << "  \"kCurve\": ";
		if (curve != NULL && curve->m_ok)
		{
			oss << "{\n";
			oss << "    \"k\": [";
			for (size_t i = 0; i < curve->m_k.size(); ++i) oss << (i ? "," : "") << curve->m_k[i];
			oss << "],\n";
			oss << "    \"silhouette\": [";
			for (size_t i = 0; i < curve->m_silhouette.size(); ++i) oss << (i ? "," : "") << JsonNum(curve->m_silhouette[i]);
			oss << "],\n";
			oss << "    \"inertia\": [";
			for (size_t i = 0; i < curve->m_inertia.size(); ++i) oss << (i ? "," : "") << JsonNum(curve->m_inertia[i]);
			oss << "],\n";
			oss << "    \"recommendedK\": " << curve->m_recommendedK << "\n";
			oss << "  },\n";
		}
		else
		{
			oss << "null,\n";
		}

		oss << "  \"scaler\": {\n";
		oss << "    \"features\": [";
		for (size_t i = 0; i < result.m_scaler.m_features.size(); ++i)
			oss << (i ? "," : "") << "\"" << JsonEscape(result.m_scaler.m_features[i]) << "\"";
		oss << "],\n";
		oss << "    \"mean\": [";
		for (size_t i = 0; i < result.m_scaler.m_mean.size(); ++i) oss << (i ? "," : "") << JsonNum(result.m_scaler.m_mean[i]);
		oss << "],\n";
		oss << "    \"scale\": [";
		for (size_t i = 0; i < result.m_scaler.m_scale.size(); ++i) oss << (i ? "," : "") << JsonNum(result.m_scaler.m_scale[i]);
		oss << "],\n";
		oss << "    \"log1p\": [";
		for (size_t i = 0; i < result.m_scaler.m_log1p.size(); ++i) oss << (i ? "," : "") << (result.m_scaler.m_log1p[i] ? "true" : "false");
		oss << "]\n";
		oss << "  },\n";

		oss << "  \"clusterSizes\": [";
		for (size_t i = 0; i < result.m_clusterSizes.size(); ++i) oss << (i ? "," : "") << result.m_clusterSizes[i];
		oss << "],\n";

		oss << "  \"centroids\": [\n";
		for (size_t c = 0; c < result.m_centroids.size(); ++c)
		{
			oss << "    [";
			for (size_t f = 0; f < result.m_centroids[c].size(); ++f)
				oss << (f ? "," : "") << JsonNum(result.m_centroids[c][f]);
			oss << "]" << (c + 1 < result.m_centroids.size() ? "," : "") << "\n";
		}
		oss << "  ],\n";

		oss << "  \"assignments\": [\n";
		const size_t n = matrix.RowCount();
		for (size_t i = 0; i < n; ++i)
		{
			oss << "    {\"fileName\": \"" << JsonEscape(matrix.m_sampleFile[i]) << "\", "
				<< "\"regionIndex\": " << matrix.m_sampleRegionIndex[i] << ", "
				<< "\"channel\": \"" << JsonEscape(i < matrix.m_sampleChannel.size() ? matrix.m_sampleChannel[i] : std::string()) << "\", "
				<< "\"cluster\": " << (i < result.m_labels.size() ? result.m_labels[i] : -1) << "}"
				<< (i + 1 < n ? "," : "") << "\n";
		}
		oss << "  ]\n";
		oss << "}\n";

		ofs << oss.str();
		ofs.close();
		return true;
	}
	catch (const std::exception& e)
	{
		errorOut = std::string("std::exception: ") + e.what();
		return false;
	}
	catch (...)
	{
		errorOut = "알 수 없는 예외";
		return false;
	}
}

bool ClusterCsvIO::LoadClusters(const std::string& jsonPath,
	ClusterResult& resultOut, ClusterParams& paramsOut, std::string& errorOut)
{
	resultOut = ClusterResult();
	paramsOut = ClusterParams();
	errorOut.clear();
	try
	{
		std::ifstream in(jsonPath.c_str(), std::ios::binary);
		if (!in.is_open())
		{
			errorOut = "clusters.json 을 열 수 없습니다: " + jsonPath;
			return false;
		}
		std::ostringstream buf;
		buf << in.rdbuf();
		const std::string text = buf.str();

		JsonValue root;
		JsonParser parser(text);
		if (!parser.Parse(root) || root.type != JsonValue::OBJ)
		{
			errorOut = "clusters.json 파싱 실패(형식 오류): " + jsonPath;
			return false;
		}

		std::map<std::string, JsonValue>::const_iterator it;

		it = root.obj.find("params");
		if (it != root.obj.end() && it->second.type == JsonValue::OBJ)
		{
			const JsonValue& p = it->second;
			std::map<std::string, JsonValue>::const_iterator pit;
			pit = p.obj.find("features");
			if (pit != p.obj.end() && pit->second.type == JsonValue::ARR)
				for (size_t i = 0; i < pit->second.arr.size(); ++i)
					paramsOut.m_features.push_back(pit->second.arr[i].str);
			pit = p.obj.find("scaleMode"); if (pit != p.obj.end()) paramsOut.m_scaleMode = static_cast<int>(pit->second.num);
			pit = p.obj.find("logAreaLike"); if (pit != p.obj.end()) paramsOut.m_logAreaLike = pit->second.b;
			pit = p.obj.find("k"); if (pit != p.obj.end()) paramsOut.m_k = static_cast<int>(pit->second.num);
			pit = p.obj.find("kMax"); if (pit != p.obj.end()) paramsOut.m_kMax = static_cast<int>(pit->second.num);
			pit = p.obj.find("seed"); if (pit != p.obj.end()) paramsOut.m_seed = static_cast<int>(pit->second.num);
			pit = p.obj.find("attempts"); if (pit != p.obj.end()) paramsOut.m_attempts = static_cast<int>(pit->second.num);
		}

		it = root.obj.find("k"); if (it != root.obj.end()) resultOut.m_k = static_cast<int>(it->second.num);
		it = root.obj.find("silhouette"); if (it != root.obj.end()) resultOut.m_silhouette = it->second.num;

		it = root.obj.find("scaler");
		if (it != root.obj.end() && it->second.type == JsonValue::OBJ)
		{
			const JsonValue& sc = it->second;
			std::map<std::string, JsonValue>::const_iterator sit;
			sit = sc.obj.find("features");
			if (sit != sc.obj.end() && sit->second.type == JsonValue::ARR)
				for (size_t i = 0; i < sit->second.arr.size(); ++i)
					resultOut.m_scaler.m_features.push_back(sit->second.arr[i].str);
			sit = sc.obj.find("mean");
			if (sit != sc.obj.end() && sit->second.type == JsonValue::ARR)
				for (size_t i = 0; i < sit->second.arr.size(); ++i)
					resultOut.m_scaler.m_mean.push_back(sit->second.arr[i].num);
			sit = sc.obj.find("scale");
			if (sit != sc.obj.end() && sit->second.type == JsonValue::ARR)
				for (size_t i = 0; i < sit->second.arr.size(); ++i)
					resultOut.m_scaler.m_scale.push_back(sit->second.arr[i].num);
			sit = sc.obj.find("log1p");
			if (sit != sc.obj.end() && sit->second.type == JsonValue::ARR)
				for (size_t i = 0; i < sit->second.arr.size(); ++i)
					resultOut.m_scaler.m_log1p.push_back(sit->second.arr[i].b);
		}

		it = root.obj.find("clusterSizes");
		if (it != root.obj.end() && it->second.type == JsonValue::ARR)
			for (size_t i = 0; i < it->second.arr.size(); ++i)
				resultOut.m_clusterSizes.push_back(static_cast<int>(it->second.arr[i].num));

		it = root.obj.find("centroids");
		if (it != root.obj.end() && it->second.type == JsonValue::ARR)
		{
			for (size_t c = 0; c < it->second.arr.size(); ++c)
			{
				std::vector<double> row;
				const JsonValue& rowVal = it->second.arr[c];
				if (rowVal.type == JsonValue::ARR)
					for (size_t f = 0; f < rowVal.arr.size(); ++f)
						row.push_back(rowVal.arr[f].num);
				resultOut.m_centroids.push_back(row);
			}
		}

		it = root.obj.find("assignments");
		if (it != root.obj.end() && it->second.type == JsonValue::ARR)
		{
			for (size_t i = 0; i < it->second.arr.size(); ++i)
			{
				const JsonValue& a = it->second.arr[i];
				int cluster = -1;
				std::map<std::string, JsonValue>::const_iterator ait = a.obj.find("cluster");
				if (ait != a.obj.end())
					cluster = static_cast<int>(ait->second.num);
				resultOut.m_labels.push_back(cluster);
			}
		}

		resultOut.m_ok = true;
		return true;
	}
	catch (const std::exception& e)
	{
		errorOut = std::string("std::exception: ") + e.what();
		return false;
	}
	catch (...)
	{
		errorOut = "알 수 없는 예외";
		return false;
	}
}

bool ClusterCsvIO::WriteClusterIdInplace(const std::string& csvPath,
	const FeatureMatrix& matrix, const ClusterResult& result, std::string& errorOut)
{
	errorOut.clear();
	try
	{
		if (!result.m_ok || result.m_labels.size() != matrix.RowCount())
		{
			errorOut = "ClusterResult 가 유효하지 않거나 matrix 와 라벨 수가 다릅니다";
			return false;
		}

		std::ifstream in(csvPath.c_str(), std::ios::binary);
		if (!in.is_open())
		{
			errorOut = "CSV 파일을 열 수 없습니다: " + csvPath;
			return false;
		}
		std::vector<std::string> lines;
		std::string line;
		while (std::getline(in, line))
		{
			StripEol(line);
			lines.push_back(line);
		}
		in.close();
		if (lines.empty())
		{
			errorOut = "CSV 가 비어있습니다: " + csvPath;
			return false;
		}

		std::vector<std::string> headers = SplitCsvLine(lines[0]);
		std::map<std::string, int> colOf;
		for (size_t i = 0; i < headers.size(); ++i)
			colOf[headers[i]] = static_cast<int>(i);
		const int idxFileName = colOf.count("FileName") ? colOf["FileName"] : -1;
		const int idxRegionIndex = colOf.count("RegionIndex") ? colOf["RegionIndex"] : -1;
		const int idxChannel = colOf.count("Channel") ? colOf["Channel"] : -1;
		if (idxFileName < 0 || idxRegionIndex < 0)
		{
			errorOut = "CSV 헤더 형식이 예상과 다릅니다: " + csvPath;
			return false;
		}

		// key = fileName \x1f regionIndex \x1f channel -> cluster label
		std::map<std::string, int> keyToCluster;
		for (size_t i = 0; i < matrix.RowCount(); ++i)
		{
			const std::string key = matrix.m_sampleFile[i] + "\x1f" + std::to_string(matrix.m_sampleRegionIndex[i])
				+ "\x1f" + (i < matrix.m_sampleChannel.size() ? matrix.m_sampleChannel[i] : std::string());
			keyToCluster[key] = (i < result.m_labels.size()) ? result.m_labels[i] : -1;
		}

		std::ofstream ofs(csvPath.c_str(), std::ios::binary | std::ios::trunc);
		if (!ofs.is_open())
		{
			errorOut = "CSV 파일을 다시 열 수 없습니다(쓰기): " + csvPath;
			return false;
		}
		ofs << lines[0] << ",cluster_id\r\n";
		for (size_t li = 1; li < lines.size(); ++li)
		{
			if (lines[li].empty())
				continue;
			std::vector<std::string> fields = SplitCsvLine(lines[li]);
			const std::string fileName = (idxFileName < static_cast<int>(fields.size())) ? fields[idxFileName] : std::string();
			const std::string regionIdx = (idxRegionIndex < static_cast<int>(fields.size())) ? fields[idxRegionIndex] : std::string();
			const std::string channel = (idxChannel >= 0 && idxChannel < static_cast<int>(fields.size())) ? fields[idxChannel] : std::string();
			const std::string key = fileName + "\x1f" + regionIdx + "\x1f" + channel;

			std::map<std::string, int>::const_iterator it = keyToCluster.find(key);
			ofs << lines[li] << ",";
			if (it != keyToCluster.end() && it->second >= 0)
				ofs << it->second;
			ofs << "\r\n";
		}
		ofs.close();
		return true;
	}
	catch (const std::exception& e)
	{
		errorOut = std::string("std::exception: ") + e.what();
		return false;
	}
	catch (...)
	{
		errorOut = "알 수 없는 예외";
		return false;
	}
}

} // namespace Grf
