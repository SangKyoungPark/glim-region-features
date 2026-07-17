# GlimRegionFeatures — 다차원 feature 공간 Blob 군집화 스펙

> 목적: Score/룰 기반 분류(불량코드) 이전 단계에서, 라벨 없이 축적된 Region 특징값을
>       다차원 feature 공간에서 군집화(clustering)하여 "비슷한 모양끼리" 묶어 보고,
>       군집을 사람이 검토해 불량코드를 부여하면 그 즉시 분류 룰(INI) 초안을 뽑아낸다.
> 계산 소스: C++ 엔진(`cv::kmeans`)이 유일한 소스다. 웹(Python)은 계산을 하지 않고
>            PCA 2D 투영으로 시각화만 담당한다(사유는 "1. 설계 원칙" 참조).

## 0. End-to-End 플로우 (원본 이미지 폴더 → 군집 → 프로파일)

가장 흔한 사용 경로는 아직 이진화/CSV 화가 안 된 "원본 불량 크롭 이미지 폴더"에서 시작한다.

```
[원본 이미지 폴더]
     │  GlimRegionBatch.exe --cluster-folder <폴더> clusters.json [이진화 옵션] [군집 옵션]
     │   내부적으로: CsvExporter::ExportFolder(이진화+feature추출) → <clusters.json>.src.csv 로 보존
     ▼                                                              → ClusterCsvIO::LoadFeatureMatrix
[clusters.json]  ◄────────────────────────────────────────────────  ← ClusterEngine::Run (+ 필요 시 KSelector)
     │
     │  (웹: 산점도/K곡선/군집카드로 검토, 사용자가 clusterId→불량코드 라벨링)
     │
     │  GlimRegionBatch.exe --to-profile clusters.json <clusters.json>.src.csv out.ini --labels 0=PINHOLE,1=TEAR
     ▼
[out.ini]  → 사용자 검토/수정 후 Coater.ini 등 실제 프로파일에 반영(ProfileLoader 로 로드)
```

이미 `result.csv` 가 있는 경우(기존 배치 검사 결과 재활용)에는 `--cluster-folder` 대신
`--cluster <result.csv> <clusters.json>` 를 바로 사용하면 된다(이진화/feature 추출 단계 생략).

## 1. 설계 원칙

- **군집 계산 단일 소스 = C++ `ClusterEngine`(`cv::kmeans`)**. 웹(Python)은 군집 계산을 하지
  않고 PCA 2D 투영·시각화만 담당한다. `cv::kmeans` 와 `sklearn.cluster.KMeans` 는 시드를
  맞춰도 부동소수 연산 순서/구현 차이로 결과가 완전히 일치하지 않으므로, 여러 소스가 각자
  계산하면 "웹에서 본 군집"과 "실제 분류 룰"이 어긋날 수 있다 — 단일 소스만이 이를 방지한다.
- DBSCAN/HDBSCAN 등은 웹 쪽 "탐색 전용" 옵션으로 향후 추가할 수 있으나, 분류/프로파일 산출
  경로에는 사용하지 않는다(이번 구현 범위 밖).
- 기존 `result.csv` 스키마는 **무변경**(바이트 동일 계약 보존). 군집 결과는 별도 `clusters.json`
  에 저장한다. `result.csv` 에 `cluster_id` 를 얹고 싶다면 `--cluster-inplace` 옵트인만 사용한다
  (기본 미사용 — 기존 CSV 소비자(웹 `/api/run`, Viewer)에 영향 없음).

## 2. 전처리(FeatureScaler)

| scaleMode | 값 | 공식 |
|---|---|---|
| zscore(기본) | 0 | `(v - mean) / stddev` |
| minmax | 1 | `(v - min) / (max - min)` |
| robust | 2 | `(v - median) / IQR` (IQR = Q3-Q1) |

- 분모가 0에 가까우면(≤1e-12) 1.0 으로 대체(0 나눗셈 가드).
- `logAreaLike=true` 이면 이름에 `area`/`contlength`/`diameter` 를 포함하는 컬럼에 한해
  `log1p` 를 스케일 적용 전에 선적용한다(면적류는 값의 분포가 넓어 왜곡되기 쉬움).
