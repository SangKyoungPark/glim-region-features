# csv_parse.py - GlimRegionBatch 결과 CSV 파싱 + 집계
#
# _parse_csv: 결과 CSV → rows/columns/score_features (카드 갤러리·DB 저장용)
# _summarize: rows → 요약/불량분포/코드별 score 평균 (분석 탭용)
# 부가: _to_float(안전 변환), _bin_png_path(--dumpbin 이진화 PNG 경로 탐색)
import os
import csv
from collections import defaultdict


def _to_float(s):
    try:
        return float(s)
    except (TypeError, ValueError):
        return None


def _bin_png_path(bin_dir, file_name, channel):
    """--dumpbin 결과 이진화 PNG 경로 후보 중 실제 존재하는 것 반환(없으면 "").
    projection: <이름>_bin_B.png / _bin_W.png, 그 외 단일: <이름>_bin.png.
    엔진 명명 편차 대비로 fileName(확장자 포함)·stem 두 후보를 모두 확인한다."""
    if not bin_dir or not file_name:
        return ""
    stem = os.path.splitext(file_name)[0]
    suffixes = []
    ch = (channel or "").upper()
    if ch in ("B", "W"):
        suffixes += ["_bin_%s.png" % ch]
    suffixes += ["_bin.png"]
    for base in (file_name, stem):
        for suf in suffixes:
            cand = os.path.join(bin_dir, base + suf)
            if os.path.isfile(cand):
                return cand
    return ""


def _parse_csv(csv_path, overlay_dir, bin_dir=None):
    """CSV → (rows, columns, score_features)."""
    rows = []
    columns = []
    score_features = []
    with open(csv_path, "r", newline="", encoding="utf-8", errors="replace") as f:
        reader = csv.DictReader(f)
        columns = reader.fieldnames or []
        score_features = [c[len("score_"):] for c in columns if c.startswith("score_")]
        for rec in reader:
            region_index = rec.get("RegionIndex", "")
            try:
                ri = int(float(region_index))
            except (TypeError, ValueError):
                ri = -1

            file_name = rec.get("FileName", "")
            file_path = rec.get("FilePath", "")
            channel = (rec.get("Channel", "") or "").strip()
            overlay_path = os.path.join(overlay_dir, file_name + "_ov.png") if file_name else ""
            bin_path = _bin_png_path(bin_dir, file_name, channel) if ri >= 0 else ""

            scores = {}
            for feat in score_features:
                v = _to_float(rec.get("score_" + feat, ""))
                if v is not None:
                    scores[feat] = v

            # 전체 원시 특징값(비어있지 않은 값만) — 상세 패널용
            raw = {}
            for c in columns:
                val = rec.get(c, "")
                if val is None or val == "":
                    continue
                fv = _to_float(val)
                raw[c] = fv if fv is not None else val

            rows.append({
                "fileName": file_name,
                "filePath": file_path,
                "overlayPath": overlay_path,
                "binPath": bin_path,
                "channel": channel,
                "regionIndex": ri,
                "area": _to_float(rec.get("area", "")),
                "circularity": _to_float(rec.get("circularity", "")),
                "roundness": _to_float(rec.get("roundness", "")),
                "anisometry": _to_float(rec.get("anisometry", "")),
                "convexity": _to_float(rec.get("convexity", "")),
                "contlength": _to_float(rec.get("contlength", "")),
                "diameter": _to_float(rec.get("diameter", "")),
                # mm 파생 컬럼(--scale-x/--scale-y 제공 시에만 CSV 에 존재)
                "areaMm2": _to_float(rec.get("area_mm2", "")),
                "widthMm": _to_float(rec.get("width_mm", "")),
                "heightMm": _to_float(rec.get("height_mm", "")),
                "diameterMm": _to_float(rec.get("diameter_mm", "")),
                "classifiedCode": rec.get("ClassifiedCode") if "ClassifiedCode" in columns else None,
                "scores": scores,
                "raw": raw,
            })
    return rows, columns, score_features


def _summarize(rows, score_features):
    total_regions = sum(1 for r in rows if r["regionIndex"] >= 0)
    no_region = sum(1 for r in rows if r["regionIndex"] < 0)
    image_names = set(r["fileName"] for r in rows)

    # 불량 분포(ClassifiedCode 별 카운트) — Region 행만
    dist = defaultdict(int)
    for r in rows:
        if r["regionIndex"] < 0:
            code = "NO_REGION"
        else:
            code = r.get("classifiedCode") or "(none)"
        dist[code] += 1
    dist_list = [{"code": k, "count": v} for k, v in
                 sorted(dist.items(), key=lambda kv: (-kv[1], kv[0]))]

    # 분류코드별 score_* 평균
    acc = defaultdict(lambda: defaultdict(lambda: [0.0, 0]))  # code -> feat -> [sum,count]
    for r in rows:
        if r["regionIndex"] < 0:
            continue
        code = r.get("classifiedCode") or "(none)"
        for feat, v in r["scores"].items():
            acc[code][feat][0] += v
            acc[code][feat][1] += 1
    score_by_code = {}
    for code, feats in acc.items():
        score_by_code[code] = {
            feat: (s / c if c else 0.0) for feat, (s, c) in feats.items()
        }

    return {
        "summary": {
            "totalImages": len(image_names),
            "totalRegions": total_regions,
            "noRegionCount": no_region,
        },
        "defectDistribution": dist_list,
        "scoreByCode": score_by_code,
    }
