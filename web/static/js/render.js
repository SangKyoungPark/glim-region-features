// render.js - 결과/분석 탭 렌더(요약/분포/Score/히스토그램/필터/갤러리/상세)
"use strict";

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

  // 0 region / 과분할 경고
  const box = el("resultWarn");
  if (box) {
    const imgs = s.totalImages || 0, regs = s.totalRegions || 0;
    const avg = imgs ? regs / imgs : 0;
    let w = "";
    if (regs === 0)
      w = "⚠ 검출된 Region 0개 — 이진화 방식/극성을 바꿔보세요 (설정 탭). 어두운 불량이면 <b>Dark</b>, 이미 이진화된 크롭이면 <b>Binary</b>, 밝은 불량이면 <b>Bright</b>.";
    else if (avg > 20)
      w = `⚠ 과분할 의심 — 이미지 ${imgs}장에서 Region <b>${regs.toLocaleString()}개</b> (평균 ${avg.toFixed(0)}/장). 텍스처 노이즈까지 잡혔을 수 있어요. 이진화 임계/극성을 조정하거나, 이 크롭이 이미 이진화면 <b>Binary</b> 방식을 쓰세요. (결과가 많아 갤러리는 상위 일부만 표시)`;
    box.innerHTML = w ? `<div class="rw-msg">${w}</div>` : "";
  }
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
  // 렌더 상한: 카드가 수만 개면 브라우저가 멈추므로 상위 CAP 개만 렌더.
  const CAP = 300;
  if (rows.length > CAP) {
    const note = document.createElement("div");
    note.className = "gallery-cap-note";
    note.innerHTML = `표시 제한: 총 <b>${rows.length.toLocaleString()}개</b> 중 상위 ${CAP}개만 렌더합니다. `
      + `정렬/코드·채널 필터로 좁히거나 <b>CSV 다운로드</b>로 전체를 확인하세요.`;
    gallery.appendChild(note);
  }
  rows.slice(0, CAP).forEach(r => gallery.appendChild(buildCard(r, feats)));
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
    html += `<tr><td class="k" title="${esc(featureTip(k))}">${esc(k)}</td><td class="v">${typeof val === "number" ? val.toFixed(4) : esc(val)}</td></tr>`;
  });
  html += `</table>`;

  el("detailBody").innerHTML = html;
  el("detailOverlay").classList.remove("hidden");
}

// 홈 탭 특징값 용어집(검색 가능). FEATURE_DOCS(feature_docs.js) 기반.
function renderFeatureGlossary(filter) {
  const box = el("featureGlossary");
  if (!box || typeof FEATURE_DOCS === "undefined") return;
  const q = (filter || "").trim().toLowerCase();
  const keys = Object.keys(FEATURE_DOCS).filter(k => {
    if (!q) return true;
    const d = FEATURE_DOCS[k] || {};
    return k.toLowerCase().includes(q) ||
      (d.label || "").toLowerCase().includes(q) ||
      (d.desc || "").toLowerCase().includes(q);
  });
  if (!keys.length) { box.innerHTML = '<div class="empty">일치하는 특징값 없음</div>'; return; }
  const rows = keys.map(k => {
    const d = FEATURE_DOCS[k] || {};
    const hint = d.hint ? ` <span class="g-hint">(${esc(d.hint)})</span>` : "";
    const ex = (typeof featureExample === "function") ? featureExample(k) : "";
    const codeBtn = (typeof featureHasCode === "function" && featureHasCode(k))
      ? `<button class="g-code-btn" data-feat="${esc(k)}" title="수식 + OpenCV 코드">&lt;/&gt;</button>` : "";
    return `<tr><td class="g-key">${esc(k)}</td><td class="g-label">${esc(d.label || "")}</td>` +
      `<td class="g-ex">${ex}</td><td class="g-desc">${esc(d.desc || "")}${hint}</td><td class="g-code">${codeBtn}</td></tr>`;
  }).join("");
  box.innerHTML = `<table class="glossary-table"><tr><th>feature</th><th>이름</th><th>예시</th><th>설명</th><th>코드</th></tr>${rows}</table>`;
  box.querySelectorAll(".g-code-btn").forEach(btn => btn.addEventListener("click", () => {
    if (typeof openFeatureCode === "function") openFeatureCode(btn.getAttribute("data-feat"));
  }));
}
