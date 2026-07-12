# GlimRegionViewer 빌드 우회 (VS 18 툴셋 MFC 결손 PC 전용)

> 이 문서는 특정 개발 PC(VS 18 Professional)의 툴셋 결손 상태를 우회하는 방법이다.
> 정상적인 VS2022(v143 완전 설치) 환경에서는 필요 없다 — 솔루션 빌드만 하면 된다.

## 증상
- `GlimRegionViewer` 빌드 시 `MSB8041: 이 프로젝트에는 MFC 라이브러리가 필요합니다`
- `VCToolsVersion=14.40` 고정 시 `TRK0005: CL.exe을(를) 찾지 못했습니다`

## 원인 (2026-07-11 실측)
VS 18 Professional의 MSVC 툴셋이 쪼개져 설치됨:

| 툴셋 | cl.exe | MFC (mfc140u/mfc140) |
|---|---|---|
| 14.40.33807 | ❌ | ✅ (유니코드+MBCS) |
| 14.44.35207 | ✅ | ❌ |
| 14.50.35717 | ✅ | ❌ |

v143 요청 시 MSBuild가 14.44를 선택 → 컴파일러는 있으나 MFC가 없어 MSB8041.

## 우회 방법: 14.44 컴파일러 + 14.40 MFC 조합
같은 v143(14.4x) 계열이라 바이너리 호환. `build\mfc1440.props`가 14.40의 atlmfc
include/lib 경로와 `_AFXDLL` 정의를 주입하고, `UseOfMfc=false`로 MSB8041 검사를 건너뛴다.

```
MSBuild GlimRegionFeatures.sln /t:GlimRegionViewer ^
  /p:Configuration=Release /p:Platform=x64 ^
  /p:UseOfMfc=false ^
  /p:ForceImportBeforeCppTargets=%CD%\build\mfc1440.props ^
  /m
```

- props 내 `MfcRoot` 경로는 이 PC 기준 절대경로 — 다른 PC면 수정 필요.
- 프로젝트 파일(vcxproj)은 정상 환경 기준 그대로 유지한다 (UseOfMfc=Dynamic).

## 근본 해결 (우회 불필요하게 만들기)
Visual Studio Installer → 개별 구성 요소에서 다음 중 하나:
- **"MSVC v143 - VS 2022 C++ x64/x86 빌드 도구(v14.40)"** 설치 → 14.40이 완전해짐
  → `/p:VCToolsVersion=14.40.33807`만으로 빌드 가능
- 또는 14.44용 MFC가 설치 목록에 있으면 그것을 설치
