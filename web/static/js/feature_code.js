// feature_code.js - feature별 수식 + OpenCV(C++) 코드 스니펫 ('Code 보기' 모달용)
//  수식 출처: docs/HALCON13_FEATURE_SPEC.md, 코드: GlimRegionFeatures/src/UseCase/FeatureCalculator.cpp
"use strict";

var FEATURE_CODE = {
  area: {
    formula: "A = 영역의 전경 픽셀 수 (Σ 1)",
    opencv:
"// 이진 마스크의 전경 픽셀 수\n" +
"int area = cv::countNonZero(mask);\n" +
"// 또는 모멘트의 m00 (이진 픽셀 모멘트)\n" +
"cv::Moments m = cv::moments(mask, true);\n" +
"double A = m.m00;"
  },
  contlength: {
    formula: "L = 외곽 컨투어 둘레 길이",
    opencv:
"std::vector<cv::Point> outer; // findContours 외곽\n" +
"double L = cv::arcLength(outer, /*closed=*/true);"
  },
  circularity: {
    formula: "C = A / (π · max²),  max = 중심→컨투어 최대거리\n범위 0~1 (원=1)",
    opencv:
"cv::Moments m = cv::moments(mask, true);\n" +
"cv::Point2d c(m.m10/m.m00, m.m01/m.m00); // 무게중심\n" +
"double maxD = 0;\n" +
"for (auto& p : outer)\n" +
"    maxD = std::max(maxD, cv::norm(cv::Point2d(p) - c));\n" +
"double circularity = A / (CV_PI * maxD * maxD);"
  },
  compactness: {
    formula: "C = L² / (4π·A)\n원=1, 윤곽 복잡할수록 커짐(상한 없음)",
    opencv:
"double L = cv::arcLength(outer, true);\n" +
"double compactness = (L*L) / (4.0 * CV_PI * A);"
  },
  convexity: {
    formula: "C = A / A_convex   (볼록껍질 면적 대비)\n범위 0~1, 오목/톱니면 감소(Burr·Tear)",
    opencv:
"std::vector<cv::Point> hull;\n" +
"cv::convexHull(outer, hull);\n" +
"double aConvex = cv::contourArea(hull);\n" +
"double convexity = A / aConvex;"
  },
  rectangularity: {
    formula: "C = A / A_rect,  A_rect = 최소면적 회전사각형 면적\n범위 0~1 (사각형=1)",
    opencv:
"cv::RotatedRect rr = cv::minAreaRect(outer);\n" +
"double aRect = rr.size.width * rr.size.height;\n" +
"double rectangularity = A / aRect;"
  },
  roundness: {
    formula: "d_i = 중심→컨투어 거리\nDistance = mean(d_i),  Sigma = std(d_i)\nRoundness = 1 − Sigma/Distance\nSides = 1.4111·(Distance/Sigma)^0.4724",
    opencv:
"std::vector<double> d;\n" +
"for (auto& p : outer) d.push_back(cv::norm(cv::Point2d(p) - c));\n" +
"double mean = 0; for(double v:d) mean+=v; mean/=d.size();\n" +
"double var = 0; for(double v:d) var+=(v-mean)*(v-mean); var/=d.size();\n" +
"double sigma = std::sqrt(var);\n" +
"double roundness = 1.0 - sigma/mean;\n" +
"double sides = 1.4111 * std::pow(mean/sigma, 0.4724);"
  },
  ra: {
    formula: "중심 2차 모멘트 μ20,μ02,μ11(면적 정규화)에서\nRa = √(2·(μ20+μ02+√((μ20−μ02)²+4μ11²)))",
    opencv:
"cv::Moments m = cv::moments(mask, true);\n" +
"double mu20=m.mu20/m.m00, mu02=m.mu02/m.m00, mu11=m.mu11/m.m00;\n" +
"double t = std::sqrt((mu20-mu02)*(mu20-mu02) + 4*mu11*mu11);\n" +
"double Ra = std::sqrt(2.0*(mu20+mu02+t));\n" +
"double Rb = std::sqrt(2.0*(mu20+mu02-t));\n" +
"// 또는 근사: cv::RotatedRect e = cv::fitEllipse(outer);"
  },
  rb: {
    formula: "Rb = √(2·(μ20+μ02−√((μ20−μ02)²+4μ11²)))  (단반경)",
    opencv:
"// ra 항목과 동일 계산. Rb = √(2·(mu20+mu02 − t))\n" +
"double Rb = std::sqrt(2.0*(mu20+mu02-t));"
  },
  phi: {
    formula: "Phi = ½·atan2(2μ11, μ20−μ02)   (타원 장축 각, rad)",
    opencv:
"double phi = 0.5 * std::atan2(2*mu11, mu20 - mu02);"
  },
  anisometry: {
    formula: "Anisometry = Ra / Rb   (길쭉함, ≥1)",
    opencv:
"double anisometry = Ra / Rb;   // 원=1, 스크래치는 매우 큼"
  },
  bulkiness: {
    formula: "Bulkiness = π · Ra · Rb / A",
    opencv:
"double bulkiness = CV_PI * Ra * Rb / A;"
  },
  structure_factor: {
    formula: "StructureFactor = Anisometry · Bulkiness − 1",
    opencv:
"double structureFactor = anisometry * bulkiness - 1.0;"
  },
  diameter: {
    formula: "D = 영역 내 가장 먼 두 점 사이 거리(최대 캘리퍼)",
    opencv:
"double D = 0;\n" +
"std::vector<cv::Point> hull;\n" +
"cv::convexHull(outer, hull);   // 볼록껍질 위에서만 비교(빠름)\n" +
"for (size_t i=0;i<hull.size();++i)\n" +
"  for (size_t j=i+1;j<hull.size();++j)\n" +
"    D = std::max(D, cv::norm(hull[i]-hull[j]));"
  },
  smallest_circle_radius: {
    formula: "최소 외접원 반지름 (영역을 감싸는 최소 원)",
    opencv:
"cv::Point2f center; float radius;\n" +
"cv::minEnclosingCircle(outer, center, radius);"
  },
  euler_number: {
    formula: "Euler = 연결성분 수 − 구멍 수",
    opencv:
"std::vector<std::vector<cv::Point>> cs; std::vector<cv::Vec4i> hi;\n" +
"cv::findContours(mask, cs, hi, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);\n" +
"int holes=0; for(auto&h:hi) if(h[3]>=0) holes++;  // 자식=구멍\n" +
"int euler = 1 - holes;   // 단일 성분 가정"
  },
  holes: {
    formula: "구멍 수 = 내부(자식) 컨투어 개수",
    opencv:
"cv::findContours(mask, cs, hi, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);\n" +
"int holes=0; for(auto& h:hi) if(h[3]>=0) holes++;"
  },
  hu0: {
    formula: "Hu 모멘트 φ1 = η20 + η02  (7개 불변 모멘트 중 1번)",
    opencv:
"cv::Moments m = cv::moments(mask, true);\n" +
"double hu[7];\n" +
"cv::HuMoments(m, hu);   // hu[0..6] = 크기·회전·이동 불변"
  }
};