- 스케일 파라미터(`ScalerParams`: `features`,`mean`,`scale`,`log1p`)는 결과에 저장되어
  동일 변환 재적용(재현) 및 centroid 원 스케일 복원(`FeatureScaler::InverseTransform`)에 쓰인다.

### Feature 선택 3모드(웹/CLI 공통 개념)

1. **사용자 선택(기본)** — 스케일 불변 형상계수 프리셋 12종을 권장 기본값으로 제시한다:
   `circularity, compactness, convexity, rectangularity, roundness, anisometry, bulkiness,
   structure_factor, aspect_ratio, fill_ratio, inner_outer_ratio, k_factor`
   (위치성 `row/col`, 각도 `phi/orientation` 은 제외 — 군집 의미가 없음).
2. **전체 스칼라** — `--features` 를 생략하면 `result.csv` 의 식별자/`score_*`/`ClassifiedCode`/
   `*_mm` 파생 컬럼을 제외한 모든 원시 특징값 컬럼을 사용한다(`ClusterCsvIO::LoadFeatureMatrix`).
3. **자동(저분산/상관 프루닝)** — 후속 과제(이번 구현 범위 밖). 현재는 위 두 모드만 지원.

## 3. K 결정(KSelector)

- `K = kMin..kMax` 스윕(기본 2..8) → 각 K 에 대해 `cv::kmeans` 실행 후 실루엣 평균과 관성
  (compactness, `cv::kmeans` 반환값)을 계산한다.
- 실루엣이 최대인 K 를 `recommendedK` 로 추천한다(동률이면 먼저 나온 작은 K).
- 사용자가 K 를 직접 지정(`--k N`)해도 곡선은 참고용으로 함께 계산해 반환한다(웹 표시용).
- 실루엣 계산은 `ClusterEngine::ComputeSilhouette` 하나만 사용한다(`KSelector` 도 동일 함수를
  재사용 — 로직 분기로 인한 스펙 불일치 방지).

## 4. 결정성(Determinism) 규칙

같은 입력(FeatureMatrix + ClusterParams)이면 **항상 동일한 라벨**을 재현해야 한다. 이를 위해:

1. `cv::setRNGSeed(seed)` 고정 + 고정 `attempts`(기본 5) + `cv::KMEANS_PP_CENTERS` +
   고정 `TermCriteria(EPS+COUNT, 100, 1e-4)`.
2. 군집화는 "단일 스레드 집계 단계"에서만 호출한다(`cv::setRNGSeed` 는 OpenCV 전역 RNG 를
   건드리므로). 파일 단위 병렬 검사(`CsvExporter::ExportFolder`)와는 무관 — 그 산출물(CSV)이
   전부 만들어진 뒤 별도 단계로 1회 호출하는 것을 전제로 한다.
3. 입력 샘플 순서 = `FeatureMatrix` 행 순서(= CSV 등장 순서, `AddSample` 호출 순서) 그대로 사용.
4. **정준(canonical) 라벨 재매핑**: `cv::kmeans` 가 반환하는 원시 라벨(순서 임의)을
   "군집 크기 desc, 동률 시 첫 feature 중심(스케일된 좌표) 오름차순, 완전 동률이면 원 인덱스"
   순서로 재정렬해 `cluster_id`(0-based)를 재부여한다. 이로써 같은 입력이면 항상 같은
   `cluster_id` 가 재현된다.
5. `ScalerParams` 를 결과에 기록해 동일 변환을 재적용하거나 원 스케일로 복원할 수 있다.

## 5. 모듈 배치(클린 아키텍처)

```
Domain/  FeatureMatrix.h/.cpp   — N×M 행렬 + 컬럼명 + 샘플키(fileName, regionIndex, channel)
         ClusterResult.h/.cpp   — labels[], centroids(원스케일 복원), K, silhouette, scalerParams,
                                   clusterSizes, ok/error
UseCase/ FeatureScaler.h/.cpp   — 무상태. Fit→ScalerParams, Apply(zscore/minmax/robust, +log옵션)
         ClusterEngine.h/.cpp   — 무상태. Run(FeatureMatrix, ClusterParams) const → ClusterResult
                                   (cv::kmeans + 정준 relabel). ComputeSilhouette 공개 static.
         KSelector.h/.cpp       — 무상태. Sweep(scaled, kMin, kMax) → 실루엣/관성 곡선 + 추천 K
Adapter/ ClusterCsvIO.h/.cpp    — result.csv ↔ FeatureMatrix, ClusterResult ↔ clusters.json,
                                   (옵트인) result.csv 인플레이스 cluster_id 컬럼 추가
Profile/ ProfileWriter.h/.cpp   — ClusterResult + 사용자 라벨 → 프로파일 INI 초안(ProfileLoader 의 역)
```

