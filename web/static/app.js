// app.js - GlimRegion 대시보드 프론트 (바닐라 JS, 외부 CDN 없음, SVG 직접 차트)
// 4탭 SPA: 홈 / 설정 / 결과 / 분석. 페이지 리로드 없음.
"use strict";

const state = {
  data: null,
  activeCode: null,   // null = 전체
  sortKey: "regionIndex",
  sortDir: "desc",
  previewFiles: [],
  previewSel: null,   // 선택된 미리보기 파일명
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
function setStatus(s) { el("runStatus").textContent = s; }

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
const FEAT_PALETTE = ["#4ea1ff", "#35c98b", "#ffb454", "#ff6ec7", "#a78bfa", "#f97316", "#22d3ee", "#e879f9"];
function featColor(i) { return FEAT_PALETTE[i % FEAT_PALETTE.length]; }

// ---------- 탭 전환 ----------
function showTab(name) {
  document.querySelectorAll(".tab-btn").forEach(b =>
    b.classList.toggle("active", b.getAttribute("data-tab") === name));
  document.querySelectorAll(".tab-page").forEach(p =>
    p.classList.toggle("active", p.id === "tab-" + name));
  if (name === "home") renderRecent();
}

// ---------- 설정 읽기/쓰기 ----------
function readSettings() {
  return {
    folderPath: el("folderPath").value.trim(),
    profile: el("profile").value,
    threads: parseInt(el("threads").value, 10) || 0,
    polarity: el("polarity").value,
    mode: el("mode").value,
    thresh: parseInt(el("thresh").value, 10),
    offset: parseInt(el("offset").value, 10),
  };
}
function applySettings(s) {
  if (!s) return;
  if (s.folderPath !== undefined) el("folderPath").value = s.folderPath;
  if (s.profile) el("profile").value = s.profile;
  if (s.threads !== undefined) el("threads").value = s.threads;
  if (s.polarity) el("polarity").value = s.polarity;
  if (s.mode) el("mode").value = s.mode;
  if (s.thresh !== undefined && !Number.isNaN(s.thresh)) el("thresh").value = s.thresh;
  if (s.offset !== undefined && !Number.isNaN(s.offset)) el("offset").value = s.offset;
  syncBinFields();
}
// 방식에 따라 TH/offset 입력 노출 토글
function syncBinFields() {
  const mode = el("mode").value;
  el("threshField").style.display = (mode === "fixed") ? "" : "none";
  el("offsetField").style.display = (mode === "auto") ? "" : "none";
}

// ---------- 최근 실행 기록 (localStorage) ----------
function loadRecent() {
  try { return JSON.parse(localStorage.getItem(RECENT_KEY) || "[]"); }
  catch (e) { return []; }
}
function saveRecent(list) {
  try { localStorage.setItem(RECENT_KEY, JSON.stringify(list.slice(0, 12))); } catch (e) {}
}
function pushRecent(settings, meta) {
  const list = loadRecent();
  const entry = Object.assign({}, settings, {
    time: Date.now(),
    totalRegions: meta.totalRegions,
    totalImages: meta.totalImages,
  });
  // 같은 폴더+이진화 조합은 최신으로 갱신(중복 제거)
  const key = e => `${e.folderPath}|${e.profile}|${e.polarity}|${e.mode}|${e.thresh}|${e.offset}`;
  const filtered = list.filter(e => key(e) !== key(entry));
  filtered.unshift(entry);
  saveRecent(filtered);
}
function renderRecent() {
  const list = loadRecent();
  const box = el("recentList");
  if (!list.length) { box.innerHTML = '<div class="empty">기록이 없습니다. 분석을 실행하면 여기에 쌓입니다.</div>'; return; }
  box.innerHTML = list.map((e, i) => {
    const dt = new Date(e.time);
    const ts = `${dt.getMonth() + 1}/${dt.getDate()} ${String(dt.getHours()).padStart(2, "0")}:${String(dt.getMinutes()).padStart(2, "0")}`;
    const binDesc = e.mode === "binary" ? `${e.polarity}·binary`
      : e.mode === "auto" ? `${e.polarity}·auto(off ${e.offset})`
        : `${e.polarity}·fixed(TH ${e.thresh})`;
    return `<div class="recent-item" data-idx="${i}">
      <div class="ri-main">
        <div class="ri-folder">${esc(e.folderPath || "(경로 없음)")}</div>
        <div class="ri-meta">${esc(e.profile)} · ${esc(binDesc)} · Region ${e.totalRegions ?? "-"} · 이미지 ${e.totalImages ?? "-"}</div>
      </div>
      <div class="ri-time">${ts}</div>
    </div>`;
  }).join("");
  box.querySelectorAll(".recent-item").forEach(item => {
    item.addEventListener("click", () => {
      const idx = parseInt(item.getAttribute("data-idx"), 10);
      const e = loadRecent()[idx];
      if (!e) return;
      applySettings(e);
      showTab("settings");
      setStatus("최근 기록에서 설정을 불러왔습니다.");
    });
  });
}

// ---------- 미리보기 ----------
async function loadPreviewFiles() {
  const folder = el("folderPath").value.trim();
  if (!folder) { setStatus("폴더 경로를 입력하세요."); return; }
  setStatus("미리보기 목록 로딩...");
  try {
    const res = await fetch("/api/files?folder=" + encodeURIComponent(folder) + "&n=5");
    const data = await res.json();
    if (!data.ok) { setStatus("오류: " + (data.error || res.status)); return; }
    state.previewFiles = data.files || [];
    state.previewSel = state.previewFiles.length ? state.previewFiles[0] : null;
    renderPreviewFiles();
    updatePreview();
    setStatus(`미리보기 파일 ${state.previewFiles.length}개`);
  } catch (e) {
    setStatus("미리보기 요청 실패: " + e);
  }
}
function renderPreviewFiles() {
  const box = el("previewFiles");
  if (!state.previewFiles.length) { box.innerHTML = '<div class="empty">이미지가 없습니다.</div>'; return; }
  box.innerHTML = state.previewFiles.map(f =>
    `<button class="pf-btn ${f === state.previewSel ? "active" : ""}" data-file="${esc(f)}">${esc(f)}</button>`).join("");
  box.querySelectorAll(".pf-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      state.previewSel = btn.getAttribute("data-file");
      renderPreviewFiles();
      updatePreview();
    });
  });
}
function updatePreview() {
  const pane = el("previewPane");
  const folder = el("folderPath").value.trim();
  if (!folder || !state.previewSel) { pane.innerHTML = '<div class="empty">미리보기를 불러오세요.</div>'; return; }
  const s = readSettings();
  const origUrl = imgUrl(joinPath(folder, state.previewSel));
  const q = new URLSearchParams({
    folder: folder, file: state.previewSel,
    polarity: s.polarity, mode: s.mode,
    thresh: String(Number.isNaN(s.thresh) ? 127 : s.thresh),
    offset: String(Number.isNaN(s.offset) ? 20 : s.offset),
    _: String(Date.now()),   // 캐시 방지
  });
  const binUrl = "/api/preview?" + q.toString();
  pane.innerHTML =
    `<figure><img src="${origUrl}" alt="원본"><figcaption>원본</figcaption></figure>`
    + `<figure><img src="${binUrl}" alt="이진화" onerror="this.style.opacity=0.2"><figcaption>이진화 (${esc(s.polarity)}/${esc(s.mode)})</figcaption></figure>`;
}

