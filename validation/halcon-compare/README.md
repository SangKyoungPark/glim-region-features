# 할콘 vs 우리 엔진 수치 비교 검증

우리 `GlimRegionFeatures` 엔진이 산출하는 Region Features가 **실제 HALCON**과
얼마나 일치하는지 같은 이진 마스크로 검증한다. HALCON은 이 PC에 없으므로,
**HALCON 데모/평가판이 있는 PC에서 HDevelop 프로그램을 실행**해 `halcon.csv`를
만든 뒤, 우리 CSV와 비교표를 생성한다.

## 핵심 아이디어
- **동일 이진 마스크**(`masks/*.png`, 8bit 0/255)를 두 엔진이 똑같이 본다 → 이진화 차이 배제, **순수 feature 계산만 비교**.
- 합성 도형(원/타원/사각/도넛)은 **해석적 정답값**(`analytic_ref.json`)을 알고 있어 두 엔진 모두 정답 대비 평가 가능.

## 파일
| 파일 | 역할 |
|------|------|
| `gen_masks.py` | 이진 마스크 + `analytic_ref.json` 생성 (이미 실행됨) |
| `masks/*.png` | 동일 입력 마스크 (원/타원/사각/도넛) |
| `ours.csv` | **우리 엔진** 결과 (`GlimRegionBatch --binary` 로 생성됨) |
| `halcon_compare.hdev.txt` | **HDevelop 프로그램** — 같은 마스크로 HALCON feature 계산 → `halcon.csv` |
| `compare.py` | 우리 vs 할콘 vs 정답 비교표 (콘솔 + `comparison.csv` + `comparison.png`) |

## 실행 순서
1. **(선택) 마스크 재생성**: `python gen_masks.py`
2. **(선택) 우리 CSV 재생성**:
   `GlimRegionBatch.exe masks ours.csv --binary`
3. **HALCON 실행** (데모/평가판 PC):
   - HDevelop 실행 → 새 프로그램에 `halcon_compare.hdev.txt` 전체 붙여넣기
   - `MaskDir` / `CsvPath` 경로 확인 (이 폴더 기준 절대경로로 수정)
   - `masks/` 폴더를 그 PC로 복사(또는 공유 경로) 후 F5 실행 → `halcon.csv` 생성
4. **비교표 생성**: `halcon.csv`를 이 폴더에 두고
   `python compare.py`
   → 콘솔 표 + `comparison.csv`(롱포맷) + `comparison.png`(오차% 색상표)

## 읽는 법
- 오차% 기준: `halcon.csv`가 있으면 **할콘 대비 우리 오차**, 없으면 **정답 대비 우리 오차**(드라이런).
- 각도(`phi`,`orientation`)는 규약 차이(HALCON은 반시계 등)가 있어 **참고 표기만** — 값 자체보다 축 방향 일치 여부로 판단.

## 알려진 편차(정상)
- `contlength`(둘레) ~5%, `compactness` ~10% 과대: **디지털 격자에서 원 둘레 과대추정**이 원리적 특성. HALCON도 유사 경향. 면적/원형도/타원축/이방성 등 핵심 형상값은 오차 2% 이내.

## 비교 대상 feature
area, circularity, convexity, rectangularity, compactness, roundness, sides,
ra, rb, anisometry, bulkiness, structure_factor, contlength, diameter,
connect_components, holes, euler_number (+ phi/orientation 참고).
좌표·모멘트·Hu 등은 HALCON 직접 오퍼레이터 대응이 1:1이 아니라 이번 표에서 제외(필요 시 확장).
