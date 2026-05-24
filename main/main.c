/*
 * main.c — application entry, serial commands, mode supervisor, LED
 *
 * Architecture (tasks, modes, boot order): see README.md in the repo root.
 *
 * Serial commands (115200 baud): wifi | bt | stop
 * Hardware: ESP32-D0WD, DOIT DevKit V1 (blue LED on GPIO 2)
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

#include "scanner_types.h"
#include "wifi_scanner.h"

static const char *TAG = "main";

/* On-board LED: swap ON/OFF if your board uses active-low wiring (see README) */
#define SCAN_LED_GPIO       GPIO_NUM_2
#define SCAN_LED_LEVEL_ON   1
#define SCAN_LED_LEVEL_OFF  0

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

/* Called from mode_set() so LED always matches wifi/bt vs idle/stop */
static void scan_led_apply(scanner_mode_t mode)
{
    int level = SCAN_LED_LEVEL_OFF;
    if (mode == MODE_WIFI || mode == MODE_BT) {
        level = SCAN_LED_LEVEL_ON;
    }
    gpio_set_level(SCAN_LED_GPIO, level);
}

/* ── Mode subsystem (see scanner_types.h) ─────────────────────────────────── */

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

/* Strip spaces and CR/LF so "wifi\r\n" matches exactly */
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

/* Placeholder until real Bluedroid scanner lands (Grand Plan Phase 1) */
static void bt_scanner_run(void)
{
    ESP_LOGI(TAG, "Bluetooth scanner started (placeholder — Stage 3)");
    while (mode_get() == MODE_BT) {
        mode_wait_change(1000);
    }
    ESP_LOGI(TAG, "Bluetooth scanner stopped");
}

/*
 * command_task — reads serial input only; never runs Wi-Fi/BT itself.
 * Typing a command calls mode_set(); supervisor_task does the real work.
 */
static void command_task(void *arg)
{
    char buf[64];
    while (1) {
        if (fgets(buf, sizeof(buf), stdin) != NULL) {
            cmd_trim(buf);
            if (buf[0] == '\0') {
                /* empty line */
            } else if (cmd_is(buf, "wifi")) {
                mode_set(MODE_WIFI);
                ESP_LOGI(TAG, "→ Wi-Fi mode");
            } else if (cmd_is(buf, "bt")) {
                mode_set(MODE_BT);
                ESP_LOGI(TAG, "→ Bluetooth mode");
            } else if (cmd_is(buf, "stop")) {
                mode_set(MODE_IDLE);
                ESP_LOGI(TAG, "→ Idle");
            } else {
                ESP_LOGW(TAG, "Unknown command. Use: wifi | bt | stop");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/*
 * supervisor_task — picks which scanner to run based on mode.
 * Blocking calls (e.g. wifi_scanner_run) live here, not in command_task.
 */
static void supervisor_task(void *arg)
{
    ESP_LOGI(TAG, "Supervisor ready — commands: wifi | bt | stop");
    while (1) {
        switch (mode_get()) {
        case MODE_WIFI: {
            esp_err_t err = wifi_scanner_run();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Wi-Fi scanner error: %s — returning to idle",
                         esp_err_to_name(err));
                mode_set(MODE_IDLE);
            }
            break;
        }
        case MODE_BT:
            bt_scanner_run();
            break;

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

/*
 * app_main — runs once at boot, then FreeRTOS tasks take over.
 * Step numbers match README.md "Walkthrough: boot sequence".
 */
void app_main(void)
{
    /* 1. NVS (flash storage for Wi-Fi calibration, etc.) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS OK");

    /* 2. Synchronisation for mode_wait_change / idle wait */
    s_mode_eg = xEventGroupCreate();
    configASSERT(s_mode_eg != NULL);

    scan_led_init();

    /* 3. USB serial: blocking stdin so fgets() waits for your commands */
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

    /* 4. Network stack — kept alive for whole program (future ESP-NOW) */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    assert(sta != NULL);
    ESP_LOGI(TAG, "TCP/IP stack OK");

    /* 5. One-time scanner setup (Wi-Fi event handler registration) */
    ESP_ERROR_CHECK(wifi_scanner_init());

    /* 6. Start command reader and mode supervisor */
    if (xTaskCreate(command_task, "cmd_task", 4096, NULL, 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create cmd_task");
        abort();
    }
    if (xTaskCreate(supervisor_task, "supervisor", 8192, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create supervisor task");
        abort();
    }
}
