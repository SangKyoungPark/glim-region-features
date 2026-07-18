// FeatureMatrix.cpp
#include "Domain/FeatureMatrix.h"

namespace Grf {

int FeatureMatrix::ColumnIndex(const std::string& name) const
{
	for (size_t i = 0; i < m_columns.size(); ++i)
		if (m_columns[i] == name)
			return static_cast<int>(i);
	return -1;
}

} // namespace Grf
