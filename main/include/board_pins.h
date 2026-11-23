#pragma once

/*
 * 板级引脚选择：
 *   - 默认：自研原理图（docs/4.pdf）→ board_pins_custom.h
 *   - 开发板飞线：取消下一行注释，或
 *     idf.py reconfigure -DBOARD_S3_BRINGUP=1 && idf.py build
 */

/* 当前在 ESP32-S3-CAM 上测：打开 bringup，避免占用 UART 的 43/44。
 * 换自研板时改回 0 或注释掉本行。 */
#define BOARD_S3_BRINGUP 1

#if defined(BOARD_S3_BRINGUP) && (BOARD_S3_BRINGUP)
#include "board_pins_s3_bringup.h"
#else
#include "board_pins_custom.h"
#endif
