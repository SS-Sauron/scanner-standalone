/*
 * wifi_scanner.h — Wi-Fi access-point scan module (public API)
 *
 * This module only starts/stops the Wi-Fi *driver*. The network stack
 * (esp_netif, event loop) is set up once in main.c and stays running.
 * See README.md § "Walkthrough: Wi-Fi scanning".
 */

#ifndef WIFI_SCANNER_H
#define WIFI_SCANNER_H

#include "esp_err.h"

/* Call once at boot, after esp_event_loop_create_default() in app_main. */
esp_err_t wifi_scanner_init(void);

/*
 * Run the scan loop until mode is no longer MODE_WIFI.
 * Blocks the supervisor task — call only from supervisor_task.
 */
esp_err_t wifi_scanner_run(void);

#endif /* WIFI_SCANNER_H */
