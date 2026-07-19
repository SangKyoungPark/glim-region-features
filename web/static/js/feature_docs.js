// feature_docs.js
// 각 Halcon feature 키 -> {label, desc, hint}. UI 툴팁(title)용.
var FEATURE_DOCS = {
  "area": {
    "label": "면적",
    "desc": "영역(Region)의 픽셀 수",
    "hint": "클수록 큰 불량"
  },
  "area_holes": {
    "label": "구멍 면적",
    "desc": "영역 내부 빈 구멍(hole)들의 픽셀 수 합",
    "hint": "0이면 구멍 없음"
  },
  "contlength": {
    "label": "둘레",
    "desc": "외곽 컨투어(테두리) 길이",
    "hint": "복잡/길쭉할수록 큼"
  },
  "diameter": {
    "label": "최대 지름",
    "desc": "영역에서 가장 멀리 떨어진 두 점 사이 거리",
    "hint": "가장 긴 폭"
  },
  "circularity": {
    "label": "원형도",
    "desc": "면적 대비 둘레로 계산한 원에 가까운 정도 (0~1)",
    "hint": "1=완전한 원, 낮을수록 찌그러짐/길쭉"
  },
  "compactness": {
    "label": "컴팩트니스",
    "desc": "둘레²/(4π·면적). 원=1, 경계가 복잡·길쭉할수록 증가",
    "hint": "1에 가까울수록 원형, 클수록 복잡"
  },
  "convexity": {
    "label": "볼록도",
    "desc": "볼록껍질(convex hull) 대비 실제 면적 비 (0~1)",
    "hint": "1=완전 볼록, 낮으면 오목/톱니(Burr 의심)"
  },
  "rectangularity": {
    "label": "사각도",
    "desc": "최소 사각형 대비 채워진 면적 비 (0~1)",
    "hint": "1=꽉 찬 사각형에 가까움"
  },
  "roundness": {
    "label": "진원도",
    "desc": "1 − (중심~테두리 거리 표준편차/평균). 테두리가 얼마나 고른지 (0~1)",
    "hint": "1=매끈한 원, 낮으면 울퉁불퉁"
  },
  "roundness_distance": {
    "label": "중심-테두리 평균거리",
    "desc": "무게중심에서 외곽까지의 평균 거리",
    "hint": "영역 반경 규모"
  },
  "roundness_sigma": {
    "label": "중심-테두리 거리 편차",
    "desc": "그 거리의 표준편차(들쭉날쭉한 정도)",
    "hint": "클수록 테두리 불규칙"
  },
  "sides": {
    "label": "변 수 추정",
    "desc": "테두리 주기성으로 추정한 변(꼭짓점) 개수",
    "hint": "각진 형상 판단"
  },
  "ra": {
    "label": "등가 타원 장반경",
    "desc": "영역과 같은 2차 모멘트를 갖는 타원의 긴 반지름",
    "hint": "길이 방향 크기"
  },
  "rb": {
    "label": "등가 타원 단반경",
    "desc": "그 타원의 짧은 반지름",
    "hint": "폭 방향 크기"
  },
  "phi": {
    "label": "타원 방향(rad)",
    "desc": "등가 타원 장축이 향한 각도(라디안)",
    "hint": "기울기"
  },
  "orientation": {
    "label": "영역 방향(rad)",
    "desc": "영역 주축이 향한 각도(라디안, -π~π]",
    "hint": "방향성 결함(주름/크랙)"
  },
  "anisometry": {
    "label": "이방성",
    "desc": "장반경/단반경(Ra/Rb) 비",
    "hint": "1=등방(원), 클수록 길쭉(주름/스크래치)"
  },
  "bulkiness": {
    "label": "벌키니스",
    "desc": "π·Ra·Rb/면적",
    "hint": "1 근처=타원꼴로 꽉 참, 크면 속 빈/퍼짐"
  },
  "structure_factor": {
    "label": "구조 계수",
    "desc": "이방성·벌키니스 − 1",
    "hint": "형상 복잡도 종합"
  },
  "aspect_ratio": {
    "label": "종횡비",
    "desc": "최소 회전 사각형의 장변/단변 비",
    "hint": "클수록 가늘고 김"
  },
  "fill_ratio": {
    "label": "채움비",
    "desc": "면적/최소 회전 사각형 면적 (0~1)",
    "hint": "1=사각형을 꽉 채움"
  },
  "inner_outer_ratio": {
    "label": "내/외접원 비",
    "desc": "최대 내접원 반지름/최소 외접원 반지름 (0~1)",
    "hint": "1=원형·고른 형상"
  },
  "inner_rect_fill_ratio": {
    "label": "내접사각 채움비",
    "desc": "최대 내접 사각형 면적/바운딩박스 면적",
    "hint": "1=사각형성 높음"
  },
  "connect_components": {
    "label": "연결 성분 수",
    "desc": "영역을 이루는 연결 덩어리 수(보통 1)",
    "hint": ">1이면 분리된 조각"
  },
  "holes": {
    "label": "구멍 수",
    "desc": "영역 내부의 구멍(hole) 개수",
    "hint": "0=구멍 없음"
  },
  "euler_number": {
    "label": "오일러 수",
    "desc": "연결 성분 수 − 구멍 수",
    "hint": "1=구멍 없는 단일 덩어리, 낮으면 구멍 존재"
  },
  "num_runs": {
    "label": "런 개수",
    "desc": "행 단위 연속 전경 구간(run) 개수",
    "hint": "줄무늬·복잡도"
  },
  "k_factor": {
    "label": "K 계수",
    "desc": "런 개수/√면적",
    "hint": "클수록 복잡/줄무늬성"
  },
  "l_factor": {
    "label": "L 계수",
    "desc": "런 개수/영역 높이",
    "hint": "세로 복잡도"
  },
  "mean_run_length": {
    "label": "평균 런 길이",
    "desc": "면적/런 개수 (행당 평균 가로 길이)",
    "hint": "가로 두께 대략"
  },
  "run_len_min": {
    "label": "런 최소 길이",
    "desc": "가장 짧은 런의 픽셀 길이",
    "hint": ""
  },
  "run_len_max": {
    "label": "런 최대 길이",
    "desc": "가장 긴 런의 픽셀 길이",
    "hint": "가장 넓은 가로폭"
  },
  "run_len_mode": {
    "label": "런 최빈 길이",
    "desc": "가장 자주 나오는 런 길이",
    "hint": "대표 가로폭"
  },
  "thickness_mean": {
    "label": "평균 두께",
    "desc": "주축 방향을 따라 잰 수직 두께의 평균",
    "hint": "선형 결함의 굵기"
  },
  "thickness_max": {
    "label": "최대 두께",
    "desc": "주축 방향 수직 두께의 최댓값",
    "hint": "가장 두꺼운 지점"
  },
  "thickness_length": {
    "label": "주축 길이",
    "desc": "두께를 잰 주축 방향의 길이(구간 수)",
    "hint": "선형 결함 길이"
  },
  "center_row": {
    "label": "무게중심 Row",
    "desc": "영역 무게중심의 세로(row) 좌표",
    "hint": "위치(참고용)"
  },
  "center_col": {
    "label": "무게중심 Col",
    "desc": "영역 무게중심의 가로(col) 좌표",
    "hint": "위치(참고용)"
  },
  "bbox_row1": {
    "label": "바운딩박스 상단",
    "desc": "축평행 외접 사각형 좌상단 row",
    "hint": "위치(참고용)"
  },
  "bbox_col1": {
    "label": "바운딩박스 좌측",
    "desc": "좌상단 col",
    "hint": "위치(참고용)"
  },
  "bbox_row2": {
    "label": "바운딩박스 하단",
    "desc": "우하단 row",
    "hint": "위치(참고용)"
  },
  "bbox_col2": {
    "label": "바운딩박스 우측",
    "desc": "우하단 col",
    "hint": "위치(참고용)"
  },
  "rect2_center_row": {
    "label": "회전사각 중심 Row",
    "desc": "최소면적 회전 사각형 중심 row",
    "hint": "위치(참고용)"
  },
  "rect2_center_col": {
    "label": "회전사각 중심 Col",
    "desc": "회전 사각형 중심 col",
    "hint": "위치(참고용)"
  },
  "rect2_len1": {
    "label": "회전사각 반장변",
    "desc": "최소 회전 사각형의 긴 반변 길이",
    "hint": "길이 크기"
  },
  "rect2_len2": {
    "label": "회전사각 반단변",
    "desc": "짧은 반변 길이",
    "hint": "폭 크기"
  },
  "rect2_phi": {
    "label": "회전사각 각도(rad)",
    "desc": "회전 사각형의 방향각",
    "hint": "기울기"
  },
  "smallest_circle_row": {
    "label": "외접원 중심 Row",
    "desc": "최소 외접원 중심 row",
    "hint": "위치(참고용)"
  },
  "smallest_circle_col": {
    "label": "외접원 중심 Col",
    "desc": "최소 외접원 중심 col",
    "hint": "위치(참고용)"
  },
  "smallest_circle_radius": {
    "label": "외접원 반지름",
    "desc": "영역을 감싸는 최소 원의 반지름",
    "hint": "바깥 크기"
  },
  "inner_circle_row": {
    "label": "내접원 중심 Row",
    "desc": "최대 내접원 중심 row",
    "hint": "위치(참고용)"
  },
  "inner_circle_col": {
    "label": "내접원 중심 Col",
    "desc": "최대 내접원 중심 col",
    "hint": "위치(참고용)"
  },
  "inner_circle_radius": {
    "label": "내접원 반지름",
    "desc": "영역 안에 들어가는 최대 원의 반지름",
    "hint": "내부 폭"
  },
  "inner_rect_row1": {
    "label": "내접사각 상단",
    "desc": "최대 내접 축평행 사각형 좌상단 row",
    "hint": "위치(참고용)"
  },
  "inner_rect_col1": {
    "label": "내접사각 좌측",
    "desc": "좌상단 col",
    "hint": "위치(참고용)"
  },
  "inner_rect_row2": {
    "label": "내접사각 하단",
    "desc": "우하단 row",
    "hint": "위치(참고용)"
  },
  "inner_rect_col2": {
    "label": "내접사각 우측",
    "desc": "우하단 col",
    "hint": "위치(참고용)"
  },
  "hu0": {
    "label": "Hu 모멘트 1",
    "desc": "크기·회전·이동 불변 형상 서명 1",
    "hint": "형상 지문(고급)"
  },
  "hu1": {
    "label": "Hu 모멘트 2",
    "desc": "불변 형상 서명 2",
    "hint": "형상 지문(고급)"
  },
  "hu2": {
    "label": "Hu 모멘트 3",
    "desc": "불변 형상 서명 3",
    "hint": "형상 지문(고급)"
  },
  "hu3": {
    "label": "Hu 모멘트 4",
    "desc": "불변 형상 서명 4",
    "hint": "형상 지문(고급)"
  },
  "hu4": {
    "label": "Hu 모멘트 5",
    "desc": "불변 형상 서명 5",
    "hint": "형상 지문(고급)"
  },
  "hu5": {
    "label": "Hu 모멘트 6",
    "desc": "불변 형상 서명 6",
    "hint": "형상 지문(고급)"
  },
  "hu6": {
    "label": "Hu 모멘트 7",
    "desc": "불변 형상 서명 7",
    "hint": "형상 지문(고급)"
  },
  "m2nd_m20": {
    "label": "정규 2차모멘트 μ20",
    "desc": "row 방향 분산(정규화)",
    "hint": "세로 퍼짐(고급)"
  },
  "m2nd_m02": {
    "label": "정규 2차모멘트 μ02",
    "desc": "col 방향 분산(정규화)",
    "hint": "가로 퍼짐(고급)"
  },
  "m2nd_m11": {
    "label": "정규 2차모멘트 μ11",
    "desc": "row·col 공분산(정규화)",
    "hint": "기울어짐(고급)"
  },
  "m2nd_ia": {
    "label": "주축 관성 최대",
    "desc": "주축 관성의 최대 고유값",
    "hint": "긴 축(고급)"
  },
  "m2nd_ib": {
    "label": "주축 관성 최소",
    "desc": "주축 관성의 최소 고유값",
    "hint": "짧은 축(고급)"
  },
  "mc_mu20": {
    "label": "중심 2차모멘트 μ20",
    "desc": "row 분산(비정규화)",
    "hint": "고급"
  },
  "mc_mu02": {
    "label": "중심 2차모멘트 μ02",
    "desc": "col 분산(비정규화)",
    "hint": "고급"
  },
  "mc_mu11": {
    "label": "중심 2차모멘트 μ11",
    "desc": "row·col 공분산(비정규화)",
    "hint": "고급"
  },
  "m3rd_m30": {
    "label": "3차모멘트 m30",
    "desc": "row 방향 비대칭(왜도)",
    "hint": "고급"
  },
  "m3rd_m03": {
    "label": "3차모멘트 m03",
    "desc": "col 방향 비대칭(왜도)",
    "hint": "고급"
  },
  "m3rd_m21": {
    "label": "3차모멘트 m21",
    "desc": "혼합 3차 모멘트",
    "hint": "고급"
  },
  "m3rd_m12": {
    "label": "3차모멘트 m12",
    "desc": "혼합 3차 모멘트",
    "hint": "고급"
  },
  "moment_phi1": {
    "label": "회전불변 Φ1",
    "desc": "η20+η02 (회전 불변)",
    "hint": "고급"
  },
  "moment_phi2": {
    "label": "회전불변 Φ2",
    "desc": "(η20−η02)²+4η11² (회전 불변)",
    "hint": "고급"
  },
  "moment_psi1": {
    "label": "스케일불변 Ψ1",
    "desc": "η20 (스케일 불변)",
    "hint": "고급"
  },
  "moment_psi2": {
    "label": "스케일불변 Ψ2",
    "desc": "η11 (스케일 불변)",
    "hint": "고급"
  },
  "moment_psi3": {
    "label": "스케일불변 Ψ3",
    "desc": "η02 (스케일 불변)",
    "hint": "고급"
  },
  "moment_psi4": {
    "label": "스케일불변 Ψ4",
    "desc": "η20·η02 − η11² (스케일 불변)",
    "hint": "고급"
  },
  "area_mm2": {
    "label": "면적 (mm²)",
    "desc": "픽셀→mm 스케일 적용 시 실제 면적",
    "hint": "스케일 설정 시에만"
  }
};

function featureTip(key) {
  var d = (typeof FEATURE_DOCS !== "undefined") ? FEATURE_DOCS[key] : null;
  if (!d) return key;
  var t = (d.label ? d.label + " — " : "") + (d.desc || "");
  if (d.hint) t += " (" + d.hint + ")";
  return t;
}
