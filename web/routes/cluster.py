# routes/cluster.py - 군집(Cluster) API
#
# 군집 계산은 C++ Batch(cv::kmeans)가 유일 소스. 웹은 라벨 수신 + raw 2축 산점도만.
#  프리셋 "shape8" = 뷰어와 동일한 형상 8종. "all" = 형상 8종 + 크기 파생.
import os
import csv
import json
import time
import hashlib

from fastapi import APIRouter
from fastapi.responses import JSONResponse
from pydantic import BaseModel

import config
import engine
import state
from binarize import build_binarize
from csv_parse import _to_float

router = APIRouter()


class ClusterRequest(BaseModel):
    # 이진화/폴더 파라미터는 RunRequest 와 동일 형태(설정 탭 값 재사용).
    folderPath: str
    threads: int = 0
    polarity: str = "bright"     # bright | dark
    mode: str = ""               # fixed | auto | binary | wrinkle | projection | ""(legacy)
    thresh: int = 127
    offset: int = 20
    kernel: int = 15
    blur: int = 31
    response: int = 4
    blackTh: int = 20
    whiteTh: int = 235
    projKernel: int = 3
    binarize: str = "bright127"
    # 군집 전용 파라미터
    features: str = "shape8"     # 쉼표 문자열 또는 "shape8" / "all" 프리셋
    scaleMode: str = "zscore"    # zscore | minmax | robust
    k: int = 0                   # 0 = auto
    kMax: int = 8


SHAPE8_FEATURES = ["circularity", "compactness", "convexity", "rectangularity",
                   "roundness", "anisometry", "bulkiness", "structure_factor"]
ALL_CLUSTER_FEATURES = (["area", "contlength", "diameter"] + SHAPE8_FEATURES +
                        ["ra", "rb", "roundness_distance", "roundness_sigma"])


def _resolve_features(spec):
    """features 스펙(쉼표 문자열 또는 프리셋) → feature 이름 리스트."""
    s = (spec or "").strip().lower()
    if s in ("", "shape8"):
        return list(SHAPE8_FEATURES)
    if s == "all":
        return list(ALL_CLUSTER_FEATURES)
    feats = [x.strip() for x in str(spec).split(",") if x.strip()]
    return feats or list(SHAPE8_FEATURES)


def _read_cluster_feature_rows(csv_path, feature_cols):
    """CSV 의 Region 행만(순서 보존) 읽어 선택 feature 값 배열을 뽑는다.
    엔진(ClusterCsvIO)이 RegionIndex<0(NO_REGION) 행을 제외하므로 여기서도 동일하게 제외해야
    assignments 와 행 순서가 1:1 로 맞는다(fileName 매칭 아님)."""
    rows = []
    with open(csv_path, "r", newline="", encoding="utf-8", errors="replace") as f:
        reader = csv.DictReader(f)
        for rec in reader:
            try:
                ri = int(float(rec.get("RegionIndex", "")))
            except (TypeError, ValueError):
                ri = -1
            if ri < 0:
                continue  # NO_REGION 행 제외(엔진과 동일)
            vals = [_to_float(rec.get(c, "")) for c in feature_cols]
            rows.append({
                "file": rec.get("FileName", ""),
                "path": (rec.get("FilePath", "") or "").strip(),  # hover 이미지 툴팁용 원본 경로
                "regionIndex": ri,
                "channel": (rec.get("Channel", "") or "").strip(),
                "values": vals,
            })
    return rows


