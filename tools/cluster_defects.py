# -*- coding: utf-8 -*-
"""
불량 크롭 이미지 군집화 분석 (GlimRegionFeatures 출력 CSV 기반)

- 입력: <ROOT>\*_result.csv  (GlimRegionFeatures/GlimRegionBatch 가 생성한 region feature CSV)
- 처리: 특징 정제 -> (시각화)RobustScaler -> t-SNE / (군집)log+StandardScaler -> KMeans+DBSCAN
- 출력: <ROOT>\_cluster_analysis\ 에 산점도/썸네일/요약 CSV

사용법:
    python cluster_defects.py [ROOT_DIR]
    (ROOT_DIR 생략 시 D:\128Crop)

시각화와 군집화에 정규화를 분리하는 이유:
    한 가지 스케일링으로는 둘을 동시에 못 잡는다.
    - RobustScaler 는 t-SNE 지도를 깨끗하게 분리하지만 KMeans 가 크기특징에 지배돼 트리비얼(k=2)해진다.
    - 크기특징 log + StandardScaler 는 순수 군집을 주지만 t-SNE 투영이 뭉개진다.
    -> 지도는 RobustScaler, 군집은 log+StandardScaler 로 각각 최적화한 뒤 겹쳐 그린다.
"""
import os, sys, glob, warnings
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import cm
import cv2
from sklearn.preprocessing import RobustScaler, StandardScaler
from sklearn.decomposition import PCA
from sklearn.manifold import TSNE
from sklearn.cluster import KMeans, DBSCAN
from sklearn.metrics import silhouette_score, adjusted_rand_score
warnings.filterwarnings('ignore')

ROOT = sys.argv[1] if len(sys.argv) > 1 else r'D:\128Crop'
OUT  = os.path.join(ROOT, '_cluster_analysis')
os.makedirs(OUT, exist_ok=True)

# ---------- 1. 로드 ----------
csvs = glob.glob(os.path.join(ROOT, '*_result.csv'))
if not csvs:
    sys.exit(f'[error] {ROOT} 에 *_result.csv 가 없습니다.')
frames = []
for f in csvs:
    df = pd.read_csv(f)
    df['folder'] = os.path.basename(f).replace('_result.csv', '')
    frames.append(df)
data = pd.concat(frames, ignore_index=True)
print(f'[load] {len(data)} rows, {data.shape[1]} cols, from {len(csvs)} csv')

# ---------- 2. 특징 선택/정제 ----------
# 위치/각도/깨진(FLT_MAX) 컬럼 제외 - 불량 '종류'와 무관하거나 발산값
exclude = {
    'RegionIndex','center_row','center_col','phi','orientation','rect2_phi',
    'bbox_row1','bbox_col1','bbox_row2','bbox_col2',
    'rect2_center_row','rect2_center_col','smallest_circle_row','smallest_circle_col',
    'inner_circle_row','inner_circle_col',
    'inner_circle_radius','inner_outer_ratio',           # FLT_MAX
    'hu2','hu3','hu4','hu5','hu6',                         # 0/FLT_MAX 로 깨짐
}
num = data.select_dtypes(include=[np.number]).copy()
feat_cols = [c for c in num.columns if c not in exclude]
X = num[feat_cols].replace([np.inf, -np.inf], np.nan)

# 30% 이상 결측 또는 분산 0 컬럼 제거
keep = [c for c in X.columns if X[c].isna().mean() < 0.3 and X[c].std(skipna=True) > 1e-9]
dropped = sorted(set(X.columns) - set(keep))
X = X[keep].fillna(X[keep].median())
print(f'[feature] 사용 {len(keep)}종, 제외 {dropped}')

# ---------- 3. 두 개의 정규화 공간 ----------
n = len(X)
# (A) 시각화용: RobustScaler -> t-SNE 지도가 가장 깨끗하게 분리
X_viz = RobustScaler().fit_transform(X.values)
# (B) 군집화용: 크기특징 log1p + StandardScaler -> 특정 특징 지배 없이 순수 분리
Xlog = X.copy()
size_like = ['area','area_holes','contlength','diameter','ra','rb',
             'rect2_len1','rect2_len2','smallest_circle_radius','roundness_distance']
for c in [c for c in size_like if c in Xlog.columns]:
    Xlog[c] = np.log1p(Xlog[c].clip(lower=0))
X_clu = StandardScaler().fit_transform(Xlog.values)