우산 헤더 `GlimRegionFeatures.h` 에 전부 등록되어 있다. 모든 클래스는 무상태(stateless)이며
`ClusterEngine`/`KSelector` 는 `cv::setRNGSeed` 로 인해 4번 규칙(단일 스레드 집계 단계)을 지켜야
하는 점을 제외하면 재진입 안전하다.

### FeatureMatrix API 계약

```cpp
FeatureMatrix m;
m.SetColumns({"circularity", "convexity"});         // 컬럼(특징 이름) 정의를 먼저
m.AddSample("img001.png", 0, {0.83, 0.91});           // (fileName, regionIndex, values) 순서로 추가
```

- `AddSample` 호출 순서 = `ClusterResult::m_labels` 의 인덱스 순서와 항상 1:1 대응한다
  (엔진 내부에서 행 순서를 재배열하지 않음 — 스케일링/`cv::kmeans`/정준 재라벨링 전 과정에서
  인덱스 순서를 보존한다).
- `values.size()` 가 `SetColumns` 로 정의한 컬럼 수와 다르면 `AddSample` 은 추가하지 않고
  `false` 를 반환한다(방어적 동작 — 호출부가 실수로 열 수를 어긋나게 넣는 것을 방지).

## 6. clusters.json 스키마

```json
{
  "params": {
    "features": ["circularity", "convexity"],
    "scaleMode": 0,
    "logAreaLike": false,
    "k": 3,
    "kMax": 8,
    "seed": 12345,
    "attempts": 5
  },
  "k": 3,
  "silhouette": 0.42,
  "kCurve": {
    "k": [2, 3, 4, 5, 6, 7, 8],
    "silhouette": [0.31, 0.42, 0.38, 0.30, 0.27, 0.25, 0.22],
    "inertia": [120.5, 95.2, 88.1, 80.4, 75.0, 70.2, 66.9],
    "recommendedK": 3
  },
  "scaler": {
    "features": ["circularity", "convexity"],
    "mean": [0.71, 0.85],
    "scale": [0.12, 0.08],
    "log1p": [false, false]
  },
  "clusterSizes": [40, 25, 10],
  "centroids": [
    [0.91, 0.95],
    [0.55, 0.70],
    [0.30, 0.40]
  ],
  "assignments": [
    {"fileName": "img001.png", "regionIndex": 0, "channel": "", "cluster": 0},
    {"fileName": "img002.png", "regionIndex": 1, "channel": "B", "cluster": 2}
  ]
}
```

- `scaleMode` 는 정수(0=zscore, 1=minmax, 2=robust).
- `k`(최상위)/`silhouette`/`clusterSizes`/`centroids` 는 **실제 실행된 K**(자동이면 추천 K,
  지정이면 그 값)의 결과다. `kCurve` 는 스윕 범위 전체의 참고 곡선(K 를 직접 지정해도 가능하면
  함께 채워진다 — 웹에서 "이 K 가 최선인지" 시각적으로 판단하도록).
- `centroids` 는 **원 스케일 복원값**이다(스케일된 좌표가 아니라 사람이 바로 읽을 수 있는 값).
- `assignments` 의 순서 = `FeatureMatrix` 행 순서(= 원본 `result.csv` 순서)와 동일하다.
- 자체 최소 JSON 파서/라이터로 읽고 쓴다(외부 JSON 라이브러리 의존 없음). 비-ASCII(한글 파일명
  등)는 이스케이프 없이 UTF-8 그대로 기록/판독한다.

## 7. Batch CLI

