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
//
// ────────────────────────────────────────────────────────────────────────────
// [쓰레드 안전성 보장] (감사 완료 — 가변 static/전역 상태 없음)
//   - 모든 계산기(FeatureCalculator / RegionExtractor / ScoreNormalizer /
//     SelectShapeRule / CsvExporter)는 "무상태(stateless)"이다.
//     계산 함수는 const 메서드(또는 static/free 함수)이며, 모든 작업 상태는
//     지역변수 또는 호출자 소유(입력은 const 참조)로만 존재한다.
//   - 따라서 Region 단위로 여러 쓰레드에서 동시 호출해도 안전하다.
//     예) 스레드마다 자신의 FeatureCalculator(또는 공유 1개)로 서로 다른 Region 을
//         Compute() 해도 경합 없음. RegionExtractor::Extract 도 입력 Mat 이 다르면 안전.
//   - Profile / ProfileLoader / ScoreNormalizer / SelectShapeRule 은 "로드 후 읽기전용"이다.
//     Load()/AddConfig()/AddRule() 등 설정 변경은 초기화(단일 쓰레드) 단계에서만 수행하고,
//     이후에는 여러 쓰레드가 동시에 읽기(Normalize/Classify) 공유해도 안전하다.
//   - 주의: 설정 변경 메서드(SetConnectivity, AddConfig 등)를 계산과 동시에 호출하지 말 것.
//   - cv::Mat 얕은복사: 각 계산은 입력을 읽기만 하고 출력은 새 Mat(지역)로 생성하므로,
//     동일 입력 Mat 을 여러 쓰레드가 읽는 것은 안전(쓰기 없음).
//
// [전처리 GPU 삽입 지점] Adapter/IPreprocessor.h 참조.
//   전처리(원본→이진화)를 IPreprocessor 로 분리했다. 기본은 CpuPreprocessor(threshold).
//   GPU PC에서 대형 원본을 다룰 때 CUDA 구현(cv::cuda 빌드 또는 자체 커널)을 이 인터페이스로
//   삽입하면 나머지 파이프라인은 무수정으로 재사용된다.
// ────────────────────────────────────────────────────────────────────────────

#include "Domain/Region.h"
#include "Domain/FeatureVector.h"
#include "Domain/ScoreResult.h"
#include "Adapter/IPreprocessor.h"
#include "Adapter/RegionExtractor.h"
#include "Adapter/CsvExporter.h"
#include "UseCase/FeatureCalculator.h"
#include "UseCase/ScoreNormalizer.h"
#include "UseCase/SelectShapeRule.h"
#include "Profile/IniFile.h"
#include "Profile/ProfileLoader.h"
