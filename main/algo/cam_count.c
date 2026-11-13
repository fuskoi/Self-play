#include "algo.h"
#include <stdlib.h>
#include <string.h>

/* 简易灰度阈值 + 连通域点数，限制返回 1~3 */
int algo_cam_count_people(const uint8_t *gray, int w, int h)
{
    if (!gray || w <= 0 || h <= 0) {
        return 1;
    }

    /* 下采样步长，降低算量 */
    const int step = (w > 80) ? 2 : 1;
    int tw = w / step;
    int th = h / step;
    if (tw * th > 160 * 120) {
        return 1;
    }

    /* 自适应阈值：均值 */
    uint32_t sum = 0;
    int n = 0;
    for (int y = 0; y < h; y += step) {
        for (int x = 0; x < w; x += step) {
            sum += gray[y * w + x];
            n++;
        }
    }
    uint8_t thr = (uint8_t)(sum / (n ? n : 1));
    if (thr < 40) thr = 40;
    if (thr > 200) thr = 200;

    /* 只统计较暗连通块数量（俯视人头往往偏暗/对比）——按场景可反相 */
    bool *mask = (bool *)calloc((size_t)tw * th, 1);
    bool *vis = (bool *)calloc((size_t)tw * th, 1);
    if (!mask || !vis) {
        free(mask);
        free(vis);
        return 1;
    }

    for (int y = 0, yy = 0; y < h; y += step, yy++) {
        for (int x = 0, xx = 0; x < w; x += step, xx++) {
            mask[yy * tw + xx] = gray[y * w + x] < thr;
        }
    }

    int blobs = 0;
    int *q = (int *)malloc((size_t)tw * th * sizeof(int));
    if (!q) {
        free(mask);
        free(vis);
        return 1;
    }

    for (int i = 0; i < tw * th; i++) {
        if (!mask[i] || vis[i]) continue;
        int qh = 0, qt = 0;
        q[qt++] = i;
        vis[i] = true;
        int area = 0;
        while (qh < qt) {
            int cur = q[qh++];
            area++;
            int cx = cur % tw;
            int cy = cur / tw;
            const int ddx[4] = {1, -1, 0, 0};
            const int ddy[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
                int nx = cx + ddx[k];
                int ny = cy + ddy[k];
                if (nx < 0 || nx >= tw || ny < 0 || ny >= th) continue;
                int ni = ny * tw + nx;
                if (mask[ni] && !vis[ni]) {
                    vis[ni] = true;
                    q[qt++] = ni;
                }
            }
        }
        /* 过滤过小噪声 */
        if (area >= (tw * th) / 80) {
            blobs++;
        }
    }

    free(q);
    free(mask);
    free(vis);

    if (blobs < 1) blobs = 1;
    if (blobs > 3) blobs = 3;
    return blobs;
}