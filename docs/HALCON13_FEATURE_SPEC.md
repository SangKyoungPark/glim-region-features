# GlimRegionFeatures — Halcon 13 Region Features 재현 스펙

> 원본 스펙: https://www.mvtec.com/doc/halcon/13/en/toc_regions_features.html
> 목적: 불량 Blob(Region)마다 형상 특징값을 계산해 Score로 변환하고,
>       코터/롤프레스/슬리터 3개 공정의 불량 분류에 사용한다.
> 구현: OpenCV(cv::Mat) + 직접 구현. Halcon SDK는 사용하지 않는다 (라이선스 회피).

## 0. 공통 정의

- **Region**: 이진화 이미지에서 추출한 하나의 연결 성분(connected component).
  - 내부 표현: 런렝스(run-length) 인코딩 + 외곽 컨투어(std::vector<cv::Point>) + 픽셀 수
  - 좌표계: Halcon과 동일하게 (row, col). OpenCV (x=col, y=row) 변환에 주의.
- **A**: Region 면적 (픽셀 수)
- **L**: 외곽 컨투어 길이 (contlength)
- **(r̄, c̄)**: 무게중심 (area_center)
- 연결성: 8-연결 기본

## 1. 크기/기본 (우선순위 1)

### area_center
- A = 픽셀 수, 중심 = 픽셀 좌표 평균 (r̄, c̄)

### area_holes
- 구멍 면적 = fill_up(Region) 면적 − A
- 구현: 컨투어 내부 채운 면적(cv::drawContours FILLED) − A

### contlength
- 외곽 컨투어 길이. 인접 픽셀 간 거리: 수평/수직 = 1, 대각 = √2
- 구현: cv::arcLength(contour, true) 또는 체인코드 직접 합산 (Halcon 방식은 체인코드)

### diameter_region
- 경계점 쌍 사이 최대 거리 + 해당 두 점 좌표
- 구현: convex hull 위에서 rotating calipers (O(n))

## 2. 형상 계수 (우선순위 1 — 분류 핵심)

### circularity
- C = A / (π · max²), max = 중심에서 컨투어 점까지의 **최대** 거리
- 범위 0~1, 원 = 1. 길쭉하거나 뾰족하면 급격히 감소.

### compactness
- C = L² / (4πA)
- 원 = 1, 복잡한 윤곽일수록 커짐. 상한 없음.

### convexity
- C = A / A_convex (볼록껍질 면적 대비 원면적)
- 범위 0~1. 오목한 패임/가지가 있으면 감소. Tear(찢김) 판별에 유효.

### rectangularity
- C = A / A_rect, A_rect = 최적 적합 직사각형 면적
- 구현: cv::minAreaRect 면적으로 근사. 범위 0~1, 직사각형 = 1.

### roundness (컨투어 기반)
- 컨투어 점들의 중심 거리 d_i에 대해:
  - Distance = mean(d_i)
  - Sigma = sqrt(mean((d_i − Distance)²))
  - Roundness = 1 − Sigma / Distance
  - Sides(다각형 변 수 추정) = 1.4111 · (Distance/Sigma)^0.4724
- circularity와 달리 "윤곽의 매끄러움" 척도 → Burr(버) 검출에 핵심.

## 3. 타원 근사 (우선순위 1 — 방향성 불량)

### elliptic_axis
- 중심 2차 모멘트 μ20, μ02, μ11 (픽셀수로 정규화)로부터:
  - Ra = sqrt( 2·(μ20+μ02+sqrt((μ20−μ02)²+4μ11²)) )  ← 장반경 (½ 계수 포함식 확인)
  - Rb = sqrt( 2·(μ20+μ02−sqrt((μ20−μ02)²+4μ11²)) )
  - Phi = 0.5 · atan2(2μ11, μ20−μ02) ← Halcon은 −atan2(2μ11, μ02−μ20)/2 부호 관례, 좌표계 변환 검증 필수
- 단위 테스트: 합성 타원 이미지(장축 100, 단축 40, 30°)로 Ra/Rb/Phi 오차 < 2% 확인

### eccentricity
- Anisometry = Ra / Rb (길쭉함, ≥1)
- Bulkiness = π · Ra · Rb / A
- StructureFactor = Anisometry · Bulkiness − 1
- 세로줄 스크래치(MD 결함)는 Anisometry가 매우 큼.

### orientation_region
- elliptic_axis의 Phi에, 중심에서 가장 먼 컨투어 점 방향으로 부호/방향 결정. 범위 (−π, π]

