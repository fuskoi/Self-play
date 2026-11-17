#include "algo.h"
#include <string.h>
#include <math.h>

/*
 * 8×8 深度 → 人体连通域分簇
 * - 高度带过滤（安装高度 − 距离）
 * - 可选背景差分（空场标定后）
 * - presence_a/b：双 ToF 存在度，写入簇权重供方向辅助
 */

static float zone_height_m(uint16_t dist_mm, float install_h_m)
{
    float d = dist_mm / 1000.0f;
    if (d < 0.05f || d > install_h_m + 0.5f) {
        return -1.0f;
    }
    return install_h_m - d;
}

int algo_cluster_from_depth(const uint16_t dist_mm[APP_TOF_ZONE_N],
                            float install_h_m,
                            float presence_a,
                            float presence_b,
                            perceive_result_t *out)
{
    memset(out, 0, sizeof(*out));
    bool mask[APP_TOF_ZONE_N] = {0};

    for (int i = 0; i < APP_TOF_ZONE_N; i++) {
        uint16_t d = dist_mm[i];
        if (algo_bg_ready()) {
            uint16_t bg = algo_bg_get(i);
            /* 相对空场更近 ≥80mm 才当前景 */
            if (bg > 0 && d < TOF_INVALID_MM && bg > d + 80) {
                /* ok */
            } else if (bg > 0) {
                continue;
            }
        }
        float h = zone_height_m(d, install_h_m);
        if (h >= APP_HUMAN_H_MIN_M && h <= APP_HUMAN_H_MAX_M) {
            mask[i] = true;
        }
    }

    bool visited[APP_TOF_ZONE_N] = {0};
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int idx = y * 8 + x;
            if (!mask[idx] || visited[idx] || out->n_clusters >= APP_MAX_CLUSTERS) {
                continue;
            }

            int q[APP_TOF_ZONE_N];
            int qh = 0, qt = 0;
            q[qt++] = idx;
            visited[idx] = true;

            float sx = 0, sy = 0, sdepth = 0;
            int cnt = 0;
            int minx = x, maxx = x, miny = y, maxy = y;

            while (qh < qt) {
                int cur = q[qh++];
                int cx = cur % 8;
                int cy = cur / 8;
                sx += (float)cx;
                sy += (float)cy;
                sdepth += dist_mm[cur] / 1000.0f;
                cnt++;
                if (cx < minx) {
                    minx = cx;
                }
                if (cx > maxx) {
                    maxx = cx;
                }
                if (cy < miny) {
                    miny = cy;
                }
                if (cy > maxy) {
                    maxy = cy;
                }

                const int dx[4] = {1, -1, 0, 0};
                const int dy[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                    int nx = cx + dx[k];
                    int ny = cy + dy[k];
                    if (nx < 0 || nx > 7 || ny < 0 || ny > 7) {
                        continue;
                    }
                    int nidx = ny * 8 + nx;
                    if (mask[nidx] && !visited[nidx]) {
                        visited[nidx] = true;
                        q[qt++] = nidx;
                    }
                }
            }

            if (cnt < 2) {
                continue;
            }

            cluster_t *c = &out->clusters[out->n_clusters++];
            c->cx = sx / (float)cnt;
            c->cy = sy / (float)cnt;
            c->width = (float)(maxx - minx + 1);
            c->height = (float)(maxy - miny + 1);
            c->depth_m = sdepth / (float)cnt;
            /* 用全局 A/B 存在度；后续可用 zone 分区细化 */
            c->score_a = presence_a;
            c->score_b = presence_b;
        }
    }

    if (out->n_clusters >= 2) {
        out->multi_suspect = true;
    }
    for (int i = 0; i < out->n_clusters; i++) {
        if (out->clusters[i].width >= APP_W_SINGLE_ZONES * APP_MULTI_WIDTH_K) {
            out->multi_suspect = true;
            break;
        }
    }
    return out->n_clusters;
}
