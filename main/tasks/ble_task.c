#include "tasks_api.h"
#include "net_proto.h"
#include "ble_svc.h"
#include "wifi_mqtt.h"
#include "app_config.h"
#include "esp_log.h"
#include "cJSON.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <stdio.h>
#include <string.h>

/*
 * 方案 §6.2 GATT（16-bit UUID，完整形式 0000FFF0-…-00805F9B34FB）：
 *   0xFFF0  Service
 *   0xFFF1  telemetry  Notify/Read
 *   0xFFF2  wifi_cred  Write  {"ssid":"...","pass":"..."}
 *   0xFFF3  command    Write  下行 JSON
 *   0xFFF4  device_info Read
 */

static const char *TAG = "ble";

#define UUID_SVC_RIDER     0xFFF0
#define UUID_CHR_TELEMETRY 0xFFF1
#define UUID_CHR_WIFI_CRED 0xFFF2
#define UUID_CHR_COMMAND   0xFFF3
#define UUID_CHR_DEVINFO   0xFFF4

static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_telem_val_handle;
static bool s_notify_enabled;

static int gap_event(struct ble_gap_event *event, void *arg);
static void start_advertise(void);

static void ble_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ensure addr rc=%d", rc);
        return;
    }
    start_advertise();
}

static void ble_on_reset(int reason)
{
    ESP_LOGW(TAG, "reset reason=%d", reason);
}

static void host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static int chr_access(uint16_t conn_handle, uint16_t attr_handle,
                      struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;
    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR && uuid16 == UUID_CHR_DEVINFO) {
        char id[24];
        char info[96];
        proto_device_id(id, sizeof(id));
        snprintf(info, sizeof(info), "{\"device_id\":\"%s\",\"fw\":\"1.0\"}", id);
        int rc = os_mbuf_append(ctxt->om, info, strlen(info));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR && uuid16 == UUID_CHR_TELEMETRY) {
        char js[TELEMETRY_JSON_MAX];
        app_state_t *st = app_state_get();
        app_state_lock();
        proto_build_telemetry(st, js, sizeof(js));
        app_state_unlock();
        int rc = os_mbuf_append(ctxt->om, js, strlen(js));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len == 0 || len >= CMD_JSON_MAX) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        char buf[CMD_JSON_MAX];
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &len);
        if (rc != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        buf[len] = 0;

        if (uuid16 == UUID_CHR_WIFI_CRED) {
            cJSON *root = cJSON_Parse(buf);
            if (!root) {
                ESP_LOGW(TAG, "wifi_cred bad json");
                return BLE_ATT_ERR_UNLIKELY;
            }
            cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
            cJSON *pass = cJSON_GetObjectItem(root, "pass");
            if (!cJSON_IsString(ssid) || !ssid->valuestring || !ssid->valuestring[0]) {
                cJSON_Delete(root);
                return BLE_ATT_ERR_UNLIKELY;
            }
            const char *pw = (cJSON_IsString(pass) && pass->valuestring) ? pass->valuestring : "";
            esp_err_t err = wifi_creds_save(ssid->valuestring, pw);
            ESP_LOGI(TAG, "wifi_cred SSID=%s save=%s", ssid->valuestring, esp_err_to_name(err));
            cJSON_Delete(root);
            if (err != ESP_OK) {
                return BLE_ATT_ERR_UNLIKELY;
            }
            wifi_mqtt_notify_creds_changed();
            return 0;
        }

        if (uuid16 == UUID_CHR_COMMAND) {
            ESP_LOGI(TAG, "command: %s", buf);
            app_state_t *st = app_state_get();
            app_state_lock();
            proto_handle_command(buf, st);
            app_state_unlock();
            xEventGroupSetBits(app_events(), EVT_UI_REFRESH | EVT_TELEMETRY_PUSH);
            return 0;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(UUID_SVC_RIDER),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(UUID_CHR_TELEMETRY),
                .access_cb = chr_access,
                .val_handle = &s_telem_val_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(UUID_CHR_WIFI_CRED),
                .access_cb = chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = BLE_UUID16_DECLARE(UUID_CHR_COMMAND),
                .access_cb = chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = BLE_UUID16_DECLARE(UUID_CHR_DEVINFO),
                .access_cb = chr_access,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {0},
        },
    },
    {0},
};

static void start_advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    const char *name = ble_svc_gap_device_name();
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)name;
    fields.name_len = (uint8_t)strlen(name);
    fields.name_is_complete = 1;
    static ble_uuid16_t svc_uuid = BLE_UUID16_INIT(UUID_SVC_RIDER);
    fields.uuids16 = &svc_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv = {0};
    adv.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv.disc_mode = BLE_GAP_DISC_MODE_GEN;
    int rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv, gap_event, NULL);
    if (rc != 0) {
        rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &adv, gap_event, NULL);
    }
    ESP_LOGI(TAG, "advertise rc=%d name=%s", rc, name);
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            mqtt_route_set_ble_connected(true);
            ESP_LOGI(TAG, "connected handle=%u", s_conn_handle);
        } else {
            start_advertise();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected reason=%d", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_notify_enabled = false;
        mqtt_route_set_ble_connected(false);
        start_advertise();
        return 0;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_telem_val_handle) {
            s_notify_enabled = event->subscribe.cur_notify != 0;
            ESP_LOGI(TAG, "subscribe notify=%d", s_notify_enabled);
            if (s_notify_enabled) {
                xEventGroupSetBits(app_events(), EVT_TELEMETRY_PUSH);
            }
        }
        return 0;
    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu=%d", event->mtu.value);
        return 0;
    default:
        return 0;
    }
}

bool ble_is_connected(void)
{
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

esp_err_t ble_notify_telemetry(const char *json)
{
    if (!json || !s_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return ESP_ERR_INVALID_STATE;
    }
    struct os_mbuf *om = ble_hs_mbuf_from_flat(json, strlen(json));
    if (!om) {
        return ESP_ERR_NO_MEM;
    }
    int rc = ble_gatts_notify_custom(s_conn_handle, s_telem_val_handle, om);
    return (rc == 0) ? ESP_OK : ESP_FAIL;
}

void ble_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "start NimBLE GATT");

    char name[28];
    char id[24];
    proto_device_id(id, sizeof(id));
    /* 广播名 PC-xxxxxx */
    const char *suffix = id;
    const char *dash = strchr(id, '-');
    if (dash && dash[1]) {
        suffix = dash + 1;
    }
    snprintf(name, sizeof(name), "PC-%.20s", suffix);

    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ESP_ERROR_CHECK(ble_gatts_count_cfg(s_gatt_svcs));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(s_gatt_svcs));
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set(name));

    nimble_port_freertos_init(host_task);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
