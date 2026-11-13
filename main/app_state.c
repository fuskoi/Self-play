#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static app_state_t s_st;
static SemaphoreHandle_t s_mtx;

void app_state_init(void)
{
    memset(&s_st, 0, sizeof(s_st));
    s_st.net_mode = NET_MODE_AUTO;
    s_st.sys_mode = SYS_MODE_ACTIVE;
    s_st.occ_threshold = APP_OCC_THRESHOLD_DEFAULT;
    s_st.idle_ms = APP_T_IDLE_MS;
    s_st.bat_percent = 100;
    s_mtx = xSemaphoreCreateMutex();
}

app_state_t *app_state_get(void)
{
    return &s_st;
}

void app_state_lock(void)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
}

void app_state_unlock(void)
{
    xSemaphoreGive(s_mtx);
}