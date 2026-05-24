/*
 * ble_scanner.c — BLE GAP active scan → device_cache
 */

#include "ble_scanner.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "device_cache.h"
#include "scanner_types.h"

static const char *TAG = "ble_scan";

#define BLE_SCAN_PARAMS_DONE_BIT  BIT0
#define BLE_SCAN_STARTED_BIT      BIT1

static EventGroupHandle_t s_ble_eg;
static scanner_mode_t     s_active_mode;
static bool               s_scanning;

static esp_ble_scan_params_t s_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x50,
};

static void parse_and_upsert(esp_ble_gap_cb_param_t *param)
{
    if (param->scan_rst.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT) {
        return;
    }

    device_record_t rec = { 0 };
    rec.valid = true;
    rec.radio = SCAN_RADIO_BLE;
    memcpy(rec.addr, param->scan_rst.bda, 6);
    rec.addr_type = (uint8_t)param->scan_rst.ble_addr_type;
    rec.rssi = (int8_t)param->scan_rst.rssi;

    uint8_t len = 0;
    uint8_t *p;

    if (param->scan_rst.adv_data_len > 0) {
        rec.adv_raw_len = param->scan_rst.adv_data_len;
        if (rec.adv_raw_len > DEVICE_ADV_RAW_MAX) {
            rec.adv_raw_len = DEVICE_ADV_RAW_MAX;
        }
        memcpy(rec.adv_raw, param->scan_rst.ble_adv, rec.adv_raw_len);

        p = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                     ESP_BLE_AD_TYPE_NAME_CMPL, &len);
        if (p == NULL) {
            p = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                         ESP_BLE_AD_TYPE_NAME_SHORT, &len);
        }
        if (p && len > 0) {
            if (len > DEVICE_NAME_MAX) {
                len = DEVICE_NAME_MAX;
            }
            memcpy(rec.name, p, len);
            rec.name[len] = '\0';
            rec.name_len = len;
        }

        p = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                     ESP_BLE_AD_TYPE_FLAG, &len);
        if (p && len >= 1) {
            (void)p;
        }

        p = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                     ESP_BLE_AD_TYPE_APPEARANCE, &len);
        if (p && len >= 2) {
            rec.appearance = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
        }

        p = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                     ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE, &len);
        if (p && len >= 2) {
            rec.mfg_company_id = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
            if (len > 2) {
                rec.mfg_data_len = (uint8_t)(len - 2);
                if (rec.mfg_data_len > DEVICE_MFG_DATA_MAX) {
                    rec.mfg_data_len = DEVICE_MFG_DATA_MAX;
                }
                memcpy(rec.mfg_data, p + 2, rec.mfg_data_len);
            }
        }
    }

    if (param->scan_rst.scan_rsp_len > 0 && rec.name_len == 0) {
        uint8_t *rsp = param->scan_rst.ble_adv + param->scan_rst.adv_data_len;
        p = esp_ble_resolve_adv_data(rsp, ESP_BLE_AD_TYPE_NAME_CMPL, &len);
        if (p == NULL) {
            p = esp_ble_resolve_adv_data(rsp, ESP_BLE_AD_TYPE_NAME_SHORT, &len);
        }
        if (p && len > 0) {
            if (len > DEVICE_NAME_MAX) {
                len = DEVICE_NAME_MAX;
            }
            memcpy(rec.name, p, len);
            rec.name[len] = '\0';
            rec.name_len = len;
        }
    }

    device_cache_upsert(&rec);
}

void ble_scanner_reset_state(void)
{
    s_scanning = false;
    if (s_ble_eg) {
        xEventGroupClearBits(s_ble_eg, BLE_SCAN_PARAMS_DONE_BIT | BLE_SCAN_STARTED_BIT);
    }
}

void ble_scanner_gap_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (s_ble_eg == NULL) {
        return;
    }

    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        if (param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            xEventGroupSetBits(s_ble_eg, BLE_SCAN_PARAMS_DONE_BIT);
        }
        break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        if (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            xEventGroupSetBits(s_ble_eg, BLE_SCAN_STARTED_BIT);
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
        parse_and_upsert(param);
        break;
    default:
        break;
    }
}

esp_err_t ble_scanner_run(uint32_t duration_ms, scanner_mode_t active_mode)
{
    if (s_ble_eg == NULL) {
        s_ble_eg = xEventGroupCreate();
        if (s_ble_eg == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    s_active_mode = active_mode;
    ble_scanner_reset_state();

    ESP_LOGI(TAG, "BLE scan %u ms", (unsigned)duration_ms);

    esp_err_t ret = esp_ble_gap_set_scan_params(&s_scan_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set_scan_params: %s", esp_err_to_name(ret));
        return ret;
    }

    EventBits_t bits = xEventGroupWaitBits(s_ble_eg,
                                           BLE_SCAN_PARAMS_DONE_BIT,
                                           pdTRUE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(5000));
    if ((bits & BLE_SCAN_PARAMS_DONE_BIT) == 0) {
        ESP_LOGE(TAG, "scan params timeout");
        return ESP_ERR_TIMEOUT;
    }

    ret = esp_ble_gap_start_scanning(0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "start_scanning: %s", esp_err_to_name(ret));
        return ret;
    }
    s_scanning = true;

    uint32_t elapsed = 0;
    const uint32_t step_ms = 200;

    while (mode_get() == s_active_mode && elapsed < duration_ms) {
        mode_wait_change(step_ms);
        elapsed += step_ms;
    }

    if (s_scanning) {
        esp_ble_gap_stop_scanning();
        s_scanning = false;
    }

    ESP_LOGI(TAG, "BLE scan done");
    return ESP_OK;
}
