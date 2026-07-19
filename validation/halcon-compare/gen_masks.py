# -*- coding: utf-8 -*-
# gen_masks.py
# 할콘 vs 우리 엔진 수치 비교용 "동일 이진 마스크" 생성.
#  - 합성 도형(원/타원/사각)은 해석적 정답값(analytic)을 함께 기록 → 할콘/우리 둘 다 정답 대비 평가 가능.
#  - 8bit 0/255 PNG. 두 엔진이 "완전히 같은 Region 픽셀"을 보게 하여 이진화 차이를 배제한다.
import os
import json
import math
import numpy as np
import cv2

HERE = os.path.dirname(os.path.abspath(__file__))
MASK_DIR = os.path.join(HERE, "masks")
os.makedirs(MASK_DIR, exist_ok=True)

W = H = 300
refs = {}


def save(name, img, analytic):
    cv2.imwrite(os.path.join(MASK_DIR, name + ".png"), img)
    refs[name] = analytic


# 1) 원 r=60
img = np.zeros((H, W), np.uint8)
cv2.circle(img, (150, 150), 60, 255, -1)
save("circle_r60", img, {
    "area": math.pi * 60 ** 2,
    "circularity": 1.0, "convexity": 1.0, "anisometry": 1.0,
    "ra": 60.0, "rb": 60.0, "roundness": 1.0, "compactness": 1.0,
    "contlength": 2 * math.pi * 60, "diameter": 120.0,
    "rectangularity": math.pi / 4,
})

# 2) 타원 a=50, b=20, 30도
img = np.zeros((H, W), np.uint8)
cv2.ellipse(img, (150, 150), (50, 20), 30, 0, 360, 255, -1)
save("ellipse_50_20_a30", img, {
    "area": math.pi * 50 * 20,
    "ra": 50.0, "rb": 20.0, "anisometry": 2.5,
    "diameter": 100.0, "phi_deg": 30.0,
    "contlength": math.pi * (3 * (50 + 20) - math.sqrt((3 * 50 + 20) * (50 + 3 * 20))),
})

# 3) 사각형 120 x 60
img = np.zeros((H, W), np.uint8)
cv2.rectangle(img, (90, 120), (209, 179), 255, -1)  # 120w x 60h (inclusive)
save("rect_120x60", img, {
    "area": 120 * 60,
    "rectangularity": 1.0, "convexity": 1.0, "anisometry": 2.0,
    "contlength": 2 * (120 + 60), "diameter": math.sqrt(120 ** 2 + 60 ** 2),
})

# 4) 도넛(구멍 1개) — euler/holes 검증
img = np.zeros((H, W), np.uint8)
cv2.circle(img, (150, 150), 70, 255, -1)
cv2.circle(img, (150, 150), 30, 0, -1)
save("annulus_70_30", img, {
    "area": math.pi * (70 ** 2 - 30 ** 2),
    "holes": 1, "euler_number": 0, "connect_components": 1,
})

with open(os.path.join(HERE, "analytic_ref.json"), "w", encoding="utf-8") as f:
    json.dump(refs, f, ensure_ascii=False, indent=2)

print("생성한 마스크:", ", ".join(sorted(refs.keys())))
print("mask dir:", MASK_DIR)
print("analytic_ref.json 저장 완료")
