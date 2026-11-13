#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t battery_init(void);
uint16_t  battery_read_mv(void);
uint8_t   battery_percent(void);
bool      battery_is_low(void);
bool      battery_is_crit(void);

void standby_request_enter(void);
void standby_service(void); /* 在 power_task 中调用 */
bool standby_woke_from_tof(void);