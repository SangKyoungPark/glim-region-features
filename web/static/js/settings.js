// settings.js - 설정 읽기/쓰기 + 이진화 필드 토글
"use strict";

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
