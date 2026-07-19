# -*- coding: utf-8 -*-
# compare.py
# 우리 엔진(ours.csv) vs 할콘(halcon.csv) vs 해석적 정답(analytic_ref.json) 수치 비교표.
#  - halcon.csv 가 없으면 우리 vs 정답만(드라이런). HDevelop 실행 후 다시 돌리면 3자 비교.
#  - 출력: 콘솔 표 + comparison.csv(롱포맷) + comparison.png(오차% 색상 표).
import os
import csv
import json
import math

HERE = os.path.dirname(os.path.abspath(__file__))
OURS = os.path.join(HERE, "ours.csv")
HAL = os.path.join(HERE, "halcon.csv")
REF = os.path.join(HERE, "analytic_ref.json")

# 비교 대상 feature (할콘 직접 오퍼레이터 존재하는 형상/크기값)
FEATURES = ["area", "circularity", "convexity", "rectangularity", "compactness",
            "roundness", "sides", "ra", "rb", "anisometry", "bulkiness",
            "structure_factor", "contlength", "diameter",
            "connect_components", "holes", "euler_number"]
ANGLE = ["phi", "orientation"]  # 규약 차이(할콘 CCW 등) 있어 참고 표기만


def load_csv(path):
    if not os.path.isfile(path):
        return {}
    out = {}
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for r in csv.DictReader(f):
            out[r.get("FileName", "").strip()] = r
    return out


def fnum(d, k):
    try:
        v = d.get(k, "")
        return float(v) if v not in (None, "") else None
    except (TypeError, ValueError):
        return None


def errpct(a, b):
    # a 기준(할콘) 상대오차 %. 할콘 없으면 정답 기준.
    if a is None or b is None:
        return None
    denom = abs(a) if abs(a) > 1e-9 else 1.0
    return abs(a - b) / denom * 100.0


def main():
    ours = load_csv(OURS)
    hal = load_csv(HAL)
    ref = json.load(open(REF, encoding="utf-8")) if os.path.isfile(REF) else {}
    have_hal = len(hal) > 0

    files = sorted(ours.keys())
    rows_out = []  # 롱포맷 CSV
    print("=" * 100)
    print("할콘 vs 우리 엔진 Region Features 수치 비교" + ("" if have_hal else "  [halcon.csv 없음 → 우리 vs 정답 드라이런]"))
    print("=" * 100)

    for fn in files:
        base = os.path.splitext(fn)[0]
        analytic = ref.get(base, {})
        o = ours.get(fn, {})
        h = hal.get(fn, {}) if have_hal else {}
        print("\n■ %s" % fn)
        hdr = "  %-20s %12s %12s %12s %10s" % ("feature", "정답", "할콘", "우리", "오차%")
        print(hdr)
        print("  " + "-" * 78)
        for feat in FEATURES:
            av = analytic.get(feat)
            ov = fnum(o, feat)
            hv = fnum(h, feat) if have_hal else None
            # 오차 기준: 할콘 있으면 할콘 vs 우리, 없으면 정답 vs 우리
            base_val = hv if have_hal else av
            ep = errpct(base_val, ov)
            def fmt(x): return ("%12.4f" % x) if isinstance(x, (int, float)) else ("%12s" % ("-" if x is None else str(x)))
            print("  %-20s %s %s %s %10s" % (
                feat, fmt(av), fmt(hv), fmt(ov),
                ("-" if ep is None else "%.2f" % ep)))
            rows_out.append({"file": fn, "feature": feat,
                             "analytic": av, "halcon": hv, "ours": ov,
                             "err_pct_vs_" + ("halcon" if have_hal else "analytic"): ep})
        # 각도(참고)
        for feat in ANGLE:
            ov = fnum(o, feat)
            hv = fnum(h, feat) if have_hal else None
            if ov is not None or hv is not None:
                print("  %-20s %12s %s %s %10s" % (
                    feat + "(rad,참고)", "-",
                    ("%12.4f" % hv) if hv is not None else "%12s" % "-",
                    ("%12.4f" % ov) if ov is not None else "%12s" % "-", ""))

    # 롱포맷 CSV 저장
    with open(os.path.join(HERE, "comparison.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows_out[0].keys()))
        w.writeheader()
        w.writerows(rows_out)
    print("\ncomparison.csv 저장 완료")

    # 시각화(오차% 색상 표) — 할콘 있을 때만 의미
    try:
        render_png(files, ours, hal, ref, have_hal)
    except Exception as e:
        print("PNG 렌더 건너뜀:", e)


def render_png(files, ours, hal, ref, have_hal):
    import numpy as np
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    plt.rcParams["font.family"] = "Malgun Gothic"
    plt.rcParams["axes.unicode_minus"] = False

    feats = FEATURES
    n_f, n_file = len(feats), len(files)
    err = np.full((n_f, n_file), np.nan)
    txt = [["" for _ in files] for _ in feats]
    for j, fn in enumerate(files):
        base = os.path.splitext(fn)[0]
        analytic = ref.get(base, {})
        for i, feat in enumerate(feats):
            ov = fnum(ours.get(fn, {}), feat)
            hv = fnum(hal.get(fn, {}), feat) if have_hal else None
            base_val = hv if have_hal else analytic.get(feat)
            ep = errpct(base_val, ov)
            if ep is not None:
                err[i, j] = ep
            cells = []
            if base_val is not None: cells.append("%.2f" % base_val)
            if ov is not None: cells.append("%.2f" % ov)
            txt[i][j] = " / ".join(cells) + ("\n%.1f%%" % ep if ep is not None else "")

    fig, ax = plt.subplots(figsize=(2.2 + n_file * 2.0, 1.2 + n_f * 0.42))
    cmap = plt.get_cmap("RdYlGn_r")
    disp = np.clip(err, 0, 10)  # 0~10% 색상
    im = ax.imshow(disp, cmap=cmap, vmin=0, vmax=10, aspect="auto")
    ax.set_xticks(range(n_file)); ax.set_xticklabels([os.path.splitext(f)[0] for f in files], fontsize=8)
    ax.set_yticks(range(n_f)); ax.set_yticklabels(feats, fontsize=8)
    for i in range(n_f):
        for j in range(n_file):
            ax.text(j, i, txt[i][j], ha="center", va="center", fontsize=6.5, color="black")
    title = "할콘(기준) vs 우리 · 셀=기준/우리, 색=오차%" if have_hal else "정답(기준) vs 우리 [할콘 미실행]"
    ax.set_title(title, fontsize=11, fontweight="bold")
    cb = fig.colorbar(im, ax=ax, fraction=0.025); cb.set_label("오차 %")
    plt.tight_layout()
    out = os.path.join(HERE, "comparison.png")
    plt.savefig(out, dpi=130, bbox_inches="tight")
    print("comparison.png 저장:", out)


if __name__ == "__main__":
    main()
