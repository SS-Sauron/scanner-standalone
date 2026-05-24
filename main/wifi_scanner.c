/*
 * wifi_scanner.c — periodic Wi-Fi AP scan while MODE_WIFI is active
 *
 * Each cycle: start scan (non-blocking) → wait for SCAN_DONE event →
 * log AP count → free driver memory. See README.md for the full flow.
 */

#include "wifi_scanner.h"
#include "scanner_types.h"

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"

static const char *TAG = "wifi_scan";

/* FreeRTOS flag: set when WIFI_EVENT_SCAN_DONE fires in wifi_event_handler */
#define SCAN_DONE_BIT   BIT0
static EventGroupHandle_t           g_scan_events          = NULL;
static esp_event_handler_instance_t g_wifi_event_instance = NULL;

/* Stay below task WDT (5 s in sdkconfig); stop may not apply until this wait ends */
#define SCAN_TIMEOUT_MS  4000

/* ISR/event context: tell scan_once() the hardware finished scanning */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        xEventGroupSetBits(g_scan_events, SCAN_DONE_BIT);
    }
}

/* ESP-IDF requires this after a scan or internal AP list memory can leak */
static void scan_cleanup_ap_list(void)
{
    esp_err_t ret = esp_wifi_clear_ap_list();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_clear_ap_list failed: %s", esp_err_to_name(ret));
    }
}

esp_err_t wifi_scanner_init(void)
{
    g_scan_events = xEventGroupCreate();
    if (g_scan_events == NULL) {
        ESP_LOGE(TAG, "Failed to create scan EventGroup — out of memory");
        return ESP_ERR_NO_MEM;
    }

    /* Handler stays registered for every wifi_scanner_run() session */
    esp_err_t ret = esp_event_handler_instance_register(
                        WIFI_EVENT,
                        ESP_EVENT_ANY_ID,
                        &wifi_event_handler,
                        NULL,
                        &g_wifi_event_instance);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Event handler register failed: %s", esp_err_to_name(ret));
        vEventGroupDelete(g_scan_events);
        g_scan_events = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "Wi-Fi scanner initialised");
    return ESP_OK;
}

/* One scan: clear stale flag → start → wait → count APs → free list */
static void scan_once(void)
{
    xEventGroupClearBits(g_scan_events, SCAN_DONE_BIT);

    esp_err_t ret = esp_wifi_scan_start(NULL, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(ret));
        return;
    }

    EventBits_t bits = xEventGroupWaitBits(g_scan_events,
                                           SCAN_DONE_BIT,
                                           pdTRUE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(SCAN_TIMEOUT_MS));
    if (!(bits & SCAN_DONE_BIT)) {
        ESP_LOGW(TAG, "Scan timed out — cancelling");
        esp_wifi_scan_stop();
        scan_cleanup_ap_list();
        return;
    }

    uint16_t ap_count = 0;
    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Found %" PRIu16 " access point(s)", ap_count);
    } else {
        ESP_LOGE(TAG, "esp_wifi_scan_get_ap_num failed: %s",
                 esp_err_to_name(ret));
    }

    scan_cleanup_ap_list();
}

esp_err_t wifi_scanner_run(void)
{
    ESP_LOGI(TAG, "Starting Wi-Fi driver...");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        return ret;
    }

    ESP_LOGI(TAG, "Scanning every ~5 s  (type 'stop' to exit)");

    while (mode_get() == MODE_WIFI) {
        scan_once();
        /* Wake early if user typed stop (or changed mode) */
        mode_wait_change(5000);
    }

    ESP_LOGI(TAG, "Stopping Wi-Fi driver...");
    esp_err_t err = ESP_OK;

    ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_stop failed: %s", esp_err_to_name(ret));
        err = ret;
    }

    ret = esp_wifi_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_deinit failed: %s", esp_err_to_name(ret));
        if (err == ESP_OK) {
            err = ret;
        }
    }

    ESP_LOGI(TAG, "Wi-Fi stopped");
    return err;
}