function featureCode(key) { return FEATURE_CODE[key] || null; }
function featureHasCode(key) { return !!FEATURE_CODE[key]; }

// 'Code 보기' 모달 열기
function openFeatureCode(key) {
  var c = FEATURE_CODE[key];
  if (!c) return;
  var d = (typeof FEATURE_DOCS !== "undefined") ? (FEATURE_DOCS[key] || {}) : {};
  el("featCodeTitle").textContent = key + (d.label ? "  ·  " + d.label : "");
  el("featCodeFormula").textContent = c.formula || "";
  el("featCodeOpencv").textContent = c.opencv || "";
  el("featCodeOverlay").classList.remove("hidden");
}

function closeFeatureCode() {
  var o = el("featCodeOverlay");
  if (o) o.classList.add("hidden");
}

// 모달 닫기 배선(오버레이 클릭/✕). el 은 util.js 로드 후 사용 가능하므로 지연 실행.
(function wireFeatCodeModal() {
  function attach() {
    var o = document.getElementById("featCodeOverlay");
    var x = document.getElementById("featCodeClose");
    if (!o) { return; }
    if (x) x.addEventListener("click", closeFeatureCode);
    o.addEventListener("click", function (e) { if (e.target === o) closeFeatureCode(); });
    document.addEventListener("keydown", function (e) { if (e.key === "Escape") closeFeatureCode(); });
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", attach);
  else attach();
})();
