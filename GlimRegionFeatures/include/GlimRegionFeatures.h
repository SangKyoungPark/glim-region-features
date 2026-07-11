#pragma once
// GlimRegionFeatures.h
// 공개 우산 헤더. 이 헤더 하나만 include 하면 라이브러리 전체 API 사용 가능.
//
// 사용 예:
//   #include "GlimRegionFeatures.h"
//   using namespace Grf;
//
//   cv::Mat binary = ...;                       // 8UC1 이진화 이미지
//   RegionExtractor extractor;                  // 8-연결
//   std::vector<Region> regions = extractor.Extract(binary, /*minArea=*/5);
//
//   FeatureCalculator calc;
//   ProfileLoader profile;
//   profile.Load("Coater.ini");
//
//   for (size_t i = 0; i < regions.size(); ++i)
//   {
//       FeatureVector fv = calc.Compute(regions[i]);
//       ScoreResult sc = profile.Normalizer().Normalize(fv);
//       std::string code = profile.RuleEngine().Classify(fv, "OK");
//       // fv.ToCsvRow() 로 데이터 수집 가능
//   }
//
// 좌표계: 위치성 특징값은 Halcon 관례 (row, col). 내부 모멘트는 (x=col, y=row).

#include "Domain/Region.h"
#include "Domain/FeatureVector.h"
#include "Domain/ScoreResult.h"
#include "Adapter/RegionExtractor.h"
#include "UseCase/FeatureCalculator.h"
#include "UseCase/ScoreNormalizer.h"
#include "UseCase/SelectShapeRule.h"
#include "Profile/IniFile.h"
#include "Profile/ProfileLoader.h"