# ---------- 4. 차원축소 ----------
Xp_viz = PCA(n_components=min(10, X_viz.shape[1]), random_state=0).fit_transform(X_viz)
tsne = TSNE(n_components=2, perplexity=min(15, max(5, n//8)),
            random_state=0, init='pca', learning_rate='auto')
X2 = tsne.fit_transform(Xp_viz)                 # 시각화 좌표
Xc = PCA(n_components=min(10, X_clu.shape[1]), random_state=0).fit_transform(X_clu)  # 군집화 공간

# ---------- 5. 군집화 ----------
folder_code = pd.factorize(data['folder'])[0]
has_code = 'ClassifiedCode' in data.columns
code_code = pd.factorize(data['ClassifiedCode'])[0] if has_code else None

print('  k  | silhouette | ARI(folder) | ARI(code)')
rows = []
for k in range(2, min(9, n-1)):
    lab = KMeans(n_clusters=k, n_init=10, random_state=0).fit_predict(Xc)
    if len(set(lab)) < 2: continue
    s = silhouette_score(Xc, lab)
    ari_f = adjusted_rand_score(folder_code, lab)
    ari_c = adjusted_rand_score(code_code, lab) if has_code else float('nan')
    rows.append((k, s, ari_f, ari_c, lab))
    print(f'  {k}  |   {s:.3f}    |    {ari_f:.3f}    |   {ari_c:.3f}')
# 룰 판정코드가 있으면 그 복원도(ARI code) 최대, 없으면 silhouette 최대로 대표 k 선정
key = (lambda r: r[3]) if has_code else (lambda r: r[1])
best_k, best_s, best_ari, best_aric, km_lab = max(rows, key=key)
print(f'[kmeans] 대표 k={best_k}  silhouette={best_s:.3f}  ARI(folder)={best_ari:.3f}  ARI(code)={best_aric:.3f}')

db_lab, db_eps = None, None
for eps in np.arange(0.8, 4.0, 0.2):
    lab = DBSCAN(eps=eps, min_samples=3).fit_predict(Xc)
    nnoise = int((lab==-1).sum()); nc = len(set(lab))-(1 if -1 in lab else 0)
    if nc >= 2 and nnoise < 0.2*n:
        db_lab, db_eps = lab, eps; break
if db_lab is None:
    db_lab = DBSCAN(eps=2.0, min_samples=3).fit_predict(Xc); db_eps=2.0
print(f'[dbscan] eps={db_eps:.1f} 군집 {len(set(db_lab))-(1 if -1 in db_lab else 0)}개, '
      f'노이즈 {int((db_lab==-1).sum())}개')

data['cluster_km'] = km_lab
data['cluster_db'] = db_lab
data['tsne_x'] = X2[:,0]; data['tsne_y'] = X2[:,1]

# ---------- 6. 산점도 (군집 / 폴더 / 코드) ----------
def scatter(ax, labels, title):
    uq = sorted(pd.unique(labels), key=lambda v: str(v))
    colors = cm.tab10(np.linspace(0,1,max(len(uq),3)))
    for c,u in zip(colors, uq):
        m = labels==u
        ax.scatter(X2[m,0], X2[m,1], s=45, color=c, label=str(u),
                   edgecolor='k', linewidth=0.4, alpha=0.85)
    ax.set_title(title, fontsize=12, fontweight='bold')
    ax.legend(fontsize=8, markerscale=0.9, framealpha=0.9)
    ax.set_xticks([]); ax.set_yticks([])

npanel = 3 if has_code else 2
fig, axes = plt.subplots(1, npanel, figsize=(6*npanel,6))
scatter(axes[0], km_lab, f'KMeans (k={best_k}, ARIfolder={best_ari:.2f}, ARIcode={best_aric:.2f})')
scatter(axes[1], data['folder'].values, 'Folder (input label)')
if has_code:
    scatter(axes[2], data['ClassifiedCode'].values, 'ClassifiedCode (rule output)')
fig.suptitle('Defect Region-Feature Clustering  (t-SNE 2D, n=%d)'%n, fontsize=14, fontweight='bold')
fig.tight_layout(rect=[0,0,1,0.96])
p1 = os.path.join(OUT, 'cluster_scatter.png'); fig.savefig(p1, dpi=130); plt.close(fig)
print(f'[save] {p1}')

# ---------- 7. 군집별 대표 썸네일 ----------
def load_thumb(path, sz=96):
    img = cv2.imread(path, cv2.IMREAD_COLOR)
    if img is None: return np.zeros((sz,sz,3), np.uint8)
    return cv2.resize(cv2.cvtColor(img, cv2.COLOR_BGR2RGB), (sz,sz))

path_col = 'FilePath' if 'FilePath' in data.columns else None
if path_col:
    clusters = sorted(set(km_lab)); per = 5
    fig, axes = plt.subplots(len(clusters), per, figsize=(per*1.7, len(clusters)*1.7))
    if len(clusters)==1: axes = axes[None,:]
    for r,cl in enumerate(clusters):
        sub = data[data['cluster_km']==cl]
        cen = X2[km_lab==cl].mean(0)
        order = sub.index[np.argsort(np.linalg.norm(X2[km_lab==cl]-cen, axis=1))]
        for c in range(per):
            ax = axes[r,c]; ax.axis('off')
            if c < len(order):
                row = data.loc[order[c]]
                ax.imshow(load_thumb(row[path_col]))
                if c==0:
                    ax.set_title(f'C{cl} (n={len(sub)})\n{str(row["folder"])[:10]}',
                                 fontsize=8, loc='left')
    fig.suptitle('Representative images per KMeans cluster', fontsize=13, fontweight='bold')
    fig.tight_layout(rect=[0,0,1,0.97])
    p2 = os.path.join(OUT, 'cluster_thumbnails.png'); fig.savefig(p2, dpi=130); plt.close(fig)
    print(f'[save] {p2}')

# ---------- 8. 교차표 + 요약 CSV ----------
print('\n=== 군집 vs 폴더 ==='); print(pd.crosstab(data['cluster_km'], data['folder']))
if has_code:
    print('\n=== 군집 vs ClassifiedCode ==='); print(pd.crosstab(data['cluster_km'], data['ClassifiedCode']))
cols = [c for c in ['FileName','folder','ClassifiedCode','cluster_km','cluster_db','tsne_x','tsne_y'] if c in data.columns]
p3 = os.path.join(OUT, 'cluster_summary.csv')
data[cols].to_csv(p3, index=False, encoding='utf-8-sig')
print(f'\n[save] {p3}\nDONE')
