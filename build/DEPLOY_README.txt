========================================
 Glim Region Viewer v1.0.0
 영역(Region) 특징 분석 뷰어 - 사용 안내
========================================

[1] 실행 요구사항
 - Windows 10/11 (x64)
 - Microsoft Visual C++ 2015-2022 재배포 패키지 (x64)
     * 미설치 시 실행 안 됨. "vc_redist.x64.exe" 를 먼저 설치하세요.
 - MFC 런타임(mfc140u.dll 등) : 위 VC++ 재배포 패키지에 포함됨
 - opencv_world4120.dll : 본 폴더에 동봉되어 있습니다(같은 폴더 유지).

[2] 실행 방법
 - GlimRegionViewer.exe 를 더블클릭.
 - 종료 시 설정(폴더/프로파일/이진화 모드·파라미터/창 위치 등)이
   GlimRegionViewer.ini 로 자동 저장되어 다음 실행 때 복원됩니다.

[3] 화면 구성(탭 4개)
 - 홈   : 분석 요약(폴더/파일수/검출 Region 수/분류코드별 카운트)
 - 설정 : 이미지·폴더 열기, 프로파일 선택, 이진화 모드/파라미터, 분석 실행
 - 결과 : Region 카드 리스트 + 상세 이미지(오버레이/줌) + 특징값 리스트
 - 분석 : 분류코드별 카운트/score 평균/특징값 히스토그램 차트

[4] 이진화 모드 안내(불량 유형별 권장)
 - Bright(127) / Dark(127) : 고정 임계값. 밝은/어두운 불량용.
 - Dark auto / Bright auto  : 국소 평균 + offset 자동 임계.
 - Binary                   : 이미 이진화된 입력 그대로 사용.
 - Wrinkle (주름 권장)      : 주름/구김 검출. blackhat 전처리 기반.
                              * 60장 실측 기준 주름은 Wrinkle 모드가 가장 안정적.
 - Projection(검사기 방식)  : 흑(B)/백(W) 2채널. 열평균 프로파일 대비
                              국소 편차로 흑점/이물 검출.
                              * 흑점(Black point)은 Projection 또는 Dark auto 권장.

[5] 결과 내보내기
 - 결과 탭의 [Export CSV] 로 폴더 전체 Region 특징값을 CSV 저장.
 - X/Y Scale(mm/px) 값을 설정하면 mm 파생 컬럼이 함께 계산됩니다.

[6] 프로파일(profiles\*.ini)
 - 공정별 분류 규칙(Coater / RollPress / Slitter). 설정 탭에서 선택.
 - 새 규칙을 추가하려면 profiles 폴더에 ini 를 추가하고 재실행하세요.

문의: GLIM
