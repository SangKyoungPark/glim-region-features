// main.js - 이벤트 배선 + 초기화 (로드 순서상 가장 마지막)
"use strict";

// ---------- 이벤트 ----------
document.querySelectorAll(".tab-btn").forEach(btn =>
  btn.addEventListener("click", () => showTab(btn.getAttribute("data-tab"))));

el("runBtn").addEventListener("click", runAnalysis);
el("folderPath").addEventListener("keydown", e => { if (e.key === "Enter") runAnalysis(); });
el("previewBtn").addEventListener("click", loadPreviewFiles);
el("mode").addEventListener("change", () => { syncBinFields(); updatePreview(); });
if (el("dual")) el("dual").addEventListener("change", syncBinFields);
el("polarity").addEventListener("change", updatePreview);
el("thresh").addEventListener("input", updatePreview);
el("offset").addEventListener("input", updatePreview);
el("kernel").addEventListener("input", updatePreview);
el("blur").addEventListener("input", updatePreview);
el("response").addEventListener("input", updatePreview);
el("blackTh").addEventListener("input", updatePreview);
el("whiteTh").addEventListener("input", updatePreview);
el("projKernel").addEventListener("input", updatePreview);
el("clearRecent").addEventListener("click", () => { saveRecent([]); renderRecent(); });

// 프리셋: 콤보 선택 시 즉시 적용, 저장/삭제 버튼
el("presetSelect").addEventListener("change", e => applyPresetByName(e.target.value));
el("presetSaveBtn").addEventListener("click", savePresetPrompt);
el("presetDeleteBtn").addEventListener("click", deletePreset);

// 결과 탭 CSV 다운로드
el("csvDownloadBtn").addEventListener("click", downloadCsv);

el("histFeature").addEventListener("change", () => { if (state.data) renderHistogram(); });
el("sortKey").addEventListener("change", e => { state.sortKey = e.target.value; if (state.data) renderGallery(); });
el("sortDir").addEventListener("click", () => {
  state.sortDir = state.sortDir === "asc" ? "desc" : "asc";
  el("sortDir").textContent = state.sortDir === "asc" ? "▲" : "▼";
  el("sortDir").setAttribute("data-dir", state.sortDir);
  if (state.data) renderGallery();
});
// 군집 탭: 실행 버튼 + 축 콤보(재요청 없이 다시 그리기)
el("clRunBtn").addEventListener("click", runCluster);
el("clFolder").addEventListener("keydown", e => { if (e.key === "Enter") runCluster(); });
el("clXAxis").addEventListener("change", () => { if (state.cluster) renderClusterScatter(); });
el("clYAxis").addEventListener("change", () => { if (state.cluster) renderClusterScatter(); });

el("detailClose").addEventListener("click", () => el("detailOverlay").classList.add("hidden"));
el("detailOverlay").addEventListener("click", e => { if (e.target === el("detailOverlay")) el("detailOverlay").classList.add("hidden"); });

// 이력 탭 버튼/필터 배선
(function () {
  const b = el("histRefreshBtn"); if (b) b.addEventListener("click", loadHistory);
  const s = el("histSearchBtn"); if (s) s.addEventListener("click", loadHistory);
  const r = el("histResetBtn"); if (r) r.addEventListener("click", () => {
    el("histFrom").value = ""; el("histTo").value = "";
    el("histCode").value = ""; el("histFolder").value = "";
    loadHistory();
  });
  ["histFolder", "histFrom", "histTo"].forEach(id => {
    const e = el(id);
    if (e) e.addEventListener("keydown", ev => { if (ev.key === "Enter") loadHistory(); });
  });
  const cs = el("histCode"); if (cs) cs.addEventListener("change", loadHistory);
})();

// 특징값 용어집(홈 탭) + 검색
renderFeatureGlossary();
(function () {
  const s = el("glossarySearch");
  if (s) s.addEventListener("input", () => renderFeatureGlossary(s.value));
})();

// 초기화
syncBinFields();
renderRecent();
updateCsvButton();
loadPresets();