// ---------- 실행 ----------
async function runAnalysis() {
  const body = readSettings();
  if (!body.folderPath) { setStatus("폴더 경로를 입력하세요."); showTab("settings"); return; }

  el("runBtn").disabled = true;
  setStatus("실행 중...");
  try {
    const res = await fetch("/api/run", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    const data = await res.json();
    if (!data.ok) { setStatus("오류: " + (data.error || res.status)); return; }
    state.data = data;
    state.activeCode = null;
    pushRecent(body, data.summary);
    render();
    showTab("results");
    setStatus(`완료 · ${data.summary.totalRegions} Region · ${data.summary.elapsedMs} ms`);
  } catch (e) {
    setStatus("요청 실패: " + e);
  } finally {
    el("runBtn").disabled = false;
  }
}

// ---------- 렌더 ----------
function render() {
  const has = !!state.data;
  el("resultsEmpty").style.display = has ? "none" : "";
  el("analysisEmpty").style.display = has ? "none" : "";
  if (!has) return;
  renderSummary();
  renderDist();
  renderScoreChart();
  renderFilters();
  renderGallery();
}

function renderSummary() {
  const s = state.data.summary;
  const cards = [
    ["총 이미지", s.totalImages],
    ["총 Region", s.totalRegions],
    ["NO_REGION", s.noRegionCount],
    ["소요(ms)", s.elapsedMs],
  ];
  el("summary").innerHTML = cards.map(([lbl, num]) =>
    `<div class="stat-card"><div class="num">${num}</div><div class="lbl">${lbl}</div></div>`).join("");
}

// 불량 분포: 가로 막대 (SVG)
function renderDist() {
  const dist = state.data.defectDistribution || [];
  const box = el("distChart");
  if (!dist.length) { box.innerHTML = '<div class="empty">데이터 없음</div>'; return; }
  const max = Math.max(...dist.map(d => d.count));
  const rowH = 26, gap = 8, labelW = 120, barMax = 320, pad = 8;
  const w = labelW + barMax + 60;
  const h = pad * 2 + dist.length * rowH + (dist.length - 1) * gap;
  let svg = `<svg width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">`;
  dist.forEach((d, i) => {
    const y = pad + i * (rowH + gap);
    const bw = max > 0 ? (d.count / max) * barMax : 0;
    const col = codeColor(d.code);
    svg += `<text x="${labelW - 8}" y="${y + rowH / 2 + 4}" text-anchor="end" fill="#e6e9ef" font-size="12">${esc(d.code)}</text>`;
    svg += `<rect x="${labelW}" y="${y}" width="${bw}" height="${rowH}" rx="4" fill="${col}"></rect>`;
    svg += `<text x="${labelW + bw + 8}" y="${y + rowH / 2 + 4}" fill="#e6e9ef" font-size="12" font-weight="700">${d.count}</text>`;
  });
  svg += `</svg>`;
  box.innerHTML = svg;
}

// Score 분석: 분류코드별 score_* 평균 묶음 막대 (SVG grouped vertical bars)
function renderScoreChart() {
  const box = el("scoreChart");
  const sbc = state.data.scoreByCode || {};
  const feats = state.data.scoreFeatures || [];
  const codes = Object.keys(sbc);
  if (!codes.length || !feats.length) { box.innerHTML = '<div class="empty">프로파일 없음(Score 미산출)</div>'; return; }

  const chartH = 150, barW = 13, inGap = 2, groupGap = 26;
  const leftPad = 34, topPad = 10, bottomPad = 26;
  const groupW = feats.length * barW + (feats.length - 1) * inGap;
  const w = leftPad + codes.length * (groupW + groupGap) + 10;
  const h = topPad + chartH + bottomPad;

  let svg = `<svg width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">`;
  [0, 50, 100].forEach(g => {
    const y = topPad + chartH - (g / 100) * chartH;
    svg += `<line x1="${leftPad}" y1="${y}" x2="${w - 5}" y2="${y}" stroke="#2e3646"></line>`;
    svg += `<text x="${leftPad - 6}" y="${y + 4}" text-anchor="end" fill="#9aa3b2" font-size="10">${g}</text>`;
  });
  codes.forEach((code, ci) => {
    const gx = leftPad + ci * (groupW + groupGap);
    feats.forEach((f, fi) => {
      const v = sbc[code][f] || 0;
      const bh = (Math.max(0, Math.min(100, v)) / 100) * chartH;
      const x = gx + fi * (barW + inGap);
      const y = topPad + chartH - bh;
      svg += `<rect x="${x}" y="${y}" width="${barW}" height="${bh}" fill="${featColor(fi)}"><title>${esc(code)} · ${esc(f)}: ${v.toFixed(1)}</title></rect>`;
    });
    svg += `<text x="${gx + groupW / 2}" y="${topPad + chartH + 16}" text-anchor="middle" fill="#e6e9ef" font-size="11">${esc(code)}</text>`;
  });
  svg += `</svg>`;

  let legend = '<div class="legend">';
  feats.forEach((f, i) => {
    legend += `<span class="lg-item"><span class="lg-dot" style="background:${featColor(i)}"></span>${esc(f)}</span>`;
  });
  legend += "</div>";
  box.innerHTML = svg + legend;
}

// 분류코드 필터 버튼
function renderFilters() {
  const dist = state.data.defectDistribution || [];
  let html = `<button class="filter-btn ${state.activeCode === null ? "active" : ""}" data-code="">전체</button>`;
  dist.forEach(d => {
    html += `<button class="filter-btn ${state.activeCode === d.code ? "active" : ""}" data-code="${esc(d.code)}">`
      + `<span class="dot" style="background:${codeColor(d.code)}"></span>${esc(d.code)} (${d.count})</button>`;
  });
  const box = el("codeFilters");
  box.innerHTML = html;
  box.querySelectorAll(".filter-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      const c = btn.getAttribute("data-code");
      state.activeCode = c === "" ? null : c;
      renderFilters();
      renderGallery();
    });
  });
}

