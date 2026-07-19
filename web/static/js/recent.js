// recent.js - 최근 실행 기록 (localStorage)
"use strict";

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
