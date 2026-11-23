#include "i2c_bus.h"
#include "board_pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "i2c_bus";
static SemaphoreHandle_t s_mtx;
static bool s_inited;
static i2c_master_bus_handle_t s_bus;

#define I2C_DEV_CACHE_N 6

typedef struct {
    bool used;
    uint8_t addr7;
    i2c_master_dev_handle_t dev;
} i2c_dev_slot_t;

static i2c_dev_slot_t s_devs[I2C_DEV_CACHE_N];

esp_err_t i2c_bus_ensure_init(void)
{
    if (!s_mtx) {
        s_mtx = xSemaphoreCreateMutex();
        if (!s_mtx) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (s_inited) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus: %s", esp_err_to_name(err));
        return err;
    }

    memset(s_devs, 0, sizeof(s_devs));
    s_inited = true;
    ESP_LOGI(TAG, "I2C%d SDA=%d SCL=%d @%d (master ng)", (int)I2C_PORT_NUM,
             PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
    return ESP_OK;
}

i2c_master_bus_handle_t i2c_bus_handle(void)
{
    return s_bus;
}

static esp_err_t get_dev(uint8_t addr7, i2c_master_dev_handle_t *out)
{
    for (int i = 0; i < I2C_DEV_CACHE_N; i++) {
        if (s_devs[i].used && s_devs[i].addr7 == addr7) {
            *out = s_devs[i].dev;
            return ESP_OK;
        }
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr7,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev = NULL;
    esp_err_t err = i2c_master_bus_add_device(s_bus, &dev_cfg, &dev);
    if (err != ESP_OK) {
        return err;
    }

    for (int i = 0; i < I2C_DEV_CACHE_N; i++) {
        if (!s_devs[i].used) {
            s_devs[i].used = true;
            s_devs[i].addr7 = addr7;
            s_devs[i].dev = dev;
            *out = dev;
            return ESP_OK;
        }
    }

    /* 缓存满：仍返回可用句柄，但不缓存（少见） */
    *out = dev;
    ESP_LOGW(TAG, "dev cache full, addr=0x%02x uncached", addr7);
    return ESP_OK;
}

esp_err_t i2c_bus_write(uint8_t addr7, const uint8_t *data, size_t len, int timeout_ms)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    i2c_bus_lock();
    i2c_master_dev_handle_t dev;
    esp_err_t err = get_dev(addr7, &dev);
    if (err == ESP_OK) {
        err = i2c_master_transmit(dev, data, len, timeout_ms);
    }
    i2c_bus_unlock();
    return err;
}

esp_err_t i2c_bus_write_read(uint8_t addr7, const uint8_t *wdata, size_t wlen,
                             uint8_t *rdata, size_t rlen, int timeout_ms)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    i2c_bus_lock();
    i2c_master_dev_handle_t dev;
    esp_err_t err = get_dev(addr7, &dev);
    if (err == ESP_OK) {
        err = i2c_master_transmit_receive(dev, wdata, wlen, rdata, rlen, timeout_ms);
    }
    i2c_bus_unlock();
    return err;
}

void i2c_bus_lock(void)
{
    if (s_mtx) {
        xSemaphoreTake(s_mtx, portMAX_DELAY);
    }
}

void i2c_bus_unlock(void)
{
    if (s_mtx) {
        xSemaphoreGive(s_mtx);
    }
}
