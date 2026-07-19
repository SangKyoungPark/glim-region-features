// tabs.js - 탭 전환
"use strict";

// ---------- 탭 전환 ----------
function showTab(name) {
  document.querySelectorAll(".tab-btn").forEach(b =>
    b.classList.toggle("active", b.getAttribute("data-tab") === name));
  document.querySelectorAll(".tab-page").forEach(p =>
    p.classList.toggle("active", p.id === "tab-" + name));
  if (name === "home") renderRecent();
  if (name === "cluster") {
    // 군집 탭 폴더가 비어 있으면 설정 탭 폴더를 미리 채움(사용자 재입력 편의)
    const clf = el("clFolder");
    if (clf && !clf.value.trim()) clf.value = el("folderPath").value.trim();
  }
  if (name === "history") loadHistory();
}
