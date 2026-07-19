// run.js - 분석 실행 + CSV 다운로드
"use strict";

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
