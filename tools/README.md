# tools — 불량 이미지 군집화 분석

`GlimRegionFeatures`(또는 `GlimRegionBatch`)가 생성한 region feature CSV를 읽어
불량 크롭 이미지를 **특징 기반으로 군집화하고 시각화**하는 Python 분석 도구.

## cluster_defects.py

### 목적
- 룰베이스 판정을 **수치화(Score)** 하고 **군집화**할 수 있는지 검증/시각화.
- region feature만으로 룰 판정코드(`ClassifiedCode`)가 얼마나 복원되는지 정량화(ARI).

### 입력
`<ROOT>\*_result.csv` — 각 불량 폴더별 region feature CSV.
(컬럼 예: area, circularity, compactness, anisometry, hu0~1, score_*, ClassifiedCode ...)

### 실행
```bash
python cluster_defects.py [ROOT_DIR]
# ROOT_DIR 생략 시 기본값 D:\128Crop
```
예: `python cluster_defects.py D:\128Crop`

### 출력 (`<ROOT>\_cluster_analysis\`)
| 파일 | 내용 |
|------|------|
| `cluster_scatter.png` | t-SNE 2D 지도 3패널 (군집 / 폴더라벨 / 룰코드) |
| `cluster_thumbnails.png` | 군집별 대표 이미지 썸네일 |
| `cluster_summary.csv` | 파일별 군집 배정 + t-SNE 좌표 |

### 핵심 설계 (정규화 분리)
한 가지 스케일링으로는 시각화와 군집화를 동시에 못 잡는다.
- **시각화(t-SNE)**: `RobustScaler` — 지도가 가장 깨끗하게 분리.
- **군집화(KMeans/DBSCAN)**: 크기특징 `log1p` + `StandardScaler` — 특정 특징(면적 등) 지배 없이 순수 분리.
두 공간을 각각 최적화한 뒤 t-SNE 좌표 위에 군집 색을 겹쳐 그린다.

### 제외 특징
위치/각도(center, bbox, phi, orientation)와 발산값(inner_circle_radius, inner_outer_ratio, hu2~6=FLT_MAX/0)은
불량 종류와 무관하거나 수치가 깨져 자동 제외.

### 의존성
`numpy, opencv-python(cv2), matplotlib, pandas, scipy, scikit-learn`

### 검증 결과 (D:\128Crop, 139장, 2026-07-17)
KMeans k=3, silhouette 0.975, **ARI(code)=1.000** — region feature만으로 룰 판정코드 완벽 복원.
- C0(75) SpecOut=OK(검정 크롭/폭알람), C1(60) NullWrinkle=UNCOATED(코팅경계), C2(4) BlackPoint=PINHOLE.
- SpecOut Coat/Null은 형태특징으로 구분 불가(둘 다 폭 알람) → 폭 수치 특징 필요.