function rowCode(r) {
  return r.regionIndex < 0 ? "NO_REGION" : (r.classifiedCode || "(none)");
}

function renderGallery() {
  const feats = state.data.scoreFeatures || [];
  let rows = state.data.rows.slice();
  if (state.activeCode !== null)
    rows = rows.filter(r => rowCode(r) === state.activeCode);

  const k = state.sortKey, dir = state.sortDir === "asc" ? 1 : -1;
  rows.sort((a, b) => {
    let va = a[k], vb = b[k];
    va = (va === null || va === undefined) ? -Infinity : va;
    vb = (vb === null || vb === undefined) ? -Infinity : vb;
    return va < vb ? -dir : va > vb ? dir : 0;
  });

  const gallery = el("gallery");
  gallery.innerHTML = "";
  rows.forEach(r => gallery.appendChild(buildCard(r, feats)));
}

function buildCard(r, feats) {
  const card = document.createElement("div");
  card.className = "card";
  const code = rowCode(r);
  const badgeCol = codeColor(code);

  const featPairs = [
    ["area", 1], ["circularity", 3], ["convexity", 3],
    ["roundness", 3], ["anisometry", 3], ["diameter", 2],
  ];
  const featHtml = featPairs.map(([key, d]) =>
    `<div class="f"><span class="k">${key}</span><span class="v">${fmt(r[key], d)}</span></div>`).join("");

  const scoreHtml = feats.map(f => {
    const v = (r.scores && r.scores[f] !== undefined) ? r.scores[f] : null;
    const w = v === null ? 0 : Math.max(0, Math.min(100, v));
    return `<div class="srow"><span class="sk">${esc(f)}</span>`
      + `<span class="sbar"><div style="width:${w}%;background:${scoreColor(w)}"></div></span>`
      + `<span class="sv">${v === null ? "-" : v.toFixed(0)}</span></div>`;
  }).join("");

  const origImg = r.filePath
    ? `<figure><img loading="lazy" src="${imgUrl(r.filePath)}" alt="원본"><figcaption>원본</figcaption></figure>` : "";
  const ovImg = r.overlayPath
    ? `<figure><img loading="lazy" src="${imgUrl(r.overlayPath)}" alt="오버레이"><figcaption>오버레이</figcaption></figure>` : "";

  card.innerHTML =
    `<div class="imgs">${origImg}${ovImg}</div>`
    + `<div class="feat-grid">${featHtml}</div>`
    + `<div class="score-mini">${scoreHtml || '<span class="sk">score 없음</span>'}</div>`
    + `<div class="badge" style="background:${badgeCol}">${esc(code)}</div>`
    + `<div class="fname">${esc(r.fileName)} · #${r.regionIndex}</div>`;

  card.addEventListener("click", () => openDetail(r));
  return card;
}