## 4. 외접/내접 도형 (우선순위 2)

### smallest_rectangle1 — 축평행 바운딩박스 (cv::boundingRect)
### smallest_rectangle2 — 최소면적 회전 사각형 (cv::minAreaRect)
### smallest_circle — 최소 외접원 (cv::minEnclosingCircle)
### inner_circle — 최대 내접원: 거리변환(cv::distanceTransform, DIST_L2) 최대값 위치/반경
### inner_rectangle1 — 최대 축평행 내접 사각형: 히스토그램 스택 방식 O(W·H) [구현완료]
- Region 마스크(ToMask, 로컬 ROI) 기준. 각 행마다 열별 높이 히스토그램을 갱신하고,
  스택 기반 "히스토그램 최대 직사각형(largest rectangle in histogram)"으로 최대 내접 사각형을 찾는다.
- 출력: (row1,col1,row2,col2) — 다른 위치성 값과 동일한 Halcon (row,col) inclusive 좌표. offset(bbox 좌상단) 가산해 절대좌표로 저장.
- 파생값: `inner_rect_fill_ratio` = 내접사각형 면적 / 바운딩박스 면적 (0~1, 완전 사각형=1). rectangularity(회전 허용)와 달리 축평행 충실도.
- 검증: 정사각형 → 내접사각형 = 자기 자신(오차 ≤1px), fill_ratio ≈ 1. L자 → 큰 팔(수기 계산 4551) 일치.

파생 Score: 종횡비(Rect2 장/단변), 충진율(A / Rect2 면적), 내외접비(inner_circle.r / smallest_circle.r), inner_rect_fill_ratio(축평행 내접사각형 충실도)

## 5. 모멘트 (우선순위 2 — 분류기 입력)

### moments_region_2nd / moments_region_central
- 원점/중심 2차 모멘트, 주축 관성 Ia, Ib

### moments_region_2nd_invar / 3rd_invar
- 크기·회전 불변 모멘트 (Hu 모멘트로 대체 가능 — cv::HuMoments 7종)
- Score로 직접 쓰기보다 벡터로 분류기(향후 AI 분류 연동)에 전달.

## 6. 위상 (우선순위 2)

### connect_and_holes / euler_number
- 연결 성분 수, 구멍 수, Euler = 성분 − 구멍
- 기포 군집(구멍 다수) vs 단일 핀홀 구분.

### runlength_features [구현완료]
- 런 = 한 행(row)에서 연속된 전경 픽셀 구간. Region 은 이미 런렝스로 보관하므로 `region.Runs()` 를 직접 집계(O(런수)).
- 출력값(Halcon13 정의 재현):
  - `num_runs` = 런 개수(NumRuns)
  - `k_factor` = NumRuns / sqrt(Area) — 줄무늬/복잡도 계수(런이 많고 면적이 작을수록 큼)
  - `l_factor` = NumRuns / **바운딩박스 높이(Row2−Row1+1)**
    - 근거: Halcon 문서상 LFactor 는 "런 수 ÷ region 높이(=행 방향 확장, Row2−Row1+1)". "런이 존재하는 행 수"가 아니라 바운딩박스 높이를 분모로 사용한다.
      (단일 연결 성분은 통상 span 내 모든 행에 런이 있어 두 정의가 일치하나, 정의상 바운딩박스 높이를 채택)
  - `mean_run_length` = Area / NumRuns — 평균 런 길이(MeanLength)
  - **Bytes(메모리 추정치)는 생략** — Halcon 내부 런렝스 표현의 바이트 추정치로, 자체 구현의 메모리 레이아웃이 달라 재현 의미가 낮다.
- 검증: 정사각형(변 s) → NumRuns=s, KFactor=s/sqrt(s²)=1, MeanLength=s, LFactor=1. H자 → 다중런 행 존재(NumRuns>높이), 마스크 직접 계수와 일치.

## 7. 선택/필터 (라이브러리의 분류 룰 엔진)

### select_shape 재현
- 특징값 이름 + [min, max] 구간 배열 + AND/OR 결합으로 Region 필터링
- 이것이 공정 프로파일의 기반: "불량코드 X = 조건들의 AND"

## 8. Score 설계

- 원시 특징값 → 0~100 Score 정규화:
  - score = 100 · clamp((v − vMin) / (vMax − vMin), 0, 1), 감소형은 100 − score
  - vMin/vMax는 공정 프로파일 INI에서 로드
- FeatureVector: 모든 원시값 + Score를 담는 구조체. CSV 덤프 지원(데이터 수집·튜닝용).

