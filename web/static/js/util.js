// util.js - 전역 상태 + 공용 유틸(색상/포맷/상태표시/토스트)
// 로드 순서상 가장 먼저. 다른 파일들이 이 전역들을 참조한다.
"use strict";

const state = {
  data: null,
  activeCode: null,     // null = 전체
  activeChannel: null,  // null = 전체, "B" | "W" (projection)
  sortKey: "regionIndex",
  sortDir: "desc",
  previewFiles: [],
  previewSel: null,   // 선택된 미리보기 파일명
  presets: [],        // 서버 저장 프리셋 목록
  cluster: null,      // 군집 결과(/api/cluster 응답, 축 변경 시 재요청 없이 재사용)
};

const RECENT_KEY = "glimregion.recent.v1";

// ---------- 유틸 ----------
function el(id) { return document.getElementById(id); }
function imgUrl(path) { return "/api/image?path=" + encodeURIComponent(path); }
function joinPath(folder, file) {
  const f = folder.replace(/[\\/]+$/, "");
  return f + "/" + file;
}
function fmt(v, d = 3) {
  if (v === null || v === undefined || v === "") return "-";
  if (typeof v === "number") return Number.isFinite(v) ? v.toFixed(d) : "-";
  return String(v);
}
function esc(s) {
  return String(s).replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
}
// 상태 표시. kind: "" | "busy"(스피너) | "error"(빨강)
function setStatus(s, kind = "") {
  const node = el("runStatus");
  node.textContent = s;
  node.classList.toggle("busy", kind === "busy");
  node.classList.toggle("error", kind === "error");
}

// 전역 토스트(사용자 메시지). kind: "error" | "success" | ""
let _toastTimer = null;
function toast(msg, kind = "", ms = 3800) {
  const node = el("toast");
  if (!node) return;
  node.textContent = msg;
  node.className = "toast " + (kind || "");
  if (_toastTimer) clearTimeout(_toastTimer);
  _toastTimer = setTimeout(() => node.classList.add("hidden"), ms);
}
// 에러를 상태바 + 토스트 + 콘솔에 일관 표시(콘솔만 찍고 끝나지 않게)
function showError(msg) {
  console.error("[GlimRegion]", msg);
  setStatus("오류: " + msg, "error");
  toast(msg, "error", 5000);
}

// 분류코드 → 고정색 (해시 기반, 중립코드는 회색). 분포차트/뱃지/필터/히스토그램 공유.
function codeColor(code) {
  if (!code || code === "NO_REGION" || code === "OK" || code === "(none)")
    return "#6b7484";
  let h = 0;
  for (let i = 0; i < code.length; i++) h = (h * 31 + code.charCodeAt(i)) & 0xffff;
  const hue = h % 360;
  return `hsl(${hue}, 62%, 58%)`;
}
function scoreColor(v) {
  const t = Math.max(0, Math.min(100, v || 0));
  return `hsl(${210 - t * 0.7}, 68%, 50%)`;
}
// Channel 색: 오버레이 컨투어와 동일 의미(B=흑불량=빨강, W=백불량=초록).
function channelColor(ch) {
  const c = (ch || "").toUpperCase();
  if (c === "B") return "#e0524f";
  if (c === "W") return "#35b96f";
  return "#6b7484";
}
function channelLabel(ch) {
  const c = (ch || "").toUpperCase();
  if (c === "B") return "흑 B";
  if (c === "W") return "백 W";
  return "";
}
// Channel UI(뱃지/필터)는 projection 실행일 때만 의미가 있음(그 외엔 단일 채널로 전부 동일 태그).
function showChannels() { return !!(state.data && state.data.isProjection); }
// mm 표시는 scale 이 1.0 이 아닐 때만(엔진은 scale=1.0 에서도 area_mm2=area 로 항상 컬럼을 채움).
function scaleApplied() {
  if (!state.data) return false;
  const sx = state.data.scaleX, sy = state.data.scaleY;
  return (typeof sx === "number" && sx !== 1) || (typeof sy === "number" && sy !== 1);
}
const FEAT_PALETTE = ["#4ea1ff", "#35c98b", "#ffb454", "#ff6ec7", "#a78bfa", "#f97316", "#22d3ee", "#e879f9"];
function featColor(i) { return FEAT_PALETTE[i % FEAT_PALETTE.length]; }

// 군집 팔레트 — MFC 뷰어 CClusterPanelCtrl::ClusterColor 와 동일(tab10 계열, 시인성 우선).
const CLUSTER_PALETTE = [
  "rgb(80,160,240)", "rgb(240,130,60)", "rgb(110,210,110)",
  "rgb(230,90,100)", "rgb(190,130,230)", "rgb(235,205,70)",
  "rgb(80,210,210)", "rgb(230,120,195)", "rgb(150,150,240)",
  "rgb(170,220,90)",
];
function clusterColor(id) {
  if (id === null || id === undefined || id < 0) return "#9aa3b2"; // 미할당/노이즈
  return CLUSTER_PALETTE[id % CLUSTER_PALETTE.length];
}
