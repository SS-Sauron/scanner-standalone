/*
 * telemetry_uart.c — UART link to CrowPanel (UART2)
 */

#include "telemetry_uart.h"

#include <string.h>

#include "board_config.h"
#include "esp_log.h"
#include "driver/uart.h"

static const char *TAG = "telem_uart";
static bool s_init;

esp_err_t telemetry_uart_init(void)
{
    if (s_init) {
        return ESP_OK;
    }

    uart_config_t cfg = {
        .baud_rate = TELEMETRY_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(TELEMETRY_UART_NUM, 512, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "driver_install: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_param_config(TELEMETRY_UART_NUM, &cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = uart_set_pin(TELEMETRY_UART_NUM,
                       TELEMETRY_UART_TX_GPIO,
                       TELEMETRY_UART_RX_GPIO,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set_pin: %s", esp_err_to_name(ret));
        return ret;
    }

    s_init = true;
    ESP_LOGI(TAG, "Telemetry UART%d @ %d baud TX=%d RX=%d",
             TELEMETRY_UART_NUM, TELEMETRY_UART_BAUD,
             TELEMETRY_UART_TX_GPIO, TELEMETRY_UART_RX_GPIO);
    return ESP_OK;
}

esp_err_t telemetry_uart_write_line(const char *line)
{
    if (!s_init || line == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t len = strlen(line);
    int written = uart_write_bytes(TELEMETRY_UART_NUM, line, len);
    if (written < 0 || (size_t)written != len) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
