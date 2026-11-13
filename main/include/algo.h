#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

typedef struct {
    float cx;
    float cy;
    float width;
    float height;
    float depth_m;
} cluster_t;

typedef enum {
    SIDE_UNKNOWN = 0,
    SIDE_A,
    SIDE_B,
} track_side_t;

typedef struct {
    int      id;
    float    cx;
    float    cy;
    float    width;
    track_side_t side;
    track_side_t prev_side;
    uint32_t last_seen_ms;
    bool     active;
    bool     counted_crossing;
} track_t;

typedef struct {
    cluster_t clusters[APP_MAX_CLUSTERS];
    int       n_clusters;
    bool      multi_suspect;
} perceive_result_t;

int  algo_cluster_from_depth(const uint16_t dist_mm[APP_TOF_ZONE_N],
                             float install_h_m,
                             perceive_result_t *out);

void algo_tracker_update(const perceive_result_t *perc, uint32_t now_ms, track_t tracks[APP_MAX_TRACKS]);

typedef struct {
    int delta_in;
    int delta_out;
} cross_event_t;

cross_event_t algo_line_cross_process(track_t tracks[APP_MAX_TRACKS], float gate_y);

int algo_cam_count_people(const uint8_t *gray, int w, int h);