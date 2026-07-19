// cluster.js - 군집(Cluster) 실행 + 산점도 + hover 이미지 툴팁
"use strict";

// ---------- 군집(Cluster) ----------
async function runCluster() {
  // 폴더: 군집 탭 입력 우선, 비면 설정 탭 폴더 사용
  const folder = (el("clFolder").value.trim() || el("folderPath").value.trim());
  if (!folder) {
    toast("폴더 경로를 입력하세요.", "error");
    setStatus("폴더 경로를 입력하세요.", "error");
    el("clFolder").focus();
    return;
  }
  // 이진화는 설정 탭 값 재사용, 군집 파라미터는 군집 탭 입력
  const s = readSettings();
  const body = {
    folderPath: folder,
    threads: s.threads,
    polarity: s.polarity, mode: s.mode,
    thresh: s.thresh, offset: s.offset,
    kernel: s.kernel, blur: s.blur, response: s.response,
    blackTh: s.blackTh, whiteTh: s.whiteTh, projKernel: s.projKernel,
    features: el("clFeatures").value,
    scaleMode: el("clScale").value,
    k: parseInt(el("clK").value, 10) || 0,
    kMax: parseInt(el("clKMax").value, 10) || 8,
  };

  el("clRunBtn").disabled = true;
  el("clRunBtn").textContent = "군집화 중...";
  setStatus("군집화 실행 중... (특징 추출 + kmeans)", "busy");
  try {
    const res = await fetch("/api/cluster", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    let data;
    try { data = await res.json(); }
    catch (e) { showError(`서버 응답 파싱 실패 (HTTP ${res.status})`); return; }
    if (!data.ok) {
      if (data.stderr) console.error("[GlimRegion] cluster exe stderr:\n" + data.stderr);
      showError(data.error || `군집화 실패 (HTTP ${res.status})`);
      return;
    }
    state.cluster = data;
    state.clusterFocus = null;   // 새 군집 실행 시 강조 초기화
    renderClusterAxes();
    renderClusterScatter();
    renderClusterMembers();
    el("clResultCard").style.display = "";
    el("clusterEmpty").style.display = "none";
    setStatus(`군집 완료 · K=${data.k} · silhouette=${fmt(data.silhouette, 3)} · ${data.points.length} 점`);
    toast(`군집 완료 · K=${data.k} · ${data.points.length} 점`, "success");
  } catch (e) {
    showError("군집 요청 실패: " + e + " (서버가 실행 중인지 확인하세요)");
  } finally {
    el("clRunBtn").disabled = false;
    el("clRunBtn").textContent = "군집화 실행";
  }
}

// 축 콤보 채우기(columns = 사용된 feature). 기본 X=첫째, Y=둘째.
function renderClusterAxes() {
  const cols = (state.cluster && state.cluster.columns) || [];
  const xs = el("clXAxis"), ys = el("clYAxis");
  if (!xs || !ys) return;
  const opts = cols.map((c, i) => `<option value="${i}" title="${esc(featureTip(c))}">${esc(c)}</option>`).join("");
  xs.innerHTML = opts;
  ys.innerHTML = opts;
  xs.value = "0";
  ys.value = cols.length > 1 ? "1" : "0";
}

// raw 2축 산점도(SVG). 점 색 = cluster 라벨. 축 변경 시 재요청 없이 이 함수만 재호출.
function renderClusterScatter() {
  const box = el("clScatter");
  const cl = state.cluster;
  if (!cl || !cl.points || !cl.points.length) { box.innerHTML = '<div class="empty">데이터 없음</div>'; return; }
  const xi = parseInt(el("clXAxis").value, 10) || 0;
  const yi = parseInt(el("clYAxis").value, 10) || 0;

  // 유한값 점만 사용
  const pts = cl.points.filter(p => {
    const vx = p.values[xi], vy = p.values[yi];
    return typeof vx === "number" && isFinite(vx) && typeof vy === "number" && isFinite(vy);
  });
  if (!pts.length) { box.innerHTML = '<div class="empty">선택한 축에 표시할 값이 없습니다.</div>'; return; }

  let xmin = Infinity, xmax = -Infinity, ymin = Infinity, ymax = -Infinity;
  pts.forEach(p => {
    const vx = p.values[xi], vy = p.values[yi];
    if (vx < xmin) xmin = vx; if (vx > xmax) xmax = vx;
    if (vy < ymin) ymin = vy; if (vy > ymax) ymax = vy;
  });
  if (xmin === xmax) { xmax = xmin + 1; xmin -= 1; }
  if (ymin === ymax) { ymax = ymin + 1; ymin -= 1; }
  // 약간의 여백
  const xpad = (xmax - xmin) * 0.04, ypad = (ymax - ymin) * 0.04;
  xmin -= xpad; xmax += xpad; ymin -= ypad; ymax += ypad;

  const W = 620, H = 420, leftPad = 56, rightPad = 16, topPad = 14, bottomPad = 42;
  const plotW = W - leftPad - rightPad, plotH = H - topPad - bottomPad;
  const sx = v => leftPad + ((v - xmin) / (xmax - xmin)) * plotW;
  const sy = v => topPad + plotH - ((v - ymin) / (ymax - ymin)) * plotH;

  let svg = `<svg width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">`;
  // 축 프레임
  svg += `<rect x="${leftPad}" y="${topPad}" width="${plotW}" height="${plotH}" fill="none" stroke="#2e3646"></rect>`;
  // 그리드 + 눈금(5분할)
  const TICKS = 5;
  for (let t = 0; t <= TICKS; t++) {
    const gx = leftPad + (t / TICKS) * plotW;
    const vx = xmin + (t / TICKS) * (xmax - xmin);
    svg += `<line x1="${gx}" y1="${topPad}" x2="${gx}" y2="${topPad + plotH}" stroke="#232a36"></line>`;
    svg += `<text x="${gx}" y="${topPad + plotH + 16}" text-anchor="middle" fill="#9aa3b2" font-size="10">${vx.toFixed(Math.abs(xmax) < 10 ? 2 : 0)}</text>`;
    const gy = topPad + plotH - (t / TICKS) * plotH;
    const vy = ymin + (t / TICKS) * (ymax - ymin);
    svg += `<line x1="${leftPad}" y1="${gy}" x2="${leftPad + plotW}" y2="${gy}" stroke="#232a36"></line>`;
    svg += `<text x="${leftPad - 8}" y="${gy + 4}" text-anchor="end" fill="#9aa3b2" font-size="10">${vy.toFixed(Math.abs(ymax) < 10 ? 2 : 0)}</text>`;
  }
  // 축 라벨
  const cols = cl.columns || [];
  svg += `<text x="${leftPad + plotW / 2}" y="${H - 6}" text-anchor="middle" fill="#e6e9ef" font-size="11">${esc(cols[xi] || "")}</text>`;
  svg += `<text x="14" y="${topPad + plotH / 2}" text-anchor="middle" fill="#e6e9ef" font-size="11" transform="rotate(-90 14 ${topPad + plotH / 2})">${esc(cols[yi] || "")}</text>`;
  // 점 (보이는 점 + 넓은 투명 히트영역: hover 시 이미지 툴팁)
  //  선택 클러스터(state.clusterFocus)가 있으면 그 군집만 강조, 나머지는 흐림.
  const focus = (state.clusterFocus == null) ? null : state.clusterFocus;
  pts.forEach(p => {
    const cx = sx(p.values[xi]), cy = sy(p.values[yi]);
    const dim = (focus !== null && p.cluster !== focus);
    const col = dim ? "#5a6b60" : clusterColor(p.cluster);
    const r = dim ? 2.0 : (focus !== null ? 4.6 : 3.2);
    const op = dim ? 0.18 : 0.9;
    svg += `<circle cx="${cx.toFixed(1)}" cy="${cy.toFixed(1)}" r="${r}" fill="${col}" fill-opacity="${op}" pointer-events="none"></circle>`;
    svg += `<circle class="cl-hit" cx="${cx.toFixed(1)}" cy="${cy.toFixed(1)}" r="8" fill="transparent"`
      + ` data-path="${esc(p.path || "")}" data-file="${esc(p.file)}"`
      + ` data-region="${p.regionIndex}" data-channel="${esc(p.channel || "")}" data-cluster="${p.cluster}"></circle>`;
  });
  svg += `</svg>`;
  box.innerHTML = svg;

  attachClusterHover(box);
  renderClusterLegend();
}

// 산점도 hover 이미지 툴팁(고정 위치 div, /api/image 로 크롭 로드)
function ensureClusterTooltip() {
  let tt = el("clHoverTip");
  if (!tt) {
    tt = document.createElement("div");
    tt.id = "clHoverTip";
    tt.className = "cl-hovertip hidden";
    tt.innerHTML = '<img alt=""><div class="cl-tipcap"></div>';
    document.body.appendChild(tt);
  }
  return tt;
}

function attachClusterHover(box) {
  const svg = box.querySelector("svg");
  if (!svg) return;
  const tt = ensureClusterTooltip();
  const img = tt.querySelector("img");
  const cap = tt.querySelector(".cl-tipcap");

  const moveTip = (e) => {
    const pad = 16;
    const w = tt.offsetWidth || 180, h = tt.offsetHeight || 200;
    let x = e.clientX + pad, y = e.clientY + pad;
    if (x + w > window.innerWidth) x = e.clientX - w - pad;
    if (y + h > window.innerHeight) y = e.clientY - h - pad;
    tt.style.left = Math.max(4, x) + "px";
    tt.style.top = Math.max(4, y) + "px";
  };

  svg.addEventListener("mouseover", (e) => {
    const t = e.target;
    if (!t.classList || !t.classList.contains("cl-hit")) return;
    const path = t.getAttribute("data-path");
    const file = t.getAttribute("data-file") || "";
    const region = t.getAttribute("data-region");
    const channel = t.getAttribute("data-channel") || "";
    const cluster = t.getAttribute("data-cluster");
    if (path) img.src = "/api/image?path=" + encodeURIComponent(path);
    else img.removeAttribute("src");
    cap.textContent = `${file} #${region}${channel ? " · " + channel : ""} · cluster ${cluster}`;
    tt.classList.remove("hidden");
    moveTip(e);
  });
  svg.addEventListener("mousemove", (e) => {
    if (!tt.classList.contains("hidden")) moveTip(e);
  });
  svg.addEventListener("mouseout", (e) => {
    const t = e.target;
    if (t.classList && t.classList.contains("cl-hit")) tt.classList.add("hidden");
  });
}

function renderClusterLegend() {
  const box = el("clLegend");
  const cl = state.cluster;
  if (!box || !cl) return;
  const sizes = cl.clusterSizes || [];
  const focus = state.clusterFocus;
  let html = `<span class="lg-item" style="font-weight:700">K = ${cl.k}</span>`;
  html += `<span class="lg-item">Silhouette = ${fmt(cl.silhouette, 3)}</span>`;
  html += `<span class="lg-item lg-click${focus == null ? " lg-sel" : ""}" data-cluster="all">전체</span>`;
  for (let c = 0; c < cl.k; c++) {
    const n = (c < sizes.length) ? sizes[c] : 0;
    const sel = (focus === c) ? " lg-sel" : "";
    html += `<span class="lg-item lg-click${sel}" data-cluster="${c}"><span class="lg-dot" style="background:${clusterColor(c)}"></span>Cluster ${c} (n=${n})</span>`;
  }
  box.innerHTML = html;
  // 범례 클릭 → 해당 클러스터 강조(재클릭/전체로 해제)
  box.querySelectorAll(".lg-click").forEach(item => item.addEventListener("click", () => {
    const v = item.getAttribute("data-cluster");
    const cid = (v === "all") ? null : parseInt(v, 10);
    state.clusterFocus = (state.clusterFocus === cid) ? null : cid; // 같은 것 재클릭 시 해제
    renderClusterScatter();   // 산점도 + 범례 재렌더
    renderClusterMembers();
  }));
}

// 선택 클러스터의 멤버 크롭 이미지 갤러리(/api/image). 강조 없으면 비움.
function renderClusterMembers() {
  const box = el("clMembers");
  if (!box) return;
  const cl = state.cluster, focus = state.clusterFocus;
  if (!cl || focus == null) { box.innerHTML = ""; return; }
  const members = cl.points.filter(p => p.cluster === focus);
  const MAX = 80;
  const imgs = members.slice(0, MAX).map(p => p.path
    ? `<figure class="cl-mem"><img src="/api/image?path=${encodeURIComponent(p.path)}" alt=""`
      + ` title="${esc(p.file)} #${p.regionIndex}${p.channel ? " · " + esc(p.channel) : ""}">`
      + `<figcaption>${esc(p.file)}</figcaption></figure>`
    : "").join("");
  const more = members.length > MAX ? `<div class="hint">…외 ${members.length - MAX}개</div>` : "";
  box.innerHTML = `<div class="cl-mem-head" style="color:${clusterColor(focus)}">Cluster ${focus} · ${members.length} regions `
    + `<span class="hint">(멤버 크롭 · 범례에서 다른 군집 선택/‘전체’로 해제)</span></div>`
    + `<div class="cl-mem-grid">${imgs}</div>${more}`;
}
