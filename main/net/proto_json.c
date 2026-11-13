#include "net_proto.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_mac.h"

int proto_build_telemetry(const app_state_t *st, char *buf, int buflen)
{
    const char *net = "wifi";
    if (st->net_mode == NET_MODE_LTE) net = "lte";
    else if (st->net_mode == NET_MODE_AUTO) net = "auto";

    /* 软件方案第6.1节：设备号形如 pcounter-xxxxxx */
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char device_id[24];
    snprintf(device_id, sizeof(device_id), "%s-%02x%02x%02x",
             APP_DEVICE_ID_PREFIX, mac[3], mac[4], mac[5]);

    return snprintf(buf, buflen,
                    "{\"device_id\":\"%s\",\"in\":%lu,\"out\":%lu,\"occ\":%ld,"
                    "\"bat\":%u,\"alarm\":%u,\"net\":\"%s\",\"ts\":%lld}",
                    device_id,
                    (unsigned long)st->in_count,
                    (unsigned long)st->out_count,
                    (long)st->occ,
                    st->bat_percent,
                    st->alarm,
                    net,
                    (long long)(esp_timer_get_time() / 1000000LL));
}

bool proto_handle_command(const char *json, app_state_t *st)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (!cJSON_IsString(cmd)) {
        cJSON_Delete(root);
        return false;
    }

    if (strcmp(cmd->valuestring, "clear_daily") == 0) {
        st->in_count = 0;
        st->out_count = 0;
        st->occ = 0;
    } else if (strcmp(cmd->valuestring, "set_threshold") == 0) {
        cJSON *th = cJSON_GetObjectItem(root, "occ_threshold");
        if (cJSON_IsNumber(th)) st->occ_threshold = (uint16_t)th->valuedouble;
    } else if (strcmp(cmd->valuestring, "set_net_mode") == 0) {
        cJSON *m = cJSON_GetObjectItem(root, "mode");
        if (cJSON_IsString(m)) {
            if (strcmp(m->valuestring, "wifi") == 0) st->net_mode = NET_MODE_WIFI;
            else if (strcmp(m->valuestring, "lte") == 0) st->net_mode = NET_MODE_LTE;
            else st->net_mode = NET_MODE_AUTO;
        }
    } else if (strcmp(cmd->valuestring, "set_idle_sec") == 0) {
        cJSON *s = cJSON_GetObjectItem(root, "idle_sec");
        if (cJSON_IsNumber(s)) st->idle_ms = (uint32_t)(s->valuedouble * 1000.0);
    }

    cJSON_Delete(root);
    return true;
}