// 상세 패널
function openDetail(r) {
  const feats = state.data.scoreFeatures || [];
  const code = rowCode(r);
  let html = `<h2 style="margin-bottom:10px">${esc(r.fileName)} · Region #${r.regionIndex}`
    + ` <span class="badge" style="background:${codeColor(code)};display:inline-block;min-width:auto;padding:3px 10px;font-size:12px">${esc(code)}</span></h2>`;
  if (r.overlayPath)
    html += `<img loading="lazy" src="${imgUrl(r.overlayPath)}" alt="오버레이">`;

  if (feats.length) {
    html += `<h2 style="margin:14px 0 6px">Score (0~100)</h2><div class="score-mini">`;
    feats.forEach(f => {
      const v = (r.scores && r.scores[f] !== undefined) ? r.scores[f] : null;
      const w = v === null ? 0 : Math.max(0, Math.min(100, v));
      html += `<div class="srow"><span class="sk">${esc(f)}</span>`
        + `<span class="sbar"><div style="width:${w}%;background:${scoreColor(w)}"></div></span>`
        + `<span class="sv">${v === null ? "-" : v.toFixed(1)}</span></div>`;
    });
    html += `</div>`;
  }

  html += `<h2 style="margin:14px 0 6px">전체 특징값</h2><table>`;
  const raw = r.raw || {};
  Object.keys(raw).forEach(k => {
    if (k === "FileName" || k === "FilePath") return;
    const val = raw[k];
    html += `<tr><td class="k">${esc(k)}</td><td class="v">${typeof val === "number" ? val.toFixed(4) : esc(val)}</td></tr>`;
  });
  html += `</table>`;

  el("detailBody").innerHTML = html;
  el("detailOverlay").classList.remove("hidden");
}

