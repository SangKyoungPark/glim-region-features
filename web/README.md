# GlimRegion 웹 분석 대시보드

로컬 브라우저에서 128 크롭 검사 결과를 시각화하는 도구.
"어느 불량이 많이 나왔는지" + "각 Region이 어떤 Score가 높아서 어떤 불량으로 분류됐는지"를
한눈에 본다. 계산은 기존 `GlimRegionBatch.exe`(C++)를 재사용하고, 웹은 표시만 한다.

## 구조
브라우저 → 로컬 FastAPI 서버(`main.py`) → `GlimRegionBatch.exe` 실행 → CSV + 오버레이 PNG 파싱 → 웹 UI

## 사전 조건
- Python 3 (PATH 등록). 이 PC 는 Python 3.13 확인됨.
- `GlimRegionBatch.exe` 가 `bin/x64/Release`(또는 Debug)에 빌드되어 있을 것.
- OpenCV DLL: `C:\Lib\opencv\build\x64\vc16\bin` (config.py `OPENCV_BIN`). exe 실행 시 PATH 에 자동 추가됨.

## 실행
```
web\run.bat
```
- 최초 실행 시 `.venv` 생성 + `requirements.txt`(fastapi, uvicorn) 설치 후 서버 기동.
- 브라우저에서 http://localhost:8080 자동 오픈.

수동 실행:
```
cd web
.venv\Scripts\python -m uvicorn main:app --host 127.0.0.1 --port 8080
```

## 사용법
1. 상단 실행 바에 **폴더 경로**(서버 로컬 경로, 예: `D:/128Crop/BlackPoint`) 입력.
2. **프로파일**(코터/롤프레스/슬리터/없음) + **이진화**(Bright127/Dark127/Dark auto/Bright auto) 선택.
   - 흑점 등 어두운 불량 그레이 원본은 **Dark auto** 권장.
3. **분석 실행** → 요약 카드 / 불량 분포 / Score 분석 / 갤러리 표시.
4. 갤러리: Region 1개 = 카드 행(원본+오버레이 이미지, 핵심 특징값, score 막대, 분류 뱃지).
   행 클릭 시 상세 패널(전체 특징값).

## API
- `POST /api/run` `{folderPath, profile, binarize, threads}` → 분석 JSON
- `GET /api/image?path=<abs>` → 원본/오버레이 이미지 (실행 폴더 + cache 내부만 허용)

## 참고
- 결과는 `web/cache/<해시>/`(result.csv + overlay/*.png)에 캐시됨(gitignore).
- 폴더는 **업로드가 아니라** 서버 로컬 경로 텍스트 입력(같은 PC 전제).
- 차트는 외부 CDN 없이 SVG 로 직접 그린다(오프라인 환경).
