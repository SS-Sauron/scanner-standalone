/*
 * sweep_runner.c — sweep_all: BLE → Classic → optional nRF
 */

#include "sweep_runner.h"

#include "esp_log.h"

#include "bt_scanner.h"
#include "device_cache.h"
#include "nrf24_scanner.h"
#include "radio_guard.h"
#include "scanner_types.h"

static const char *TAG = "sweep";
static bool s_include_nrf;

void sweep_runner_set_include_nrf(bool include)
{
    s_include_nrf = include;
}

esp_err_t sweep_runner_run(void)
{
    const scanner_mode_t active_mode = MODE_SWEEP;
    esp_err_t err;

    ESP_LOGI(TAG, "sweep start (nrf=%s)", s_include_nrf ? "yes" : "no");

    err = bt_scanner_run(active_mode);
    if (err != ESP_OK || mode_get() != active_mode) {
        return err;
    }

    if (s_include_nrf && nrf24_is_attached()) {
        ESP_LOGI(TAG, "Phase 3: nRF24");
        err = nrf24_scanner_run();
        if (err == ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "nRF24 scan not implemented yet");
        }
    } else if (s_include_nrf) {
        ESP_LOGI(TAG, "Phase 3: nRF24 skipped (not attached)");
    }

    device_cache_log_summary();
    ESP_LOGI(TAG, "sweep done");
    return err;
}
