// ImageFeatureExtractor.cpp
#include "UseCase/ImageFeatureExtractor.h"
#include "Adapter/RegionExtractor.h"
#include "UseCase/FeatureCalculator.h"

namespace Grf {

ImageExtractOptions::ImageExtractOptions()
	: m_minArea(5), m_computePriority2(true)
{
	// 흑(어두운) 불량: 자동 임계값 + 어두운 쪽을 Region 으로.
	m_black.m_enabled = true;
	m_black.m_binarize.m_mode = BINMODE_OTSU;
	m_black.m_binarize.m_polarity = POLARITY_DARK;

	// 백(밝은) 불량: 자동 임계값 + 밝은 쪽을 Region 으로.
	m_white.m_enabled = true;
	m_white.m_binarize.m_mode = BINMODE_OTSU;
	m_white.m_binarize.m_polarity = POLARITY_BRIGHT;
}

ImageFeatureExtractor::ImageFeatureExtractor()
{
}

std::vector<DefectSample> ImageFeatureExtractor::ExtractOneChannel(
	const cv::Mat& src, const BinarizeParams& binarize, char polarityTag,
	int minArea, bool computePriority2) const
{
	std::vector<DefectSample> out;
	if (src.empty())
		return out;

	try
	{
		CpuPreprocessor preprocessor(binarize);
		RegionExtractor extractor;
		FeatureCalculator calc;
		calc.SetComputePriority2(computePriority2);

		// 원본 → (극성/모드에 따른) 이진화 → 8-연결 라벨링 → Region 목록.
		const std::vector<Region> regions =
			extractor.ExtractFromGray(src, preprocessor, minArea);

		out.reserve(regions.size());
		for (size_t i = 0; i < regions.size(); ++i)
		{
			DefectSample sample;
			sample.m_channel = polarityTag;
			sample.m_regionIndex = static_cast<int>(i);
			sample.m_region = regions[i];
			sample.m_features = calc.Compute(regions[i]);
			out.push_back(sample);
		}
	}
	catch (const cv::Exception&)
	{
		// 이진화/추출 실패는 빈 결과(방어적). 상위에서 다른 채널은 계속 진행한다.
		out.clear();
	}
	catch (...)
	{
		out.clear();
	}
	return out;
}

std::vector<DefectSample> ImageFeatureExtractor::Extract(
	const cv::Mat& src, const ImageExtractOptions& opt) const
{
	std::vector<DefectSample> out;
	if (src.empty())
		return out;

	// 결정적 순서: 흑 채널 먼저, 백 채널 다음.
	if (opt.m_black.m_enabled)
	{
		const std::vector<DefectSample> black = ExtractOneChannel(
			src, opt.m_black.m_binarize, 'B', opt.m_minArea, opt.m_computePriority2);
		out.insert(out.end(), black.begin(), black.end());
	}
	if (opt.m_white.m_enabled)
	{
		const std::vector<DefectSample> white = ExtractOneChannel(
			src, opt.m_white.m_binarize, 'W', opt.m_minArea, opt.m_computePriority2);
		out.insert(out.end(), white.begin(), white.end());
	}
	return out;
}

} // namespace Grf