// ---------- 이벤트 ----------
document.querySelectorAll(".tab-btn").forEach(btn =>
  btn.addEventListener("click", () => showTab(btn.getAttribute("data-tab"))));

el("runBtn").addEventListener("click", runAnalysis);
el("folderPath").addEventListener("keydown", e => { if (e.key === "Enter") runAnalysis(); });
el("previewBtn").addEventListener("click", loadPreviewFiles);
el("mode").addEventListener("change", () => { syncBinFields(); updatePreview(); });
el("polarity").addEventListener("change", updatePreview);
el("thresh").addEventListener("input", updatePreview);
el("offset").addEventListener("input", updatePreview);
el("clearRecent").addEventListener("click", () => { saveRecent([]); renderRecent(); });

el("sortKey").addEventListener("change", e => { state.sortKey = e.target.value; if (state.data) renderGallery(); });
el("sortDir").addEventListener("click", () => {
  state.sortDir = state.sortDir === "asc" ? "desc" : "asc";
  el("sortDir").textContent = state.sortDir === "asc" ? "▲" : "▼";
  el("sortDir").setAttribute("data-dir", state.sortDir);
  if (state.data) renderGallery();
});
el("detailClose").addEventListener("click", () => el("detailOverlay").classList.add("hidden"));
el("detailOverlay").addEventListener("click", e => { if (e.target === el("detailOverlay")) el("detailOverlay").classList.add("hidden"); });

// 초기화
syncBinFields();
renderRecent();
