// feature_select.js - 관심 feature 공통 선택(설정) → 결과 정렬/분석 히스토/군집 공유
"use strict";

var SHAPE8 = ["circularity", "compactness", "convexity", "rectangularity",
              "roundness", "anisometry", "bulkiness", "structure_factor"];

// 선택 가능 feature 키(FEATURE_DOCS 전체). 좌표/각도류도 사용자가 원하면 선택 가능.
function selectableFeatureKeys() {
  if (typeof FEATURE_DOCS === "undefined") return SHAPE8.slice();
  return Object.keys(FEATURE_DOCS).filter(function (k) { return k !== "area_mm2"; });
}

function initFeatureSet() {
  var saved = null;
  try { saved = JSON.parse(localStorage.getItem("grf_featureSet") || "null"); } catch (e) {}
  state.featureSet = (Array.isArray(saved) && saved.length) ? saved : SHAPE8.slice();
}

function saveFeatureSet() {
  try { localStorage.setItem("grf_featureSet", JSON.stringify(state.featureSet)); } catch (e) {}
}

function updateFeatureSetCount() {
  var c = el("featSelCount");
  if (c) c.textContent = "선택 " + (state.featureSet ? state.featureSet.length : 0) + "개";
}

function renderFeatureSelect(filter) {
  var box = el("featureSelect");
  if (!box) return;
  var q = (filter || "").trim().toLowerCase();
  var keys = selectableFeatureKeys().filter(function (k) {
    if (!q) return true;
    var d = (typeof FEATURE_DOCS !== "undefined") ? (FEATURE_DOCS[k] || {}) : {};
    return k.toLowerCase().indexOf(q) >= 0 || (d.label || "").toLowerCase().indexOf(q) >= 0;
  });
  var setMap = {};
  (state.featureSet || []).forEach(function (k) { setMap[k] = 1; });
  box.innerHTML = keys.map(function (k) {
    var d = (typeof FEATURE_DOCS !== "undefined") ? (FEATURE_DOCS[k] || {}) : {};
    var checked = setMap[k] ? " checked" : "";
    var tip = (typeof featureTip === "function") ? featureTip(k) : k;
    return '<label class="feat-chk" title="' + esc(tip) + '">'
      + '<input type="checkbox" data-feat="' + esc(k) + '"' + checked + '> '
      + esc(k) + (d.label ? ' <span class="fc-lbl">' + esc(d.label) + '</span>' : '')
      + '</label>';
  }).join("");
  box.querySelectorAll("input[data-feat]").forEach(function (cb) {
    cb.addEventListener("change", function () {
      var k = cb.getAttribute("data-feat");
      var i = state.featureSet.indexOf(k);
      if (cb.checked && i < 0) state.featureSet.push(k);
      else if (!cb.checked && i >= 0) state.featureSet.splice(i, 1);
      afterFeatureSetChange();
    });
  });
  updateFeatureSetCount();
}

function setFeaturePreset(name) {
  if (name === "shape8") state.featureSet = SHAPE8.slice();
  else if (name === "all") state.featureSet = selectableFeatureKeys().slice();
  else if (name === "clear") state.featureSet = [];
  afterFeatureSetChange();
  renderFeatureSelect(el("featSearch") ? el("featSearch").value : "");
}

// featureSet 변경 후: 저장 + 의존 드롭다운(정렬/히스토) 갱신
function afterFeatureSetChange() {
  saveFeatureSet();
  updateFeatureSetCount();
  syncFeatureDropdowns();
}

// 결과 정렬(sortKey) + 분석 히스토(histFeature) 드롭다운을 선택 세트로 갱신(현재 값 유지 시도)
function syncFeatureDropdowns() {
  var feats = (state.featureSet && state.featureSet.length) ? state.featureSet.slice() : SHAPE8.slice();
  var sk = el("sortKey");
  if (sk) {
    var cur = sk.value;
    sk.innerHTML = '<option value="regionIndex">순서(기본)</option>' +
      feats.map(function (k) { return '<option value="' + esc(k) + '">' + esc(k) + "</option>"; }).join("");
    sk.value = (cur === "regionIndex" || feats.indexOf(cur) >= 0) ? cur : "regionIndex";
    state.sortKey = sk.value;
  }
  var hf = el("histFeature");
  if (hf) {
    var cur2 = hf.value;
    hf.innerHTML = feats.map(function (k) { return '<option value="' + esc(k) + '">' + esc(k) + "</option>"; }).join("");
    hf.value = (feats.indexOf(cur2) >= 0) ? cur2 : feats[0];
  }
}

// 초기화 + 이벤트 배선(main.js 에서 호출)
function initFeatureSelectUI() {
  initFeatureSet();
  renderFeatureSelect("");
  syncFeatureDropdowns();
  var s = el("featSearch");
  if (s) s.addEventListener("input", function () { renderFeatureSelect(s.value); });
  ["shape8", "all", "clear"].forEach(function (name) {
    var b = el("featPreset_" + name);
    if (b) b.addEventListener("click", function () { setFeaturePreset(name); });
  });
}
