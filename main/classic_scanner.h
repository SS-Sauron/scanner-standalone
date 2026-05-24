/*
 * classic_scanner.h — Classic Bluetooth inquiry (GAP only)
 */

#ifndef CLASSIC_SCANNER_H
#define CLASSIC_SCANNER_H

#include <stdint.h>

#include "esp_gap_bt_api.h"
#include "esp_err.h"

#include "scanner_types.h"

void classic_scanner_reset_state(void);
void classic_scanner_gap_handler(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

esp_err_t classic_scanner_run(uint32_t duration_ms, scanner_mode_t active_mode);

#endif /* CLASSIC_SCANNER_H */