```
GlimRegionBatch.exe --cluster <result.csv> <clusters.json>
    [--features a,b,c] [--k N|auto] [--kmin N] [--kmax M]
    [--scale zscore|minmax|robust] [--seed S] [--attempts N]
    [--log-area] [--cluster-inplace]

GlimRegionBatch.exe --cluster-folder <입력폴더> <clusters.json>
    [기존 --dark/--bright/--thresh/--otsu/--binary/--offset/--wrinkle/--proj 이진화 옵션 전부]
    [--features a,b,c] [--k N|auto] [--kmin N] [--kmax M]
    [--scale zscore|minmax|robust] [--seed S] [--attempts N] [--log-area]
    ; 원샷: 이진화 → feature 추출 → 군집화. 임시 CSV 를 <clusters.json>.src.csv 로 보존(재사용).
    ; 기본 이진화값은 --dark --thresh auto(배경 대비 어두운 불량 크롭 가정, 실전 기본값).

GlimRegionBatch.exe --to-profile <clusters.json> <result.csv> <output.ini>
    --labels clusterId=CODE[,clusterId=CODE...] [--plow N] [--phigh N] [--name NAME]
    ; result.csv 는 clusters.json 을 만들 때 쓴 파일과 동일해야 한다
    ; (--cluster-folder 산출물이면 <clusters.json>.src.csv).
```

예:

```
GlimRegionBatch.exe --cluster-folder D:\128Crop\BlackPoint clusters.json --dark --thresh auto --k auto
GlimRegionBatch.exe --to-profile clusters.json clusters.json.src.csv draft.ini --labels 0=PINHOLE,1=TEAR --plow 5 --phigh 95 --name Coater
```

## 8. 군집 → 불량코드 매핑(ProfileWriter)

사용자가 cluster 에 불량코드 라벨을 부여하면, 멤버들의 feature 별 분위수 envelope(기본
5~95%, 선형보간)로 `[Rule_<CODE>]` `feature=min,max` 조건(AND 결합)을 만들고,
전체 샘플 기준 percentile envelope 로 `[Score]` `vMin,vMax,inc` 초안을 함께 만든다.

- 분위수 계산은 **엔진(ProfileWriter)에 배치**한다(웹/CLI 어느 경로로 호출해도 동일 결과 —
  단일 소스 원칙). 웹은 호출만 한다.
- `[Score]` 의 증감 방향은 항상 `inc` 로 생성된다(엔진은 각 feature 의 "불량 방향" 에 대한
  사전 지식이 없음). 생성된 INI 는 **초안**이며, 사용자가 검토 후 방향/범위를 조정하고 실제
  프로파일(`Coater.ini` 등)에 반영해야 한다 — `--to-profile` 은 파일을 바로 덮어쓰지 않고
  별도 `output.ini` 로만 출력한다.
- 멤버가 없는 cluster 는 해당 `[Rule_<CODE>]` 를 경고 주석만 남기고 빈 채로 둔다.

## 9. 안전/가드

- 빈 `FeatureMatrix`, `K<=0`, `K>샘플수`, 선택한 feature 이름이 CSV/행렬에 하나도 없는 경우
  전부 가드하여 `ClusterResult::m_ok=false` + `m_error` 로 실패를 전달한다(예외를 던지지 않음).
- 0 나눗셈은 스케일 계산(분모 ≤1e-12 → 1.0 대체) 과 percentile 계산(값이 1개면 그 값 그대로
  반환) 양쪽에서 가드한다.
- 모든 파일 I/O(`ClusterCsvIO`, `ProfileWriter`, Batch CLI)는 `try/catch` 로 감싸 `cv::Exception`
  /`std::exception`/미지 예외를 실패 반환으로 변환한다(크래시 대신 에러 메시지).

## 10. 결정성 단위 테스트(GlimRegionFeaturesTest)

- `[17] cluster engine core` — 뚜렷이 분리된 2군집 합성 데이터로: (a) 동일 입력 2회 실행 시
  라벨 완전 일치(결정성), (b) 그룹 내 동일 라벨/그룹 간 다른 라벨, (c) 정준 재라벨링(크기 동률
  시 첫 feature 중심 오름차순)으로 그룹A가 `cluster_id=0`, (d) `K>N` 가드, (e) `KSelector` 가
  실루엣 최대 K(=2)를 정확히 추천하는지 검증한다.
- `[18] cluster-to-profile INI draft` — `ClusterEngine` 실행 결과를 그대로 `ProfileWriter` 에
  넘겨 INI 초안이 `[Profile]/[Score]/[SelectShape]/[Rule_<CODE>]` 섹션을 모두 포함하는지,
  라벨 없이 호출하면 실패(`ok=false`)하는지 검증한다.
