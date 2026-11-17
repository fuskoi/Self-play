#include "algo.h"
#include <string.h>
#include <math.h>

static uint16_t s_bg[APP_TOF_ZONE_N];
static uint32_t s_bg_acc[APP_TOF_ZONE_N];
static uint16_t s_bg_cnt[APP_TOF_ZONE_N];
static bool s_bg_ready;
static int s_bg_frames;

void algo_fuse_dual_depth(const uint16_t a[APP_TOF_ZONE_N],
                          const uint16_t b[APP_TOF_ZONE_N],
                          uint16_t out[APP_TOF_ZONE_N],
                          float *presence_a,
                          float *presence_b)
{
    int pa = 0, pb = 0;
    for (int i = 0; i < APP_TOF_ZONE_N; i++) {
        uint16_t da = a[i];
        uint16_t db = b[i];
        bool va = (da > 100 && da < TOF_INVALID_MM);
        bool vb = (db > 100 && db < TOF_INVALID_MM);
        if (va) {
            pa++;
        }
        if (vb) {
            pb++;
        }
        if (va && vb) {
            out[i] = (da < db) ? da : db;
        } else if (va) {
            out[i] = da;
        } else if (vb) {
            out[i] = db;
        } else {
            out[i] = TOF_INVALID_MM;
        }
    }
    if (presence_a) {
        *presence_a = (float)pa / (float)APP_TOF_ZONE_N;
    }
    if (presence_b) {
        *presence_b = (float)pb / (float)APP_TOF_ZONE_N;
    }
}

void algo_bg_reset(void)
{
    memset(s_bg, 0, sizeof(s_bg));
    memset(s_bg_acc, 0, sizeof(s_bg_acc));
    memset(s_bg_cnt, 0, sizeof(s_bg_cnt));
    s_bg_ready = false;
    s_bg_frames = 0;
}

void algo_bg_accumulate(const uint16_t dist_mm[APP_TOF_ZONE_N])
{
    for (int i = 0; i < APP_TOF_ZONE_N; i++) {
        uint16_t d = dist_mm[i];
        if (d > 100 && d < TOF_INVALID_MM) {
            s_bg_acc[i] += d;
            s_bg_cnt[i]++;
        }
    }
    s_bg_frames++;
}

bool algo_bg_finalize(void)
{
    int valid = 0;
    for (int i = 0; i < APP_TOF_ZONE_N; i++) {
        if (s_bg_cnt[i] >= 5) {
            s_bg[i] = (uint16_t)(s_bg_acc[i] / s_bg_cnt[i]);
            valid++;
        } else {
            s_bg[i] = 0;
        }
    }
    s_bg_ready = (valid >= 16);
    return s_bg_ready;
}

bool algo_bg_ready(void)
{
    return s_bg_ready;
}

uint16_t algo_bg_get(int zone)
{
    if (zone < 0 || zone >= APP_TOF_ZONE_N) {
        return 0;
    }
    return s_bg[zone];
}

int algo_count_human_zones(const uint16_t dist_mm[APP_TOF_ZONE_N], float install_h_m)
{
    int n = 0;
    for (int i = 0; i < APP_TOF_ZONE_N; i++) {
        float d = dist_mm[i] / 1000.0f;
        if (d < 0.05f || d > install_h_m + 0.5f) {
            continue;
        }
        float h = install_h_m - d;
        if (h >= APP_HUMAN_H_MIN_M && h <= APP_HUMAN_H_MAX_M) {
            n++;
        }
    }
    return n;
}
