#pragma once
// FeatureMatrix.h
// Domain 모델: 군집화 입력용 N(샘플)xM(특징) 행렬.
// 각 행은 CSV(result.csv) 의 한 Region 행(RegionIndex>=0)에 대응하며,
// 정렬 순서는 CSV 등장 순서를 그대로 유지한다(결정성 보장 — 군집 결정성 규칙 3).
// 값은 항상 "원시(스케일 전)" 특징값이다. 스케일링은 UseCase/FeatureScaler 가 담당한다.

#include <string>
#include <vector>

namespace Grf {

struct FeatureMatrix {
	std::vector<std::string> m_columns;         // 특징값 이름(열 순서 고정, GetByName/CSV 헤더와 동일 표기)
	std::vector<std::vector<double> > m_rows;    // [row][col] 원시 특징값
	std::vector<std::string> m_sampleFile;       // 행별 FileName(키)
	std::vector<int> m_sampleRegionIndex;        // 행별 RegionIndex(키)
	std::vector<std::string> m_sampleChannel;    // 행별 Channel(B/W, 키 보조. 없으면 공란)

	size_t RowCount() const { return m_rows.size(); }
	size_t ColCount() const { return m_columns.size(); }
	bool Empty() const { return m_rows.empty() || m_columns.empty(); }

	// 컬럼 이름 -> 인덱스(없으면 -1)
	int ColumnIndex(const std::string& name) const;

	// --- 캡슐화 API(뷰어/외부 호출자용. 위 공개 멤버 직접 접근과 병행 가능) ---
	// 컬럼(특징 이름) 정의. 이후 AddSample 의 values 순서는 이 순서와 일치해야 한다.
	//  (열을 재정의하면 기존 행 데이터와 열 수가 어긋날 수 있으므로 데이터 추가 전에 호출할 것)
	void SetColumns(const std::vector<std::string>& names) { m_columns = names; }

	// 샘플 1행 추가. values.size() != 컬럼 수 이면 추가하지 않고 false 반환(방어적).
	//  호출 순서 = 결과 ClusterResult::m_labels 의 인덱스 순서(엔진이 반드시 보존한다).
	bool AddSample(const std::string& fileName, int regionIndex, const std::vector<double>& values)
	{
		if (values.size() != m_columns.size())
			return false;
		m_rows.push_back(values);
		m_sampleFile.push_back(fileName);
		m_sampleRegionIndex.push_back(regionIndex);
		m_sampleChannel.push_back(std::string());
		return true;
	}

	size_t SampleCount() const { return m_rows.size(); }
	size_t ColumnCount() const { return m_columns.size(); }
	const std::vector<std::string>& Columns() const { return m_columns; }
};

} // namespace Grf