### 8.1 전 특징값 기본 스코어 테이블 (profile-less 기본값) [구현완료]

목표: Halcon 처럼 "불량 이미지를 넣으면 Blob 마다 **모든** 특징값의 Score 가 전부 나온다".
`ScoreNormalizer::SeedDefaults()` / `DefaultConfigs()` 가 아래 고정 순서 테이블을 내장한다.

- 동작:
  1. 프로파일이 없어도(또는 [Score] 가 비어도) 기본 테이블로 전 스칼라 특징값 Score 를 항상 계산한다.
  2. 프로파일 INI [Score] 에 같은 이름이 있으면 **INI 가 기본값을 제자리 교체(UpsertConfig)** → 컬럼 순서 유지, INI 우선(하위 호환).
  3. CSV 의 `score_*` 컬럼이 전 스칼라 특징값으로 확장(테이블 정의 순서로 고정 = 결정적). `ClassifiedCode` 는 룰 엔진(프로파일) 있을 때만.
- 제외 대상: 위치성(center_row/col, 각종 좌표), 각도(phi, orientation) — Score 의미가 없어 제외.

| # | feature | vMin | vMax | dir | 비고 |
|---|---|---|---|---|---|
| 1 | circularity | 0 | 1 | inc | 비율형 |
| 2 | compactness | 1 | 5 | inc | 원=1, 복잡할수록↑ |
| 3 | convexity | 0 | 1 | inc | 비율형 |
| 4 | rectangularity | 0 | 1 | inc | 비율형 |
| 5 | roundness | 0 | 1 | inc | 비율형 |
| 6 | sides | 0 | 20 | inc | 변 수 추정 |
| 7 | anisometry | 1 | 5 | inc | 길쭉함 |
| 8 | bulkiness | 0 | 2 | inc | |
| 9 | structure_factor | 0 | 5 | inc | |
| 10 | ra | 5 | 500 | inc | 반장축(px) |
| 11 | rb | 5 | 500 | inc | 반단축(px) |
| 12 | area | 10 | 5000 | inc | 공정 가설(9장) |
| 13 | contlength | 10 | 1000 | inc | |
| 14 | diameter | 5 | 500 | inc | |
| 15 | area_holes | 0 | 1000 | inc | |
| 16 | holes | 0 | 10 | inc | |
| 17 | euler_number | −10 | 1 | inc | |
| 18 | aspect_ratio | 1 | 10 | inc | |
| 19 | fill_ratio | 0 | 1 | inc | 비율형 |
| 20 | inner_outer_ratio | 0 | 1 | inc | 비율형 |
| 21 | inner_rect_fill_ratio | 0 | 1 | inc | 축평행 내접 충실도 |
| 22 | num_runs | 1 | 1000 | inc | |
| 23 | k_factor | 0 | 5 | inc | |
| 24 | l_factor | 0 | 5 | inc | |
| 25 | mean_run_length | 1 | 500 | inc | |

> 무한 스케일형(area/contlength/diameter/ra/rb/mean_run_length 등) 범위는 9장 공정 가설 기반 **보수적 초기값**이며 현장 데이터로 튜닝 대상. 비율형(0~1)은 vMin=0, vMax=1.

## 9. 공정 프로파일 (초기 가설 — 현장 데이터로 튜닝)

| 공정 | 대표 불량 | 주력 특징값 |
|---|---|---|
| 코터 | Pinhole, 미도포, 기포, Tear/Island | circularity↑, area, convexity↓(Tear), euler(기포군집) |
| 롤프레스 | 주름, 크랙, 이물 압입 | Anisometry↑(주름/크랙), orientation, rectangularity |
| 슬리터 | Burr, 절단면 불량 | roundness↓(Burr), contlength/diameter, Sides |

## 10. 코딩 규칙 (프로젝트 공통)

- C++14/17, VS2015/2022 겸용, x64+x86, MultiByte
- 클래스 PascalCase / 함수 PascalCase / 변수 camelCase / 멤버 m_ 접두사 / 탭 들여쓰기
- 0 나눗셈·빈 컨투어·빈 Region 등 위험 연산은 전부 try-catch + 가드
- 실시간 검사 삽입 전제: 계산기 내부에서 cv::Mat 재할당 최소화, 입력은 const 참조
- 빌드는 수행하지 않는다 (사용자가 VS에서 직접 빌드)

## 11. Phase 3 — 추가 오퍼레이터 16종 (Halcon13 Regions>Features 재현)

