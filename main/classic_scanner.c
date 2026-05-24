/*
 * classic_scanner.c — Classic BT inquiry → device_cache
 */

#include "classic_scanner.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "device_cache.h"
#include "scanner_types.h"

static const char *TAG = "classic_scan";

#define CLASSIC_DISC_DONE_BIT  BIT0

static EventGroupHandle_t s_classic_eg;
static scanner_mode_t     s_active_mode;
static volatile bool      s_discovery_done;

static bool get_name_from_eir(uint8_t *eir, uint8_t eir_len, char *name, uint8_t *name_len)
{
    if (eir == NULL || eir_len == 0) {
        return false;
    }

    uint8_t len = 0;
    uint8_t *p = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
    if (p == NULL) {
        p = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
    }
    if (p == NULL || len == 0) {
        return false;
    }
    if (len > DEVICE_NAME_MAX) {
        len = DEVICE_NAME_MAX;
    }
    memcpy(name, p, len);
    name[len] = '\0';
    *name_len = len;
    return true;
}

static void handle_disc_res(esp_bt_gap_cb_param_t *param)
{
    device_record_t rec = { 0 };
    rec.valid = true;
    rec.radio = SCAN_RADIO_CLASSIC;
    memcpy(rec.addr, param->disc_res.bda, 6);
    rec.addr_type = 0;

    uint32_t cod = 0;
    int8_t rssi = -127;
    uint8_t *bdname = NULL;
    uint8_t bdname_len = 0;
    uint8_t *eir = NULL;
    uint8_t eir_len = 0;

    for (int i = 0; i < param->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *p = param->disc_res.prop + i;
        switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_COD:
            cod = *(uint32_t *)(p->val);
            break;
        case ESP_BT_GAP_DEV_PROP_RSSI:
            rssi = *(int8_t *)(p->val);
            break;
        case ESP_BT_GAP_DEV_PROP_BDNAME:
            bdname_len = (p->len > DEVICE_NAME_MAX) ? DEVICE_NAME_MAX : (uint8_t)p->len;
            bdname = (uint8_t *)(p->val);
            break;
        case ESP_BT_GAP_DEV_PROP_EIR:
            eir_len = (uint8_t)p->len;
            eir = (uint8_t *)(p->val);
            break;
        default:
            break;
        }
    }

    rec.rssi = rssi;
    rec.cod = cod;

    if (bdname_len > 0 && bdname != NULL) {
        memcpy(rec.name, bdname, bdname_len);
        rec.name[bdname_len] = '\0';
        rec.name_len = bdname_len;
    } else if (eir != NULL) {
        get_name_from_eir(eir, eir_len, rec.name, &rec.name_len);
    }

    device_cache_upsert(&rec);
}

void classic_scanner_reset_state(void)
{
    s_discovery_done = false;
    if (s_classic_eg) {
        xEventGroupClearBits(s_classic_eg, CLASSIC_DISC_DONE_BIT);
    }
}

void classic_scanner_gap_handler(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    if (s_classic_eg == NULL) {
        return;
    }

    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT:
        handle_disc_res(param);
        break;
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            s_discovery_done = true;
            xEventGroupSetBits(s_classic_eg, CLASSIC_DISC_DONE_BIT);
        }
        break;
    default:
        break;
    }
}

esp_err_t classic_scanner_run(uint32_t duration_ms, scanner_mode_t active_mode)
{
    if (s_classic_eg == NULL) {
        s_classic_eg = xEventGroupCreate();
        if (s_classic_eg == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    s_active_mode = active_mode;
    classic_scanner_reset_state();

    /* inq_len units: 1.28 s; 9 ≈ 11.5 s */
    uint8_t inq_len = 9;
    if (duration_ms > 0) {
        uint32_t units = (duration_ms + 1279) / 1280;
        if (units < 1) {
            units = 1;
        }
        if (units > 0x30) {
            units = 0x30;
        }
        inq_len = (uint8_t)units;
    }

    ESP_LOGI(TAG, "Classic inquiry inq_len=%u (~%u ms)", inq_len, (unsigned)(inq_len * 1280));

    esp_err_t ret = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, inq_len, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "start_discovery: %s", esp_err_to_name(ret));
        return ret;
    }

    while (mode_get() == s_active_mode && !s_discovery_done) {
        EventBits_t bits = xEventGroupWaitBits(s_classic_eg,
                                               CLASSIC_DISC_DONE_BIT,
                                               pdFALSE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(500));
        if (bits & CLASSIC_DISC_DONE_BIT) {
            break;
        }
    }

    if (mode_get() != s_active_mode) {
        esp_bt_gap_cancel_discovery();
    }

    ESP_LOGI(TAG, "Classic inquiry done");
    return ESP_OK;
}
