#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

#ifndef TOF_INVALID_MM
#define TOF_INVALID_MM 4000
#endif

typedef struct {
    float cx;
    float cy;
    float width;
    float height;
    float depth_m;
    float score_a; /* 落在 ToF-A 的权重，方向辅助 */
    float score_b;
} cluster_t;

typedef enum {
    SIDE_UNKNOWN = 0,
    SIDE_A, /* 进门侧（默认 cy < gate） */
    SIDE_B, /* 出门侧 */
} track_side_t;

typedef struct {
    int      id;
    float    cx;
    float    cy;
    float    width;
    float    score_a;
    float    score_b;
    track_side_t side;
    track_side_t prev_side;
    uint32_t side_since_ms;
    uint32_t last_seen_ms;
    uint32_t last_cross_ms;
    bool     active;
    bool     counted_crossing;
} track_t;

typedef struct {
    cluster_t clusters[APP_MAX_CLUSTERS];
    int       n_clusters;
    bool      multi_suspect;
} perceive_result_t;

typedef struct {
    int delta_in;
    int delta_out;
} cross_event_t;

/** 双 ToF 深度融合：取更近有效距离，并输出 A/B 存在度 */
void algo_fuse_dual_depth(const uint16_t a[APP_TOF_ZONE_N],
                          const uint16_t b[APP_TOF_ZONE_N],
                          uint16_t out[APP_TOF_ZONE_N],
                          float *presence_a,
                          float *presence_b);

/** 空场背景标定（差分增强） */
void algo_bg_reset(void);
void algo_bg_accumulate(const uint16_t dist_mm[APP_TOF_ZONE_N]);
bool algo_bg_finalize(void);
bool algo_bg_ready(void);
uint16_t algo_bg_get(int zone);

int  algo_cluster_from_depth(const uint16_t dist_mm[APP_TOF_ZONE_N],
                             float install_h_m,
                             float presence_a,
                             float presence_b,
                             perceive_result_t *out);

void algo_tracker_update(const perceive_result_t *perc, uint32_t now_ms,
                         track_t tracks[APP_MAX_TRACKS]);

cross_event_t algo_line_cross_process(track_t tracks[APP_MAX_TRACKS],
                                      float gate_y, uint32_t now_ms);

int algo_cam_count_people(const uint8_t *gray, int w, int h);

/** 有效人体 zone 数，供待机防误唤醒 */
int algo_count_human_zones(const uint16_t dist_mm[APP_TOF_ZONE_N], float install_h_m);
