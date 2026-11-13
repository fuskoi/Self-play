#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t battery_init(void);
uint16_t  battery_read_mv(void);
uint8_t   battery_percent(void);
bool      battery_is_low(void);
bool      battery_is_crit(void);
/* 验收用例T5：毫伏>0 强制模拟电压；为 0 则取消模拟 */
void      battery_set_sim_mv(uint16_t mv);

void standby_request_enter(void);
void standby_service(void); /* 在电源任务中调用 */
bool standby_woke_from_tof(void);