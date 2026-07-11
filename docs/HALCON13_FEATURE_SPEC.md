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
### inner_rectangle1 — 최대 축평행 내접 사각형: 히스토그램 스택 방식 O(W·H)

파생 Score: 종횡비(Rect2 장/단변), 충진율(A / Rect2 면적), 내외접비(inner_circle.r / smallest_circle.r)

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

### runlength_features
- 런 개수, K-factor = 런수/sqrt(A) 등 — 줄무늬성 판정 보조. 우선순위 낮음.

## 7. 선택/필터 (라이브러리의 분류 룰 엔진)

### select_shape 재현
- 특징값 이름 + [min, max] 구간 배열 + AND/OR 결합으로 Region 필터링
- 이것이 공정 프로파일의 기반: "불량코드 X = 조건들의 AND"

## 8. Score 설계

- 원시 특징값 → 0~100 Score 정규화:
  - score = 100 · clamp((v − vMin) / (vMax − vMin), 0, 1), 감소형은 100 − score
  - vMin/vMax는 공정 프로파일 INI에서 로드
- FeatureVector: 모든 원시값 + Score를 담는 구조체. CSV 덤프 지원(데이터 수집·튜닝용).

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
