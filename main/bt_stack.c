/*
 * bt_stack.c — ESP32 Bluedroid dual-mode controller lifecycle
 */

#include "bt_stack.h"

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gap_bt_api.h"

#include "ble_scanner.h"
#include "classic_scanner.h"

static const char *TAG = "bt_stack";
static bool s_up;

static void ble_gap_router(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ble_scanner_gap_handler(event, param);
}

static void bt_gap_router(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    classic_scanner_gap_handler(event, param);
}

esp_err_t bt_stack_init(void)
{
    if (s_up) {
        return ESP_OK;
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "controller init: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "controller enable: %s", esp_err_to_name(ret));
        esp_bt_controller_deinit();
        return ret;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bluedroid init: %s", esp_err_to_name(ret));
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bluedroid enable: %s", esp_err_to_name(ret));
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return ret;
    }

    ret = esp_ble_gap_register_callback(ble_gap_router);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ble gap register: %s", esp_err_to_name(ret));
        bt_stack_shutdown();
        return ret;
    }

    ret = esp_bt_gap_register_callback(bt_gap_router);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bt gap register: %s", esp_err_to_name(ret));
        bt_stack_shutdown();
        return ret;
    }

    ble_scanner_reset_state();
    classic_scanner_reset_state();

    s_up = true;
    ESP_LOGI(TAG, "Bluedroid BTDM up");
    return ESP_OK;
}

void bt_stack_shutdown(void)
{
    if (!s_up) {
        return;
    }

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    s_up = false;
    ESP_LOGI(TAG, "Bluedroid BTDM down");
}

bool bt_stack_is_up(void)
{
    return s_up;
}
