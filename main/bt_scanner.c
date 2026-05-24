/*
 * bt_scanner.c — sequential BLE + Classic with stack teardown between phases
 */

#include "bt_scanner.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ble_scanner.h"
#include "bt_stack.h"
#include "classic_scanner.h"
#include "device_cache.h"
#include "radio_guard.h"
#include "scanner_types.h"

static const char *TAG = "bt_scan";

#define BLE_PHASE_MS      10000
#define CLASSIC_PHASE_MS  11000
#define HEAP_PAUSE_MS     300

esp_err_t bt_scanner_run(scanner_mode_t active_mode)
{
    esp_err_t err;

    ESP_LOGI(TAG, "BT dual scan (BLE then Classic)");
    esp_err_t guard = radio_guard_prepare_bt();
    if (guard != ESP_OK) {
        return guard;
    }

    device_cache_clear();

    err = bt_stack_init();
    if (err != ESP_OK) {
        return err;
    }

    err = ble_scanner_run(BLE_PHASE_MS, active_mode);
    bt_stack_shutdown();
    vTaskDelay(pdMS_TO_TICKS(HEAP_PAUSE_MS));
    radio_guard_log_heap(TAG);

    if (mode_get() != active_mode) {
        device_cache_log_summary();
        return err;
    }

    err = bt_stack_init();
    if (err != ESP_OK) {
        return err;
    }

    err = classic_scanner_run(CLASSIC_PHASE_MS, active_mode);
    bt_stack_shutdown();
    vTaskDelay(pdMS_TO_TICKS(HEAP_PAUSE_MS));
    radio_guard_log_heap(TAG);

    device_cache_log_summary();
    return err;
}
