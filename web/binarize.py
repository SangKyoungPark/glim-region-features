# binarize.py - granular 이진화 파라미터 → GlimRegionBatch CLI 플래그 + 캐시 라벨
#
# 설정 탭의 극성/방식/임계값 등을 exe 인자로 변환한다. mode 가 비면 legacy binarize 키로 폴백.
# /api/run, /api/preview, /api/cluster 가 동일 규칙을 공유하므로 별도 모듈로 분리.


def build_binarize(polarity, mode, thresh, offset, legacy_key=None, kernel=15, response=4, blur=31,
                   black_th=20, white_th=235, proj_kernel=3):
    """granular 파라미터 → GlimRegionBatch CLI 플래그 + 캐시 라벨. mode 비면 legacy 키 폴백."""
    polarity = (polarity or "").lower()
    mode = (mode or "").lower()
    try:
        thresh = int(thresh)
    except (TypeError, ValueError):
        thresh = 127
    try:
        offset = int(offset)
    except (TypeError, ValueError):
        offset = 20
    try:
        kernel = int(kernel)
    except (TypeError, ValueError):
        kernel = 15
    try:
        response = int(response)
    except (TypeError, ValueError):
        response = 4
    try:
        blur = int(blur)
    except (TypeError, ValueError):
        blur = 31
    try:
        black_th = int(black_th)
    except (TypeError, ValueError):
        black_th = 20
    try:
        white_th = int(white_th)
    except (TypeError, ValueError):
        white_th = 235
    try:
        proj_kernel = int(proj_kernel)
    except (TypeError, ValueError):
        proj_kernel = 3

    if mode not in ("fixed", "auto", "binary", "wrinkle", "projection"):
        lk = (legacy_key or "bright127").lower()
        if lk == "dark127":
            polarity, mode, thresh = "dark", "fixed", 127
        elif lk == "darkauto":
            polarity, mode = "dark", "auto"
        elif lk == "brightauto":
            polarity, mode = "bright", "auto"
        elif lk == "binary":
            polarity, mode = (polarity or "bright"), "binary"
        else:  # bright127
            polarity, mode, thresh = "bright", "fixed", 127

    if polarity not in ("bright", "dark"):
        polarity = "bright"

    # PROJECTION 은 극성과 무관(흑/백 2채널 동시 산출). 검사기 동일 이진화.
    if mode == "projection":
        flags = ["--proj", "--black-th", str(black_th), "--white-th", str(white_th)]
        label = "proj-b%d-w%d" % (black_th, white_th)
        if proj_kernel and proj_kernel > 0:
            flags += ["--proj-kernel", str(proj_kernel)]
            label += "-k%d" % proj_kernel
        return flags, label

    # WRINKLE 은 극성과 무관(항상 어두운 선 검출). 별도 플래그 세트로 구성.
    if mode == "wrinkle":
        kernel = max(1, kernel)
        blur = max(1, blur)
        flags = ["--wrinkle", "--kernel", str(kernel),
                 "--blur", str(blur), "--response", str(response)]
        label = "wrinkle-k%d-b%d-r%d" % (kernel, blur, response)
        return flags, label

    flags = ["--dark"] if polarity == "dark" else ["--bright"]
    if mode == "binary":
        flags += ["--binary"]
        label = "%s-binary" % polarity
    elif mode == "auto":
        flags += ["--thresh", "auto", "--offset", str(offset)]
        label = "%s-auto-off%d" % (polarity, offset)
    else:  # fixed
        thresh = max(0, min(255, thresh))
        flags += ["--thresh", str(thresh)]
        label = "%s-fixed-th%d" % (polarity, thresh)
    return flags, label