> 원본 문서 각 오퍼레이터 페이지의 반환값 정의를 기준으로 재현.
> 배열 반환형은 API(벡터)로 제공하고, FeatureVector/CSV 에는 **스칼라 요약**만 추가한다(컬럼 순서 결정적, 기존 컬럼 뒤에 append).
> 좌표계: 입출력 위치 인자는 Halcon (row, col). 모멘트는 Halcon 관례(첫 인덱스=row)로 재정의(내부 elliptic_axis 의 x=col,y=row 계산과 별개 필드).

### 11.A 단일 Region 특징값 (FeatureCalculator 확장, 우선순위 2 블록)

#### get_region_thickness
- 정의: 주축(장축)을 x' 로 회전한 좌표계에서 x' 를 1px 구간(bin)으로 나누고, 각 bin 의 수직(y') 방향 두께 = (max y' − min y' + 1) 을 계산한 **프로파일**.
- 주축각 φ 는 중심 2차 모멘트로 산출(elliptic_axis 와 동일): `φ = 0.5·atan2(2μ11, μ20−μ02)`, 주축 `u=(cosφ,sinφ)`, 수직 `v=(−sinφ,cosφ)`.
- API: `FeatureCalculator::ComputeThicknessProfile(region, lengthOut) → std::vector<double>` (프로파일, lengthOut=주축 길이=bin 수).
- CSV 스칼라 요약: `thickness_mean`(빈 구간 제외 평균), `thickness_max`, `thickness_length`(주축 길이).
- 검증: 120(col)×60(row) 직사각형 → 주축=col, 두께 프로파일 ≈ 60 일정 → mean/max ≈ 60, length ≈ 120.

#### runlength_distribution
- 정의: 런 길이별 히스토그램. Region 은 런렝스 보관(`region.Runs()`)하므로 O(런수) 집계.
- API: `FeatureCalculator::ComputeRunlengthDistribution(region) → std::vector<int>` (index=런 길이, value=개수, index 0 미사용).
- CSV 스칼라 요약: `run_len_min`, `run_len_max`, `run_len_mode`(최빈 길이). (평균은 기존 `mean_run_length` 재사용)
- 검증: 정사각형(side s) → 분포[s]=s, min=max=mode=s.

#### moments_region_2nd / moments_region_central / moments_region_3rd
- 원시 모멘트 `m_pq = Σ row^p · col^q` 를 런렝스에서 Faulhaber 부분합으로 O(런수) 누적.
- 중심(centroid) `r̄=m10/A, c̄=m01/A`.
- **중심 2차(비정규화)**: `Mu20=Σ(r−r̄)²`, `Mu02=Σ(c−c̄)²`, `Mu11=Σ(r−r̄)(c−c̄)` → CSV `mc_mu20, mc_mu02, mc_mu11` (**moments_region_central**).
- **중심 2차(정규화 /A)**: `M20=Mu20/A`(row 분산), `M02=Mu02/A`(col 분산), `M11=Mu11/A`. 주축 관성 `Ia,Ib` = 공분산행렬 `[[M20,M11],[M11,M02]]` 의 고유값(Ia=대, Ib=소) → CSV `m2nd_m20, m2nd_m02, m2nd_m11, m2nd_ia, m2nd_ib` (**moments_region_2nd**).
- **중심 3차(정규화 /A)**: `Mu30, Mu03, Mu21, Mu12` 표준 전개 후 `/A` → CSV `m3rd_m30, m3rd_m03, m3rd_m21, m3rd_m12` (**moments_region_3rd**).
  - `Mu30=m30−3r̄·m20+2A·r̄³`, `Mu03=m03−3c̄·m02+2A·c̄³`, `Mu21=m21−2r̄·m11−c̄·m20+2A·r̄²c̄`, `Mu12=m12−2c̄·m11−r̄·m02+2A·c̄²r̄`.
- 검증: 120×60 직사각형 → M20=(60²−1)/12≈299.92, M02=(120²−1)/12≈1199.92, M11≈0, Ia=M02, Ib=M20. 대칭이라 3차≈0.

#### moments_region_2nd_rel_invar (상대 불변 PHI1, PHI2)
- 스케일 정규화 `η_pq = Mu_pq / A²` (2차). 회전 불변량:
  - `PHI1 = η20 + η02`
  - `PHI2 = (η20 − η02)² + 4·η11²`
- CSV: `moment_phi1, moment_phi2`.
- 검증: 원판 → `PHI1 = 1/(2π) ≈ 0.15915` (스케일·회전 불변).

#### moments_region_central_invar (중심 불변 PSI1~PSI4)
- 스케일 불변 `η` 기반(Hu 와 별개, 본 재현 정의 — 문서 명시):
  - `PSI1 = η20`, `PSI2 = η11`, `PSI3 = η02`, `PSI4 = η20·η02 − η11²`(스케일 불변 판별식)
- CSV: `moment_psi1..moment_psi4`.

### 11.B Region 간 관계/질의 (신규 모듈 `UseCase/RegionRelation.{h,cpp}`)

무상태 static 함수 집합. 입력 const 참조, 재진입 안전.

| 오퍼레이터 | 함수 | 정의/반환 |
|---|---|---|
| find_neighbors | `FindNeighbors(regions1, regions2, maxDistance)` | 각 regions1[i] 에 대해 경계 최소거리 ≤ maxDistance 인 regions2 인덱스. bbox 간극 하한으로 빠른 배제. maxDistance≤0 이면 픽셀 교집합>0(접촉/겹침)만 이웃 |
| hamming_distance | `HammingDistance(r1,r2, Distance, Similarity)` | `Distance=A1+A2−2·|R1∩R2|`(대칭차), `Similarity=1−Distance/(A1+A2)` |
| hamming_distance_norm | `HammingDistanceNorm(r1,r2, normFactor, DistanceNorm, Similarity)` | `DistanceNorm=Distance/normFactor`(normFactor≤0 이면 원값), Similarity 동일 |
| get_region_index | `GetRegionIndex(regions, row, col)` | (row,col) 픽셀을 포함하는 모든 Region 인덱스 (bbox 배제→런 검사) |
| select_region_point | `SelectRegionPoint(regions, row, col)` | 픽셀 포함 Region 선택(= get_region_index) |
| select_region_spatial | `SelectRegionSpatial(regions, reference, dir)` | 무게중심 기준 방향(LEFT/RIGHT/ABOVE/BELOW) 관계로 선택 |
| spatial_relation | `SpatialRelation(r1, r2, percent)` | bbox 축 비교로 관계 문자열 집합 {left,right,above,below,over}. percent=경계 완화(폭/높이 대비) |
| select_shape_proto | `SelectShapeProto(regions, prototype, feature, min, max)` | prototype 과의 feature(distance_center/distance_contour/overlap/overlap_abs) 값이 [min,max] 인 Region 선택 |
| select_shape_std | `SelectShapeStd(regions, shape, percent)` | max_area(면적≥max·(1−p)), rectangle1(area/bboxArea≥1−p), rectangle2(area/minAreaRect≥1−p) |

- 교집합 픽셀 수: 두 bbox 겹침 ROI 로컬 마스크 AND 로 안전 계수(먼 Region 은 즉시 0).
- 컨투어 최소거리: 교집합>0 이면 0, 아니면 외곽 컨투어 점쌍 최소 유클리드 거리.
- 검증: 겹치는 60×60 두 사각형(겹침 30×60=1800) → Distance=3600, Similarity=0.5. 동일 Region → Distance=0, Similarity=1. 3-blob 이미지로 get_region_index/select_shape_std/spatial_relation/find_neighbors 확인.

### 11.C Score 테이블 갱신 (8.1 확장)

기본 스코어 테이블(`DefaultConfigs`)에 추가(위치성·각도 제외 원칙 유지, 모멘트 raw 값은 분류기 입력이므로 Score 제외, 불변량·크기성만 추가):

| feature | vMin | vMax | dir | 비고 |
|---|---|---|---|---|
| thickness_mean | 1 | 500 | inc | 주축 두께 평균(px) |
| thickness_max | 1 | 500 | inc | 주축 두께 최대(px) |
| run_len_max | 1 | 500 | inc | 최대 런 길이 |
| moment_phi1 | 0 | 1 | inc | 회전 불변 |
| moment_phi2 | 0 | 0.5 | inc | 회전 불변 |
| moment_psi1 | 0 | 0.5 | inc | 스케일 불변 |
| moment_psi2 | 0 | 0.5 | inc | 스케일 불변 |
| moment_psi3 | 0 | 0.5 | inc | 스케일 불변 |
| moment_psi4 | 0 | 0.1 | inc | 스케일 불변 판별식 |

> 모멘트 불변량 범위는 보수적 초기값이며 현장 데이터로 튜닝 대상. raw 모멘트(m2nd_*, mc_*, m3rd_*)와 런 min/mode, thickness_length 는 CSV 원시 컬럼으로만 출력(Score 미부여).
