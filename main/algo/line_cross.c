#include "algo.h"

/* gate_y：虚拟门线在 8x8 纵向坐标（约 3.5~4.5）
 * A 侧 cy < gate，B 侧 cy > gate
 */
cross_event_t algo_line_cross_process(track_t tracks[APP_MAX_TRACKS], float gate_y)
{
    cross_event_t ev = {0};

    for (int i = 0; i < APP_MAX_TRACKS; i++) {
        if (!tracks[i].active) continue;

        track_side_t side = (tracks[i].cy < gate_y) ? SIDE_A : SIDE_B;
        tracks[i].prev_side = tracks[i].side;

        if (tracks[i].side == SIDE_UNKNOWN) {
            tracks[i].side = side;
            continue;
        }

        if (tracks[i].side == SIDE_A && side == SIDE_B) {
            ev.delta_in += 1;
            tracks[i].side = SIDE_B;
            tracks[i].counted_crossing = true;
        } else if (tracks[i].side == SIDE_B && side == SIDE_A) {
            ev.delta_out += 1;
            tracks[i].side = SIDE_A;
            tracks[i].counted_crossing = true;
        } else {
            tracks[i].side = side;
        }
    }
    return ev;
}