#include "i2c_bus.h"
#include "board_pins.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "i2c_bus";
static SemaphoreHandle_t s_mtx;
static bool s_inited;

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

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_PORT_NUM, &conf);
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_driver_install(I2C_PORT_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    s_inited = true;
    ESP_LOGI(TAG, "I2C%d SDA=%d SCL=%d @%d", (int)I2C_PORT_NUM,
             PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
    return ESP_OK;
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
