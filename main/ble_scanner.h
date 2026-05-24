/*
 * ble_scanner.h — BLE GAP active scan (no GATT)
 */

#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <stdint.h>

#include "esp_gap_ble_api.h"
#include "esp_err.h"

#include "scanner_types.h"

void ble_scanner_reset_state(void);
void ble_scanner_gap_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

/*
 * Run BLE scan for duration_ms while mode_get() == active_mode.
 * Requires bt_stack_init() already called.
 */
esp_err_t ble_scanner_run(uint32_t duration_ms, scanner_mode_t active_mode);

#endif /* BLE_SCANNER_H */
