/*
 * radio_guard.c — tear down Wi-Fi or BT before starting the other radio
 */

#include "radio_guard.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_heap_caps.h"

#include "bt_stack.h"
#include "espnow_link.h"

static const char *TAG = "radio_guard";

static bool s_wifi_up;

void radio_guard_log_heap(const char *tag)
{
    ESP_LOGI(tag, "heap free: %u bytes", (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
}

void radio_guard_all_off(void)
{
    if (s_wifi_up) {
        esp_wifi_stop();
        esp_wifi_deinit();
        s_wifi_up = false;
        ESP_LOGI(TAG, "Wi-Fi driver off");
    }
    bt_stack_shutdown();
}

esp_err_t radio_guard_prepare_bt(void)
{
    /* Keep Wi-Fi up when ESP-NOW is active (panel link over the air). */
    if (s_wifi_up && !espnow_link_wifi_held()) {
        esp_err_t ret = esp_wifi_stop();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_wifi_stop: %s", esp_err_to_name(ret));
        }
        ret = esp_wifi_deinit();
        if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_INIT) {
            ESP_LOGW(TAG, "esp_wifi_deinit: %s", esp_err_to_name(ret));
        }
        s_wifi_up = false;
    }
    bt_stack_shutdown();
    radio_guard_log_heap(TAG);
    return ESP_OK;
}

esp_err_t radio_guard_prepare_wifi(void)
{
    bt_stack_shutdown();
    radio_guard_log_heap(TAG);
    return ESP_OK;
}

void radio_guard_wifi_mark_up(void)
{
    s_wifi_up = true;
}

void radio_guard_wifi_mark_down(void)
{
    s_wifi_up = false;
}
