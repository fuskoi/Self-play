#include "drivers.h"
#include "app_config.h"

void ui_render_main(void)
{
    app_state_t *st = app_state_get();
    app_state_lock();
    uint32_t in_c = st->in_count;
    uint32_t out_c = st->out_count;
    int32_t occ = st->occ;
    uint8_t bat = st->bat_percent;
    uint8_t alarm = st->alarm;
    net_mode_t nm = st->net_mode;
    app_state_unlock();

    const char *net = "auto";
    if (nm == NET_MODE_WIFI) net = "wifi";
    else if (nm == NET_MODE_LTE) net = "lte";

    /* 主页：进/出/在场；底栏：电量/网络/告警（同帧刷新） */
    oled_show_main(in_c, out_c, occ);
    oled_show_status(bat, net, alarm);
}