@router.post("/api/cluster")
def api_cluster(req: ClusterRequest):
    folder = req.folderPath.strip().strip('"')
    if not folder or not os.path.isdir(folder):
        return JSONResponse({"ok": False, "error": f"폴더를 찾을 수 없습니다: {folder}"}, status_code=400)
    state.allow_dir(folder)  # hover 이미지 툴팁(/api/image) 서빙 허용

    exe = engine.find_exe()
    if not exe:
        return JSONResponse({"ok": False, "error": "GlimRegionBatch.exe 를 찾을 수 없습니다. 먼저 Release 빌드가 필요합니다."}, status_code=500)

    # 이진화 플래그 + 캐시 라벨(설정 탭 재사용, /api/run 과 동일 규칙)
    bin_flags, bin_label = build_binarize(
        req.polarity, req.mode, req.thresh, req.offset, legacy_key=req.binarize,
        kernel=req.kernel, response=req.response, blur=req.blur,
        black_th=req.blackTh, white_th=req.whiteTh, proj_kernel=req.projKernel)

    features = _resolve_features(req.features)
    scale_mode = (req.scaleMode or "zscore").lower()
    if scale_mode not in ("zscore", "minmax", "robust"):
        scale_mode = "zscore"
    try:
        k = int(req.k)
    except (TypeError, ValueError):
        k = 0
    if k < 0:
        k = 0
    try:
        k_max = int(req.kMax)
    except (TypeError, ValueError):
        k_max = 8
    k_max = max(2, min(12, k_max))

    # 특징 CSV 캐시(폴더+이진화). 군집 결과는 그 안에 군집 파라미터별 파일로 분리.
    csv_key = "cluster|%s|%s" % (state.norm(folder), bin_label)
    h_csv = hashlib.md5(csv_key.encode("utf-8")).hexdigest()[:16]
    out_dir = os.path.join(config.CACHE_DIR, "cluster", h_csv)
    os.makedirs(out_dir, exist_ok=True)
    csv_path = os.path.join(out_dir, "features.csv")

    # 1) 특징 CSV 생성(프로파일 없음 — 군집엔 raw 특징값만 필요). 캐시 있으면 재사용.
    if not os.path.isfile(csv_path):
        args = [exe, folder, csv_path]
        args += bin_flags
        if req.threads and req.threads > 0:
            args += ["--threads", str(int(req.threads))]
        try:
            proc = engine.run_batch(args, timeout=600)
        except Exception as e:
            return JSONResponse({"ok": False, "error": f"특징 추출 exe 실행 실패: {e}"}, status_code=500)
        if not os.path.isfile(csv_path):
            return JSONResponse({
                "ok": False, "error": "특징 CSV 가 생성되지 않았습니다.",
                "stdout": proc.stdout, "stderr": proc.stderr,
            }, status_code=500)

    # 2) --cluster 로 clusters.json 생성(군집 계산 단일 소스 = C++)
    clu_key = "%s|%s|k%d|kmax%d" % (",".join(features), scale_mode, k, k_max)
    h_clu = hashlib.md5(clu_key.encode("utf-8")).hexdigest()[:16]
    clusters_json = os.path.join(out_dir, "clusters_%s.json" % h_clu)
    cargs = [exe, "--cluster", csv_path, clusters_json,
             "--features", ",".join(features),
             "--k", (str(k) if k > 0 else "auto"),
             "--kmax", str(k_max),
             "--scale", scale_mode]
    t0 = time.time()
    try:
        cproc = engine.run_batch(cargs, timeout=600)
    except Exception as e:
        return JSONResponse({"ok": False, "error": f"군집화 exe 실행 실패: {e}"}, status_code=500)
    elapsed_ms = int((time.time() - t0) * 1000)

    if not os.path.isfile(clusters_json):
        return JSONResponse({
            "ok": False, "error": "clusters.json 이 생성되지 않았습니다.",
            "stdout": cproc.stdout, "stderr": cproc.stderr,
        }, status_code=500)

    try:
        with open(clusters_json, "r", encoding="utf-8", errors="replace") as f:
            cj = json.load(f)
    except Exception as e:
        return JSONResponse({"ok": False, "error": f"clusters.json 파싱 실패: {e}"}, status_code=500)

    assignments = cj.get("assignments") or []
    labels = []
    for a in assignments:
        try:
            labels.append(int(a.get("cluster", -1)))
        except (TypeError, ValueError):
            labels.append(-1)

    # 실제 사용된 feature = scaler.features(CSV 헤더에 존재하는 것만). 산점도 축 후보.
    scaler = cj.get("scaler") or {}
    used_features = scaler.get("features") or features
    if not isinstance(used_features, list) or not used_features:
        used_features = features

    # CSV Region 행(순서 보존) + assignments 를 행 인덱스로 1:1 병합
    try:
        feat_rows = _read_cluster_feature_rows(csv_path, used_features)
    except Exception as e:
        return JSONResponse({"ok": False, "error": f"특징 CSV 병합 실패: {e}"}, status_code=500)

    n = min(len(feat_rows), len(labels))
    points = []
    for i in range(n):
        r = feat_rows[i]
        points.append({
            "file": r["file"],
            "path": r["path"],
            "regionIndex": r["regionIndex"],
            "channel": r["channel"],
            "cluster": labels[i],
            "values": r["values"],
        })

    cluster_sizes = cj.get("clusterSizes") or []
    result_k = cj.get("k")
    if result_k is None:
        result_k = len(cluster_sizes)
    silhouette = cj.get("silhouette")

    return {
        "ok": True,
        "columns": used_features,
        "points": points,
        "k": result_k,
        "silhouette": silhouette,
        "clusterSizes": cluster_sizes,
        "features": used_features,
        "scaleMode": scale_mode,
        "elapsedMs": elapsed_ms,
        "rowMismatch": (len(feat_rows) != len(labels)),
    }
