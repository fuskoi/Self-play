#include "algo.h"

/*
 * 虚拟门线双向过线：
 *   A→B = 进（IN）
 *   B→A = 出（OUT）
 * 带侧别稳定时间 + 冷却，抑制抖动误计
 */
cross_event_t algo_line_cross_process(track_t tracks[APP_MAX_TRACKS],
                                      float gate_y, uint32_t now_ms)
{
    cross_event_t ev = {0};

    for (int i = 0; i < APP_MAX_TRACKS; i++) {
        if (!tracks[i].active) {
            continue;
        }

        track_side_t raw = (tracks[i].cy < gate_y) ? SIDE_A : SIDE_B;

        if (tracks[i].side == SIDE_UNKNOWN) {
            tracks[i].side = raw;
            tracks[i].prev_side = raw;
            tracks[i].side_since_ms = now_ms;
            continue;
        }

        /* 瞬时侧别变化：先积累稳定时间，再判定过线 */
        if (raw != tracks[i].side) {
            if (tracks[i].prev_side != raw) {
                tracks[i].prev_side = raw;
                tracks[i].side_since_ms = now_ms;
            }
            if (now_ms - tracks[i].side_since_ms < APP_SIDE_STABLE_MS) {
                continue;
            }
            if (tracks[i].last_cross_ms != 0 &&
                now_ms - tracks[i].last_cross_ms < APP_CROSS_COOLDOWN_MS) {
                tracks[i].side = raw;
                continue;
            }

            if (tracks[i].side == SIDE_A && raw == SIDE_B) {
                ev.delta_in += 1;
                tracks[i].last_cross_ms = now_ms;
                tracks[i].counted_crossing = true;
            } else if (tracks[i].side == SIDE_B && raw == SIDE_A) {
                ev.delta_out += 1;
                tracks[i].last_cross_ms = now_ms;
                tracks[i].counted_crossing = true;
            }
            tracks[i].side = raw;
            tracks[i].side_since_ms = now_ms;
        } else {
            tracks[i].prev_side = raw;
            tracks[i].side_since_ms = now_ms;
        }
    }
    return ev;
}
