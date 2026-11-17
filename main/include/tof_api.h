#pragma once

#include <stdint.h>
#include <stdbool.h>

uint32_t tof_last_detect_ms(void);

/** 相机人数提示写入过线校正窗口 */
void tof_set_pending_person_hint(int hint);
int  tof_take_pending_person_hint(uint32_t now_ms);

/** 空场背景标定（约 1 秒，通道无人） */
void tof_start_bg_calibrate(void);
bool tof_bg_calibrating(void);
