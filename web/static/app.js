// app.js - GlimRegion 대시보드 프론트 (바닐라 JS, 외부 CDN 없음, SVG 직접 차트)
// 4탭 SPA: 홈 / 설정 / 결과 / 분석. 페이지 리로드 없음.
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
    kernel: parseInt(el("kernel").value, 10),
    blur: parseInt(el("blur").value, 10),
    response: parseInt(el("response").value, 10),
    blackTh: parseInt(el("blackTh").value, 10),
    whiteTh: parseInt(el("whiteTh").value, 10),
    projKernel: parseInt(el("projKernel").value, 10),
    scaleX: parseFloat(el("scaleX").value),
    scaleY: parseFloat(el("scaleY").value),
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
  if (s.kernel !== undefined && !Number.isNaN(s.kernel)) el("kernel").value = s.kernel;
  if (s.blur !== undefined && !Number.isNaN(s.blur)) el("blur").value = s.blur;
  if (s.response !== undefined && !Number.isNaN(s.response)) el("response").value = s.response;
  if (s.blackTh !== undefined && !Number.isNaN(s.blackTh)) el("blackTh").value = s.blackTh;
  if (s.whiteTh !== undefined && !Number.isNaN(s.whiteTh)) el("whiteTh").value = s.whiteTh;
  if (s.projKernel !== undefined && !Number.isNaN(s.projKernel)) el("projKernel").value = s.projKernel;
  if (s.scaleX !== undefined && !Number.isNaN(s.scaleX)) el("scaleX").value = s.scaleX;
  if (s.scaleY !== undefined && !Number.isNaN(s.scaleY)) el("scaleY").value = s.scaleY;
  syncBinFields();
}
// 방식에 따라 TH/offset/커널/누적/응답/흑백TH 입력 노출 토글
function syncBinFields() {
  const mode = el("mode").value;
  const isProj = (mode === "projection");
  el("threshField").style.display = (mode === "fixed") ? "" : "none";
  el("offsetField").style.display = (mode === "auto") ? "" : "none";
  el("kernelField").style.display = (mode === "wrinkle") ? "" : "none";
  el("blurField").style.display = (mode === "wrinkle") ? "" : "none";
  el("responseField").style.display = (mode === "wrinkle") ? "" : "none";
  el("blackThField").style.display = isProj ? "" : "none";
  el("whiteThField").style.display = isProj ? "" : "none";
  el("projKernelField").style.display = isProj ? "" : "none";
  // projection 은 흑/백 2채널 동시 산출이라 극성 무의미 → 극성 입력 숨김
  el("polarityField").style.display = isProj ? "none" : "";
}

