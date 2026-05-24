/*
 * main.c — application entry, serial commands, mode supervisor, LED
 *
 * Architecture (tasks, modes, boot order): see README.md in the repo root.
 *
 * Serial: wifi | bt-ble | bt-classic | bt | sweep_all | stop
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_wifi_default.h"

#include "board_config.h"
#include "scanner_types.h"
#include "wifi_scanner.h"
#include "bt_stack.h"
#include "ble_scanner.h"
#include "classic_scanner.h"
#include "sweep_runner.h"
#include "nrf24_scanner.h"
#include "device_cache.h"
#include "radio_guard.h"

static const char *TAG = "main";

static void scan_led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << SCAN_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(SCAN_LED_GPIO, SCAN_LED_LEVEL_OFF);
}

static void scan_led_apply(scanner_mode_t mode)
{
    int level = SCAN_LED_LEVEL_OFF;
    if (mode == MODE_WIFI || mode == MODE_BT || mode == MODE_BT_CLASSIC ||
        mode == MODE_SWEEP)
    {
        level = SCAN_LED_LEVEL_ON;
    }
    gpio_set_level(SCAN_LED_GPIO, level);
}

#define MODE_CHANGED_BIT  BIT0

static portMUX_TYPE            s_mode_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile scanner_mode_t s_mode     = MODE_IDLE;
static EventGroupHandle_t      s_mode_eg  = NULL;

scanner_mode_t mode_get(void)
{
    scanner_mode_t m;
    taskENTER_CRITICAL(&s_mode_mux);
    m = s_mode;
    taskEXIT_CRITICAL(&s_mode_mux);
    return m;
}

void mode_set(scanner_mode_t new_mode)
{
    taskENTER_CRITICAL(&s_mode_mux);
    s_mode = new_mode;
    taskEXIT_CRITICAL(&s_mode_mux);
    scan_led_apply(new_mode);
    xEventGroupSetBits(s_mode_eg, MODE_CHANGED_BIT);
}

scanner_mode_t mode_wait_change(uint32_t timeout_ms)
{
    xEventGroupWaitBits(s_mode_eg,
                        MODE_CHANGED_BIT,
                        pdTRUE,
                        pdFALSE,
                        pdMS_TO_TICKS(timeout_ms));
    return mode_get();
}

static void cmd_trim(char *s)
{
    if (s == NULL || *s == '\0') {
        return;
    }

    size_t len = strlen(s);
    while (len > 0 &&
           (s[len - 1] == '\r' || s[len - 1] == '\n' ||
            s[len - 1] == ' ' || s[len - 1] == '\t'))
    {
        s[--len] = '\0';
    }

    char *p = s;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }
}

static bool cmd_is(const char *s, const char *word)
{
    return s != NULL && word != NULL && strcmp(s, word) == 0;
}

static void print_help(void)
{
    ESP_LOGI(TAG, "Commands: wifi | bt-ble | bt-classic | bt | sweep_all | stop");
}

static void command_task(void *arg)
{
    char buf[64];
    print_help();
    while (1) {
        if (fgets(buf, sizeof(buf), stdin) != NULL) {
            cmd_trim(buf);
            if (buf[0] == '\0') {
                continue;
            }
            if (cmd_is(buf, "wifi")) {
                mode_set(MODE_WIFI);
                ESP_LOGI(TAG, "→ Wi-Fi mode");
            } else if (cmd_is(buf, "bt-ble")) {
                mode_set(MODE_BT);
                ESP_LOGI(TAG, "→ BLE scan");
            } else if (cmd_is(buf, "bt-classic")) {
                mode_set(MODE_BT_CLASSIC);
                ESP_LOGI(TAG, "→ Classic scan");
            } else if (cmd_is(buf, "bt")) {
                sweep_runner_set_include_nrf(false);
                mode_set(MODE_SWEEP);
                ESP_LOGI(TAG, "→ BLE + Classic scan");
            } else if (cmd_is(buf, "sweep_all")) {
                sweep_runner_set_include_nrf(true);
                mode_set(MODE_SWEEP);
                ESP_LOGI(TAG, "→ sweep_all");
            } else if (cmd_is(buf, "stop")) {
                mode_set(MODE_IDLE);
                radio_guard_all_off();
                ESP_LOGI(TAG, "→ Idle");
            } else {
                ESP_LOGW(TAG, "Unknown command");
                print_help();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

#define BLE_ONLY_MS      10000
#define CLASSIC_ONLY_MS  11000

static void supervisor_task(void *arg)
{
    ESP_LOGI(TAG, "Supervisor ready");
    while (1) {
        switch (mode_get()) {
        case MODE_WIFI: {
            esp_err_t err = wifi_scanner_run();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Wi-Fi error: %s", esp_err_to_name(err));
                mode_set(MODE_IDLE);
            }
            break;
        }
        case MODE_BT: {
            esp_err_t err = radio_guard_prepare_bt();
            if (err != ESP_OK) {
                break;
            }
            device_cache_clear();
            err = bt_stack_init();
            if (err == ESP_OK) {
                err = ble_scanner_run(BLE_ONLY_MS, MODE_BT);
                bt_stack_shutdown();
            }
            radio_guard_log_heap(TAG);
            device_cache_log_summary();
            if (mode_get() == MODE_BT) {
                mode_set(MODE_IDLE);
            }
            break;
        }
        case MODE_BT_CLASSIC: {
            esp_err_t err = radio_guard_prepare_bt();
            if (err != ESP_OK) {
                break;
            }
            device_cache_clear();
            err = bt_stack_init();
            if (err == ESP_OK) {
                err = classic_scanner_run(CLASSIC_ONLY_MS, MODE_BT_CLASSIC);
                bt_stack_shutdown();
            }
            radio_guard_log_heap(TAG);
            device_cache_log_summary();
            if (mode_get() == MODE_BT_CLASSIC) {
                mode_set(MODE_IDLE);
            }
            break;
        }
        case MODE_SWEEP: {
            esp_err_t err = sweep_runner_run();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "sweep error: %s", esp_err_to_name(err));
            }
            if (mode_get() == MODE_SWEEP) {
                mode_set(MODE_IDLE);
            }
            break;
        }
        case MODE_IDLE:
        default:
            xEventGroupWaitBits(s_mode_eg,
                                MODE_CHANGED_BIT,
                                pdTRUE,
                                pdFALSE,
                                portMAX_DELAY);
            break;
        }
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS OK");

    s_mode_eg = xEventGroupCreate();
    configASSERT(s_mode_eg != NULL);

    scan_led_init();
    nrf24_probe_on_boot();
    device_cache_init();

    ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,
                                        256, 0, 0, NULL, 0));
    uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
    int fd = fileno(stdin);
    if (fd >= 0) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        }
    }
    ESP_LOGI(TAG, "stdin blocking OK");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    assert(sta != NULL);
    ESP_LOGI(TAG, "TCP/IP stack OK");

    ESP_ERROR_CHECK(wifi_scanner_init());

    if (xTaskCreate(command_task, "cmd_task", 4096, NULL, 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create cmd_task");
        abort();
    }
    if (xTaskCreate(supervisor_task, "supervisor", 16384, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create supervisor task");
        abort();
    }
}
