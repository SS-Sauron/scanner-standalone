/*
 * device_cache.c — thread-safe-ish device table (mutex + upsert)
 */

#include "device_cache.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "telemetry_emit.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "dev_cache";

static device_record_t s_table[DEVICE_CACHE_MAX_ENTRIES];
static size_t          s_count;
static SemaphoreHandle_t s_mux;

static bool addr_equal(const uint8_t *a, const uint8_t *b)
{
    return memcmp(a, b, 6) == 0;
}

static int find_slot(const device_record_t *rec)
{
    for (size_t i = 0; i < s_count; i++) {
        if (s_table[i].valid &&
            s_table[i].radio == rec->radio &&
            s_table[i].addr_type == rec->addr_type &&
            addr_equal(s_table[i].addr, rec->addr))
        {
            return (int)i;
        }
    }
    return -1;
}

void device_cache_init(void)
{
    if (s_mux == NULL) {
        s_mux = xSemaphoreCreateMutex();
    }
    device_cache_clear();
}

void device_cache_clear(void)
{
    if (s_mux && xSemaphoreTake(s_mux, pdMS_TO_TICKS(1000)) == pdTRUE) {
        memset(s_table, 0, sizeof(s_table));
        s_count = 0;
        xSemaphoreGive(s_mux);
    }
}

esp_err_t device_cache_upsert(const device_record_t *rec)
{
    if (rec == NULL || !rec->valid) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_mux == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_mux, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    device_record_t copy = *rec;
    copy.last_seen_ms = (uint32_t)(esp_timer_get_time() / 1000);

    int idx = find_slot(&copy);
    if (idx >= 0) {
        device_record_t *slot = &s_table[idx];
        bool log_update = false;

        if (copy.rssi > slot->rssi) {
            slot->rssi = copy.rssi;
        }
        if (copy.name_len > slot->name_len) {
            memcpy(slot->name, copy.name, sizeof(slot->name));
            slot->name_len = copy.name_len;
            log_update = true;
        }
        if (copy.cod != 0 && slot->cod == 0) {
            slot->cod = copy.cod;
            log_update = true;
        }
        if (copy.appearance != 0 && slot->appearance == 0) {
            slot->appearance = copy.appearance;
            log_update = true;
        }
        if (copy.mfg_data_len > slot->mfg_data_len) {
            slot->mfg_company_id = copy.mfg_company_id;
            slot->mfg_data_len = copy.mfg_data_len;
            memcpy(slot->mfg_data, copy.mfg_data, copy.mfg_data_len);
            log_update = true;
        }
        if (copy.adv_raw_len > slot->adv_raw_len) {
            slot->adv_raw_len = copy.adv_raw_len;
            memcpy(slot->adv_raw, copy.adv_raw, copy.adv_raw_len);
        }
        slot->last_seen_ms = copy.last_seen_ms;
        xSemaphoreGive(s_mux);
        if (log_update) {
            device_cache_log_record(slot);
            telemetry_emit_record(slot);
        }
        return ESP_OK;
    }

    if (s_count >= DEVICE_CACHE_MAX_ENTRIES) {
        xSemaphoreGive(s_mux);
        return ESP_ERR_NO_MEM;
    }

    s_table[s_count] = copy;
    s_table[s_count].valid = true;
    s_count++;
    device_record_t logged = s_table[s_count - 1];
    xSemaphoreGive(s_mux);
    device_cache_log_record(&logged);
    telemetry_emit_record(&logged);
    return ESP_OK;
}

size_t device_cache_count(void)
{
    size_t n = 0;
    if (s_mux && xSemaphoreTake(s_mux, pdMS_TO_TICKS(1000)) == pdTRUE) {
        n = s_count;
        xSemaphoreGive(s_mux);
    }
    return n;
}

void device_cache_foreach(device_cache_foreach_cb_t cb, void *ctx)
{
    if (cb == NULL || s_mux == NULL) {
        return;
    }
    if (xSemaphoreTake(s_mux, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return;
    }
    for (size_t i = 0; i < s_count; i++) {
        if (s_table[i].valid) {
            cb(&s_table[i], ctx);
        }
    }
    xSemaphoreGive(s_mux);
}

void device_cache_log_record(const device_record_t *rec)
{
    if (rec == NULL || !rec->valid) {
        return;
    }
    char mac[18];
    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
             rec->addr[0], rec->addr[1], rec->addr[2],
             rec->addr[3], rec->addr[4], rec->addr[5]);
    const char *radio = "?";
    switch (rec->radio) {
    case SCAN_RADIO_BLE:     radio = "BLE"; break;
    case SCAN_RADIO_CLASSIC: radio = "BR";  break;
    case SCAN_RADIO_NRF24:   radio = "NRF"; break;
    default: break;
    }
    if (rec->radio == SCAN_RADIO_CLASSIC && rec->cod != 0) {
        if (rec->name_len > 0) {
            ESP_LOGI(TAG, "[%s] %s rssi=%d cod=0x%06lx name=%s",
                     radio, mac, rec->rssi, (unsigned long)rec->cod, rec->name);
        } else {
            ESP_LOGI(TAG, "[%s] %s rssi=%d cod=0x%06lx",
                     radio, mac, rec->rssi, (unsigned long)rec->cod);
        }
        return;
    }
    if (rec->name_len > 0) {
        ESP_LOGI(TAG, "[%s] %s rssi=%d name=%s", radio, mac, rec->rssi, rec->name);
    } else {
        ESP_LOGI(TAG, "[%s] %s rssi=%d", radio, mac, rec->rssi);
    }
}

void device_cache_log_summary(void)
{
    ESP_LOGI(TAG, "=== scan complete: %u device(s) in cache ===",
             (unsigned)device_cache_count());
}
