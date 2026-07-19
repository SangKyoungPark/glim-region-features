// presets.js - 설정 프리셋 (서버 web/presets.json)
"use strict";

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
