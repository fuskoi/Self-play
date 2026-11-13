#include "algo.h"
#include <math.h>
#include <string.h>

#define MATCH_DIST_MAX 2.5f
#define TRACK_TIMEOUT_MS 400

static int s_next_id = 1;

static float dist2(float x1, float y1, float x2, float y2)
{
    float dx = x1 - x2;
    float dy = y1 - y2;
    return dx * dx + dy * dy;
}

void algo_tracker_update(const perceive_result_t *perc, uint32_t now_ms, track_t tracks[APP_MAX_TRACKS])
{
    bool used[APP_MAX_CLUSTERS] = {0};

    for (int t = 0; t < APP_MAX_TRACKS; t++) {
        if (!tracks[t].active) continue;

        int best = -1;
        float best_d = MATCH_DIST_MAX * MATCH_DIST_MAX;
        for (int c = 0; c < perc->n_clusters; c++) {
            if (used[c]) continue;
            float d = dist2(tracks[t].cx, tracks[t].cy, perc->clusters[c].cx, perc->clusters[c].cy);
            if (d < best_d) {
                best_d = d;
                best = c;
            }
        }

        if (best >= 0) {
            used[best] = true;
            tracks[t].cx = perc->clusters[best].cx;
            tracks[t].cy = perc->clusters[best].cy;
            tracks[t].width = perc->clusters[best].width;
            tracks[t].last_seen_ms = now_ms;
            /* 侧别由 line_cross 根据 cy 相对门线更新 */
        } else if (now_ms - tracks[t].last_seen_ms > TRACK_TIMEOUT_MS) {
            tracks[t].active = false;
        }
    }

    for (int c = 0; c < perc->n_clusters; c++) {
        if (used[c]) continue;
        for (int t = 0; t < APP_MAX_TRACKS; t++) {
            if (tracks[t].active) continue;
            tracks[t].active = true;
            tracks[t].id = s_next_id++;
            tracks[t].cx = perc->clusters[c].cx;
            tracks[t].cy = perc->clusters[c].cy;
            tracks[t].width = perc->clusters[c].width;
            tracks[t].side = SIDE_UNKNOWN;
            tracks[t].prev_side = SIDE_UNKNOWN;
            tracks[t].last_seen_ms = now_ms;
            tracks[t].counted_crossing = false;
            break;
        }
    }
}