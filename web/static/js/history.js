// history.js - 검사 이력(History, PostgreSQL) 조회/렌더
"use strict";

// ---------- 검사 이력(History, PostgreSQL) ----------
function histQueryString() {
  const p = new URLSearchParams();
  p.set("limit", "300");
  const f = el("histFrom").value, t = el("histTo").value;
  const code = el("histCode").value, folder = el("histFolder").value.trim();
  if (f) p.set("date_from", f);
  if (t) p.set("date_to", t);
  if (code) p.set("code", code);
  if (folder) p.set("folder", folder);
  return p.toString();
}

// 코드 필터 드롭다운 채우기(현재 선택 유지)
async function loadHistCodes() {
  const sel = el("histCode");
  if (!sel) return;
  try {
    const res = await fetch("/api/codes");
    const data = await res.json();
    const cur = sel.value;
    const codes = data.codes || [];
    sel.innerHTML = '<option value="">(전체)</option>' +
      codes.map(c => `<option value="${esc(c)}">${esc(c)}</option>`).join("");
    sel.value = cur;
  } catch (e) { /* 무시 */ }
}

async function loadHistory() {
  const listBox = el("histRunList");
  const stateEl = el("histDbState");
  if (!listBox) return;
  loadHistCodes();
  listBox.innerHTML = '<div class="empty">불러오는 중...</div>';
  try {
    const res = await fetch("/api/runs?" + histQueryString());
    const data = await res.json();
    if (!data.enabled) {
      stateEl.textContent = "DB 비활성 상태입니다 (web/db_config.json 없음). 분석 결과가 저장되지 않습니다.";
      listBox.innerHTML = '<div class="empty">저장된 이력이 없습니다.</div>';
      return;
    }
    const filtered = !!(el("histFrom").value || el("histTo").value || el("histCode").value || el("histFolder").value.trim());
    stateEl.textContent = `PostgreSQL 연결됨 · ${filtered ? "필터 " : ""}조회된 실행 ${data.runs.length}건`;
    renderRunList(data.runs);
  } catch (e) {
    stateEl.textContent = "이력 조회 실패: " + e;
    listBox.innerHTML = '<div class="empty">조회 실패</div>';
  }
}

function renderRunList(runs) {
  const box = el("histRunList");
  if (!runs.length) { box.innerHTML = '<div class="empty">저장된 이력이 없습니다. [설정]에서 분석을 실행하면 자동 저장됩니다.</div>'; return; }
  box.innerHTML = runs.map(r => {
    const when = (r.created_at || "").replace("T", " ").slice(0, 19);
    return `<div class="hist-run" data-id="${r.id}">
      <div class="hr-main">
        <span class="hr-id">#${r.id}</span>
        <span class="hr-folder">${esc(r.folder || "")}</span>
      </div>
      <div class="hr-meta">${when} · profile=${esc(r.profile || "none")} · bin=${esc(r.binarize_label || "")} · files=${r.total_files ?? "-"} · regions=${r.total_regions ?? "-"}</div>
    </div>`;
  }).join("");
  box.querySelectorAll(".hist-run").forEach(el2 =>
    el2.addEventListener("click", () => loadRunDetail(parseInt(el2.getAttribute("data-id"), 10))));
}

async function loadRunDetail(runId) {
  const card = el("histDetailCard");
  const tbl = el("histRegionTable");
  try {
    const res = await fetch(`/api/runs/${runId}`);
    const data = await res.json();
    if (!data.ok) { toast(data.error || "상세 조회 실패", "error"); return; }
    el("histDetailTitle").textContent = `Run #${runId} · Region ${data.regions.length}건`;
    el("histDetailMeta").textContent = `${esc(data.run.folder || "")} · ${(data.run.created_at || "").replace("T", " ").slice(0, 19)}`;
    renderRegionTable(data.regions);
    card.style.display = "";
    card.scrollIntoView({ behavior: "smooth", block: "start" });
  } catch (e) {
    toast("상세 조회 실패: " + e, "error");
  }
}

function renderRegionTable(regions) {
  const box = el("histRegionTable");
  if (!regions.length) { box.innerHTML = '<div class="empty">Region 없음</div>'; return; }
  const num = v => (typeof v === "number" && Number.isFinite(v)) ? v.toFixed(3) : "-";
  const th = k => `<th title="${esc(featureTip(k))}">${k}</th>`;
  const head = `<tr><th>File</th><th>#</th><th>Ch</th>${th("area")}${th("circularity")}` +
    `${th("convexity")}${th("roundness")}${th("anisometry")}<th>code</th></tr>`;
  const body = regions.map(r => `<tr>
    <td class="rt-file">${esc(r.file_name || "")}</td>
    <td>${r.region_index}</td>
    <td>${esc(r.channel || "")}</td>
    <td>${num(r.area)}</td>
    <td>${num(r.circularity)}</td>
    <td>${num(r.convexity)}</td>
    <td>${num(r.roundness)}</td>
    <td>${num(r.anisometry)}</td>
    <td>${esc(r.classified_code || "")}</td>
  </tr>`).join("");
  box.innerHTML = `<table class="hist-table">${head}${body}</table>`;
}
