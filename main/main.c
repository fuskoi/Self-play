#include "ch9329.h"
#include "frame_proto.h"

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_spp_api.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "bt_hid";

#define BT_DEVICE_NAME   "ESP-TEXT-KB"
#define SPP_SERVER_NAME  "ESP_TEXT_SPP"
#define TEXT_BUF_MAX     (64 * 1024)

static uint32_t s_spp_handle;
static bool s_spp_connected;
static frame_parser_t s_parser;

static uint8_t *s_text_buf;
static size_t s_text_len;
static uint16_t s_expect_seq;
static bool s_typing_busy;
static SemaphoreHandle_t s_type_lock;

typedef struct {
    size_t len;
} type_job_t;

static QueueHandle_t s_type_queue;

static void spp_send_frame(uint8_t cmd, uint16_t seq, const uint8_t *payload, uint16_t len)
{
    if (!s_spp_connected || s_spp_handle == 0) {
        return;
    }

    uint8_t out[FRAME_HEADER_SIZE + 8 + FRAME_CRC_SIZE];
    size_t n = frame_build(out, sizeof(out), cmd, seq, payload, len);
    if (n == 0) {
        return;
    }
    esp_spp_write(s_spp_handle, n, out);
}

static void spp_send_ack(uint16_t seq)
{
    uint8_t payload[2] = {(uint8_t)(seq >> 8), (uint8_t)(seq & 0xFF)};
    spp_send_frame(CMD_ACK, seq, payload, sizeof(payload));
}

static void spp_send_nak(uint16_t seq, uint8_t err)
{
    spp_send_frame(CMD_NAK, seq, &err, 1);
}

static void reset_session(void)
{
    s_text_len = 0;
    s_expect_seq = 0;
    frame_parser_reset(&s_parser);
}

static void handle_frame(const frame_t *f)
{
    if (s_typing_busy && f->cmd != CMD_ABORT) {
        spp_send_nak(f->seq, ERR_BUSY);
        return;
    }

    switch (f->cmd) {
    case CMD_DATA:
        if (f->seq != s_expect_seq) {
            spp_send_nak(f->seq, ERR_SEQ);
            return;
        }
        if (s_text_len + f->len > TEXT_BUF_MAX) {
            spp_send_nak(f->seq, ERR_OVERFLOW);
            return;
        }
        memcpy(s_text_buf + s_text_len, f->data, f->len);
        s_text_len += f->len;
        s_expect_seq++;
        spp_send_ack(f->seq);
        ESP_LOGI(TAG, "DATA seq=%u +%u total=%u", f->seq, f->len, (unsigned)s_text_len);
        break;

    case CMD_END:
        spp_send_ack(f->seq);
        ESP_LOGI(TAG, "END, typing %u bytes", (unsigned)s_text_len);
        if (s_text_len > 0) {
            type_job_t job = {.len = s_text_len};
            s_typing_busy = true;
            if (xQueueSend(s_type_queue, &job, 0) != pdTRUE) {
                s_typing_busy = false;
                spp_send_nak(f->seq, ERR_BUSY);
            }
        } else {
            reset_session();
        }
        break;

    case CMD_ABORT:
        reset_session();
        spp_send_ack(f->seq);
        ESP_LOGW(TAG, "ABORT");
        break;

    default:
        spp_send_nak(f->seq, ERR_BAD_CMD);
        break;
    }
}

static void type_task(void *arg)
{
    type_job_t job;
    while (true) {
        if (xQueueReceive(s_type_queue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        xSemaphoreTake(s_type_lock, portMAX_DELAY);
        esp_err_t err = ch9329_type_utf8(s_text_buf, job.len);
        xSemaphoreGive(s_type_lock);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "type failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "type done");
        }

        reset_session();
        s_typing_busy = false;
    }
}

static void on_spp_data(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; ++i) {
        frame_t frame;
        if (frame_parser_feed(&s_parser, data[i], &frame)) {
            /* payload points into parser buffer; handle before more feeds overwrite */
            handle_frame(&frame);
            /* After handle_frame, parser may still hold remaining bytes; OK */
        }
    }
}

static void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        ESP_LOGI(TAG, "auth %s, status=%d", param->auth_cmpl.device_name, param->auth_cmpl.stat);
        break;
    case ESP_BT_GAP_PIN_REQ_EVT:
        if (param->pin_req.min_16_digit) {
            esp_bt_pin_code_t pin = {'0', '0', '0', '0', '0', '0', '0', '0',
                                    '0', '0', '0', '0', '0', '0', '0', '0'};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin);
        } else {
            esp_bt_pin_code_t pin = {'1', '2', '3', '4'};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin);
        }
        break;
    default:
        break;
    }
}

static void spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event) {
    case ESP_SPP_INIT_EVT:
        if (param->init.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(TAG, "SPP init ok");
            esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_SLAVE, 0, SPP_SERVER_NAME);
        } else {
            ESP_LOGE(TAG, "SPP init fail %d", param->init.status);
        }
        break;

    case ESP_SPP_START_EVT:
        if (param->start.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(TAG, "SPP server started");
            esp_bt_gap_set_device_name(BT_DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        }
        break;

    case ESP_SPP_SRV_OPEN_EVT:
        s_spp_handle = param->srv_open.handle;
        s_spp_connected = true;
        reset_session();
        ESP_LOGI(TAG, "SPP connected, handle=%lu", (unsigned long)s_spp_handle);
        break;

    case ESP_SPP_CLOSE_EVT:
        s_spp_connected = false;
        s_spp_handle = 0;
        ESP_LOGW(TAG, "SPP disconnected");
        break;

    case ESP_SPP_DATA_IND_EVT:
        on_spp_data(param->data_ind.data, param->data_ind.len);
        break;

    case ESP_SPP_CONG_EVT:
        break;

    case ESP_SPP_WRITE_EVT:
        break;

    default:
        break;
    }
}

static void bt_init(void)
{
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_cb));
    ESP_ERROR_CHECK(esp_spp_register_callback(spp_cb));

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_spp_cfg_t spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = true,
        .tx_buffer_size = 0,
    };
    ESP_ERROR_CHECK(esp_spp_enhanced_init(&spp_cfg));
#else
    ESP_ERROR_CHECK(esp_spp_init(ESP_SPP_MODE_CB));
#endif

    /* Fixed PIN 1234 for easy pairing */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code = {'1', '2', '3', '4'};
    esp_bt_gap_set_pin(pin_type, 4, pin_code);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    s_text_buf = calloc(1, TEXT_BUF_MAX);
    if (!s_text_buf) {
        ESP_LOGE(TAG, "alloc text buffer failed");
        return;
    }

    s_type_lock = xSemaphoreCreateMutex();
    s_type_queue = xQueueCreate(2, sizeof(type_job_t));
    frame_parser_reset(&s_parser);

    ESP_ERROR_CHECK(ch9329_init());
    xTaskCreate(type_task, "type_task", 4096, NULL, 5, NULL);
    bt_init();

    ESP_LOGI(TAG, "ready, device name=%s, pair PIN=1234", BT_DEVICE_NAME);
}
