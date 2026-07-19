// preview.js - 이진화 미리보기
"use strict";

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