// ---------- 설정 프리셋 (서버 web/presets.json) ----------
async function loadPresets(selectName) {
  try {
    const res = await fetch("/api/presets");
    const data = await res.json();
    if (!data.ok) { showError(data.error || "프리셋 목록 로딩 실패"); return; }
    state.presets = data.presets || [];
    renderPresetSelect(selectName);
  } catch (e) {
    showError("프리셋 목록 요청 실패: " + e);
  }
}
function renderPresetSelect(selectName) {
  const sel = el("presetSelect");
  if (!sel) return;
  let html = '<option value="">— 프리셋 선택 —</option>';
  state.presets.forEach(p => {
    const tag = p.builtin ? " ★" : "";
    html += `<option value="${esc(p.name)}">${esc(p.name)}${tag}</option>`;
  });
  sel.innerHTML = html;
  if (selectName) sel.value = selectName;
}
function applyPresetByName(name) {
  if (!name) return;
  const p = state.presets.find(x => x.name === name);
  if (!p) { showError("프리셋을 찾을 수 없습니다: " + name); return; }
  // 폴더 경로는 프리셋에 비어 있으면 현재 입력을 보존(불필요하게 지우지 않음)
  const s = Object.assign({}, p.settings);
  if (s.folderPath === undefined || s.folderPath === "") delete s.folderPath;
  applySettings(s);
  setStatus(`프리셋 '${name}' 적용됨`);
}
async function savePresetPrompt() {
  const sel = el("presetSelect");
  const suggested = (sel && sel.value) ? sel.value : "";
  const name = (window.prompt("저장할 프리셋 이름", suggested) || "").trim();
  if (!name) return;
  const settings = readSettings();
  try {
    const res = await fetch("/api/presets", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name, settings }),
    });
    const data = await res.json();
    if (!data.ok) { showError(data.error || "프리셋 저장 실패"); return; }
    state.presets = data.presets || [];
    renderPresetSelect(name);
    toast(`프리셋 '${name}' 저장됨`, "success");
    setStatus(`프리셋 '${name}' 저장됨`);
  } catch (e) {
    showError("프리셋 저장 요청 실패: " + e);
  }
}
async function deletePreset() {
  const sel = el("presetSelect");
  const name = sel ? sel.value : "";
  if (!name) { toast("삭제할 프리셋을 선택하세요.", "error"); return; }
  const p = state.presets.find(x => x.name === name);
  if (p && p.builtin) { toast("기본 제공 프리셋은 삭제할 수 없습니다.", "error"); return; }
  if (!window.confirm(`프리셋 '${name}' 을(를) 삭제할까요?`)) return;
  try {
    const res = await fetch("/api/presets?name=" + encodeURIComponent(name), { method: "DELETE" });
    const data = await res.json();
    if (!data.ok) { showError(data.error || "프리셋 삭제 실패"); return; }
    state.presets = data.presets || [];
    renderPresetSelect("");
    toast(`프리셋 '${name}' 삭제됨`, "success");
    setStatus(`프리셋 '${name}' 삭제됨`);
  } catch (e) {
    showError("프리셋 삭제 요청 실패: " + e);
  }
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
  const key = e => `${e.folderPath}|${e.profile}|${e.polarity}|${e.mode}|${e.thresh}|${e.offset}|${e.blackTh}|${e.whiteTh}|${e.scaleX}|${e.scaleY}`;
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
      : e.mode === "wrinkle" ? `wrinkle(k ${e.kernel}/b ${e.blur}/r ${e.response})`
      : e.mode === "projection" ? `projection(흑 ${e.blackTh}/백 ${e.whiteTh})`
      : e.mode === "auto" ? `${e.polarity}·auto(off ${e.offset})`
        : `${e.polarity}·fixed(TH ${e.thresh})`;
    const scaleDesc = (e.scaleX && e.scaleX !== 1) || (e.scaleY && e.scaleY !== 1)
      ? ` · scale ${e.scaleX}×${e.scaleY}` : "";
    return `<div class="recent-item" data-idx="${i}">
      <div class="ri-main">
        <div class="ri-folder">${esc(e.folderPath || "(경로 없음)")}</div>
        <div class="ri-meta">${esc(e.profile)} · ${esc(binDesc)}${esc(scaleDesc)} · Region ${e.totalRegions ?? "-"} · 이미지 ${e.totalImages ?? "-"}</div>
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
  if (!folder) { toast("폴더 경로를 입력하세요.", "error"); setStatus("폴더 경로를 입력하세요.", "error"); return; }
  setStatus("미리보기 목록 로딩...", "busy");
  try {
    const res = await fetch("/api/files?folder=" + encodeURIComponent(folder) + "&n=5");
    const data = await res.json();
    if (!data.ok) { showError(data.error || `미리보기 목록 실패 (HTTP ${res.status})`); return; }
    state.previewFiles = data.files || [];
    state.previewSel = state.previewFiles.length ? state.previewFiles[0] : null;
    renderPreviewFiles();
    updatePreview();
    if (!state.previewFiles.length) { setStatus("폴더에 이미지가 없습니다.", "error"); toast("폴더에 이미지가 없습니다.", "error"); }
    else setStatus(`미리보기 파일 ${state.previewFiles.length}개`);
  } catch (e) {
    showError("미리보기 요청 실패: " + e);
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
    kernel: String(Number.isNaN(s.kernel) ? 15 : s.kernel),
    blur: String(Number.isNaN(s.blur) ? 31 : s.blur),
    response: String(Number.isNaN(s.response) ? 4 : s.response),
    blackTh: String(Number.isNaN(s.blackTh) ? 20 : s.blackTh),
    whiteTh: String(Number.isNaN(s.whiteTh) ? 235 : s.whiteTh),
    projKernel: String(Number.isNaN(s.projKernel) ? 3 : s.projKernel),
    _: String(Date.now()),   // 캐시 방지
  });
  const binUrl = "/api/preview?" + q.toString();

  if (s.mode === "projection") {
    // exe 가 흑|백 2채널을 가로로 이어붙인 1장을 반환 → 캔버스로 좌/우 분할해 3단 표시
    pane.innerHTML =
      `<figure><img src="${origUrl}" alt="원본"><figcaption>원본</figcaption></figure>`
      + `<figure id="projBlack" class="proj-fig"><div class="empty">흑 로딩...</div><figcaption>흑 이진화 (TH ${esc(String(s.blackTh))})</figcaption></figure>`
      + `<figure id="projWhite" class="proj-fig"><div class="empty">백 로딩...</div><figcaption>백 이진화 (TH ${esc(String(s.whiteTh))})</figcaption></figure>`;
    loadProjectionPreview(binUrl, s.blackTh, s.whiteTh);
    return;
  }

  pane.innerHTML =
    `<figure><img src="${origUrl}" alt="원본"><figcaption>원본</figcaption></figure>`
    + `<figure><img src="${binUrl}" alt="이진화" onerror="this.style.opacity=0.2"><figcaption>이진화 (${esc(s.polarity)}/${esc(s.mode)})</figcaption></figure>`;
}

// projection 미리보기: 서버가 흑|백을 가로로 이어붙인 PNG 를 반환 → 좌=흑, 우=백 으로 분할.
function loadProjectionPreview(binUrl, blackTh, whiteTh) {
  const img = new Image();
  img.onload = () => {
    const black = document.querySelector("#projBlack");
    const white = document.querySelector("#projWhite");
    try {
      const half = Math.floor(img.naturalWidth / 2);
      const h = img.naturalHeight;
      const crop = (sx) => {
        const c = document.createElement("canvas");
        c.width = half; c.height = h;
        c.getContext("2d").drawImage(img, sx, 0, half, h, 0, 0, half, h);
        return c.toDataURL("image/png");
      };
      if (half > 0) {
        if (black) black.innerHTML = `<img src="${crop(0)}" alt="흑 이진화"><figcaption>흑 이진화 (TH ${esc(String(blackTh))})</figcaption>`;
        if (white) white.innerHTML = `<img src="${crop(half)}" alt="백 이진화"><figcaption>백 이진화 (TH ${esc(String(whiteTh))})</figcaption>`;
      } else if (black) {
        // 단일 채널만 반환된 경우 통짜 표시
        black.innerHTML = `<img src="${img.src}" alt="이진화"><figcaption>이진화</figcaption>`;
        if (white) white.remove();
      }
    } catch (e) {
      // 캔버스 분할 실패 시 통짜 폴백
      if (black) black.innerHTML = `<img src="${img.src}" alt="이진화(흑|백)"><figcaption>흑|백 이진화</figcaption>`;
      if (white) white.remove();
    }
  };
  img.onerror = () => {
    const black = document.querySelector("#projBlack");
    if (black) black.innerHTML = `<div class="empty">미리보기 생성 실패</div><figcaption>흑 이진화</figcaption>`;
  };
  img.src = binUrl;
}

// ---------- 실행 ----------
async function runAnalysis() {
  const body = readSettings();
  if (!body.folderPath) {
    toast("폴더 경로를 입력하세요.", "error");
    setStatus("폴더 경로를 입력하세요.", "error");
    showTab("settings");
    el("folderPath").focus();
    return;
  }

  el("runBtn").disabled = true;
  el("runBtn").textContent = "실행 중...";
  setStatus("분석 실행 중... (exe 처리 대기)", "busy");
  try {
    const res = await fetch("/api/run", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    let data;
    try { data = await res.json(); }
    catch (e) { showError(`서버 응답 파싱 실패 (HTTP ${res.status})`); return; }
    if (!data.ok) {
      // exe 실패 상세(stderr)가 있으면 콘솔에 남기고 사용자에겐 요약 메시지
      if (data.stderr) console.error("[GlimRegion] exe stderr:\n" + data.stderr);
      showError(data.error || `분석 실패 (HTTP ${res.status})`);
      return;
    }
    state.data = data;
    state.activeCode = null;
    state.activeChannel = null;
    pushRecent(body, data.summary);
    render();
    updateCsvButton();
    showTab("results");
    setStatus(`완료 · ${data.summary.totalRegions} Region · ${data.summary.elapsedMs} ms`);
    toast(`분석 완료 · ${data.summary.totalRegions} Region`, "success");
  } catch (e) {
    showError("요청 실패: " + e + " (서버가 실행 중인지 확인하세요)");
  } finally {
    el("runBtn").disabled = false;
    el("runBtn").textContent = "분석 실행";
  }
}

// CSV 다운로드 버튼 상태 갱신(실행 결과 있을 때만 활성)
function updateCsvButton() {
  const btn = el("csvDownloadBtn");
  if (!btn) return;
  btn.disabled = !(state.data && state.data.hash);
}
function downloadCsv() {
  if (!state.data || !state.data.hash) { toast("다운로드할 결과가 없습니다.", "error"); return; }
  // 서버가 캐시의 result.csv 를 그대로 반환(Content-Disposition 파일명 포함)
  const url = "/api/download?hash=" + encodeURIComponent(state.data.hash);
  const a = document.createElement("a");
  a.href = url;
  a.download = "";
  document.body.appendChild(a);
  a.click();
  a.remove();
  setStatus("CSV 다운로드 요청됨");
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
  renderHistogram();
  renderFilters();
  renderChannelFilters();
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

// 특징값 히스토그램: 선택 특징을 구간(bin)으로 나누고 분류코드별로 누적(stacked) 세로 막대
function renderHistogram() {
  const box = el("histChart");
  const feature = el("histFeature").value;
  const rows = (state.data.rows || []).filter(r => r.regionIndex >= 0 && typeof r[feature] === "number" && Number.isFinite(r[feature]));
  if (!rows.length) { box.innerHTML = '<div class="empty">해당 특징 데이터 없음</div>'; return; }

  const vals = rows.map(r => r[feature]);
  let vmin = Math.min(...vals), vmax = Math.max(...vals);
  if (vmin === vmax) { vmax = vmin + 1; }  // 단일값 방어
  const BINS = 12;
  const span = vmax - vmin;
  const binOf = v => Math.min(BINS - 1, Math.floor(((v - vmin) / span) * BINS));

  // 분류코드 목록(분포 순서 유지)
  const codes = (state.data.defectDistribution || [])
    .map(d => d.code).filter(c => c !== "NO_REGION");
  const codeSet = new Set(codes);
  rows.forEach(r => { const c = r.classifiedCode || "(none)"; if (!codeSet.has(c)) { codeSet.add(c); codes.push(c); } });

  // bins[b][code] = count
  const bins = Array.from({ length: BINS }, () => ({}));
  let maxTotal = 0;
  rows.forEach(r => {
    const b = binOf(r[feature]);
    const c = r.classifiedCode || "(none)";
    bins[b][c] = (bins[b][c] || 0) + 1;
  });
  bins.forEach(b => { maxTotal = Math.max(maxTotal, Object.values(b).reduce((s, v) => s + v, 0)); });
  if (maxTotal === 0) { box.innerHTML = '<div class="empty">데이터 없음</div>'; return; }

  const chartH = 180, leftPad = 34, topPad = 10, bottomPad = 34;
  const barW = 34, barGap = 6;
  const w = leftPad + BINS * (barW + barGap) + 10;
  const h = topPad + chartH + bottomPad;

  let svg = `<svg width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">`;
  // y 그리드
  const ticks = 4;
  for (let t = 0; t <= ticks; t++) {
    const val = Math.round((maxTotal / ticks) * t);
    const y = topPad + chartH - (t / ticks) * chartH;
    svg += `<line x1="${leftPad}" y1="${y}" x2="${w - 5}" y2="${y}" stroke="#2e3646"></line>`;
    svg += `<text x="${leftPad - 6}" y="${y + 4}" text-anchor="end" fill="#9aa3b2" font-size="10">${val}</text>`;
  }
  // 누적 막대
  for (let b = 0; b < BINS; b++) {
    const x = leftPad + b * (barW + barGap);
    let acc = 0;
    codes.forEach(code => {
      const cnt = bins[b][code] || 0;
      if (!cnt) return;
      const bh = (cnt / maxTotal) * chartH;
      const y = topPad + chartH - acc - bh;
      svg += `<rect x="${x}" y="${y}" width="${barW}" height="${bh}" fill="${codeColor(code)}"><title>${esc(code)}: ${cnt} (bin ${b + 1})</title></rect>`;
      acc += bh;
    });
    // x 라벨(구간 하한)
    if (b % 2 === 0) {
      const lo = vmin + (span / BINS) * b;
      svg += `<text x="${x + barW / 2}" y="${topPad + chartH + 14}" text-anchor="middle" fill="#9aa3b2" font-size="9">${lo.toFixed(span > 20 ? 0 : 2)}</text>`;
    }
  }
  svg += `<text x="${w / 2}" y="${h - 4}" text-anchor="middle" fill="#9aa3b2" font-size="10">${esc(feature)} 값 구간</text>`;
  svg += `</svg>`;

  // 범례(코드별)
  let legend = '<div class="legend">';
  codes.forEach(code => {
    legend += `<span class="lg-item"><span class="lg-dot" style="background:${codeColor(code)}"></span>${esc(code)}</span>`;
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

// Channel(흑/백) 필터 — projection 실행(hasChannel)일 때만 노출
function renderChannelFilters() {
  const box = el("channelFilters");
  if (!box) return;
  if (!showChannels()) { box.innerHTML = ""; return; }

  const counts = { B: 0, W: 0 };
  (state.data.rows || []).forEach(r => {
    const c = (r.channel || "").toUpperCase();
    if (c === "B" || c === "W") counts[c]++;
  });

  let html = `<button class="filter-btn ${state.activeChannel === null ? "active" : ""}" data-ch="">채널 전체</button>`;
  ["B", "W"].forEach(c => {
    html += `<button class="filter-btn ${state.activeChannel === c ? "active" : ""}" data-ch="${c}">`
      + `<span class="dot" style="background:${channelColor(c)}"></span>${esc(channelLabel(c))} (${counts[c]})</button>`;
  });
  box.innerHTML = html;
  box.querySelectorAll(".filter-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      const c = btn.getAttribute("data-ch");
      state.activeChannel = c === "" ? null : c;
      renderChannelFilters();
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
  if (state.activeChannel !== null)
    rows = rows.filter(r => (r.channel || "").toUpperCase() === state.activeChannel);

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
  let featHtml = featPairs.map(([key, d]) =>
    `<div class="f"><span class="k">${key}</span><span class="v">${fmt(r[key], d)}</span></div>`).join("");
  // mm 환산값(scale≠1.0 일 때만 표시)
  if (scaleApplied() && typeof r.areaMm2 === "number" && Number.isFinite(r.areaMm2))
    featHtml += `<div class="f mm"><span class="k">area_mm²</span><span class="v">${fmt(r.areaMm2, 4)}</span></div>`;

  const scoreHtml = feats.map(f => {
    const v = (r.scores && r.scores[f] !== undefined) ? r.scores[f] : null;
    const w = v === null ? 0 : Math.max(0, Math.min(100, v));
    return `<div class="srow"><span class="sk">${esc(f)}</span>`
      + `<span class="sbar"><div style="width:${w}%;background:${scoreColor(w)}"></div></span>`
      + `<span class="sv">${v === null ? "-" : v.toFixed(0)}</span></div>`;
  }).join("");

  const origImg = r.filePath
    ? `<figure><img loading="lazy" src="${imgUrl(r.filePath)}" alt="원본"><figcaption>원본</figcaption></figure>` : "";
  const binImg = r.binPath
    ? `<figure><img loading="lazy" src="${imgUrl(r.binPath)}" alt="이진화"><figcaption>이진화</figcaption></figure>` : "";
  const ovImg = r.overlayPath
    ? `<figure><img loading="lazy" src="${imgUrl(r.overlayPath)}" alt="오버레이"><figcaption>오버레이</figcaption></figure>` : "";

  // projection 채널 뱃지(흑 B / 백 W)
  const chBadge = (showChannels() && (r.channel === "B" || r.channel === "W"))
    ? `<div class="badge ch-badge" style="background:${channelColor(r.channel)}">${esc(channelLabel(r.channel))}</div>` : "";

  card.innerHTML =
    `<div class="imgs">${origImg}${binImg}${ovImg}</div>`
    + `<div class="feat-grid">${featHtml}</div>`
    + `<div class="score-mini">${scoreHtml || '<span class="sk">score 없음</span>'}</div>`
    + `<div class="badges">${chBadge}<div class="badge" style="background:${badgeCol}">${esc(code)}</div></div>`
    + `<div class="fname">${esc(r.fileName)} · #${r.regionIndex}</div>`;

  card.addEventListener("click", () => openDetail(r));
  return card;
}

// 상세 패널
function openDetail(r) {
  const feats = state.data.scoreFeatures || [];
  const code = rowCode(r);
  const chTag = (showChannels() && (r.channel === "B" || r.channel === "W"))
    ? ` <span class="badge" style="background:${channelColor(r.channel)};display:inline-block;min-width:auto;padding:3px 10px;font-size:12px">${esc(channelLabel(r.channel))}</span>` : "";
  let html = `<h2 style="margin-bottom:10px">${esc(r.fileName)} · Region #${r.regionIndex}`
    + ` <span class="badge" style="background:${codeColor(code)};display:inline-block;min-width:auto;padding:3px 10px;font-size:12px">${esc(code)}</span>${chTag}</h2>`;
  if (r.binPath)
    html += `<img loading="lazy" src="${imgUrl(r.binPath)}" alt="이진화" style="margin-bottom:8px">`;
  if (r.overlayPath)
    html += `<img loading="lazy" src="${imgUrl(r.overlayPath)}" alt="오버레이">`;

  // mm 환산값(scale≠1.0 일 때만)
  const mmPairs = [["area_mm²", r.areaMm2], ["width_mm", r.widthMm], ["height_mm", r.heightMm], ["diameter_mm", r.diameterMm]];
  const mmHave = scaleApplied() ? mmPairs.filter(([, v]) => typeof v === "number" && Number.isFinite(v)) : [];
  if (mmHave.length) {
    html += `<h2 style="margin:14px 0 6px">mm 환산</h2><table>`;
    mmHave.forEach(([k, v]) => { html += `<tr><td class="k">${esc(k)}</td><td class="v">${v.toFixed(4)}</td></tr>`; });
    html += `</table>`;
  }

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
el("detailClose").addEventListener("click", () => el("detailOverlay").classList.add("hidden"));
el("detailOverlay").addEventListener("click", e => { if (e.target === el("detailOverlay")) el("detailOverlay").classList.add("hidden"); });

// 초기화
syncBinFields();
renderRecent();
updateCsvButton();
loadPresets();
