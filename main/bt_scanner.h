/*
 * bt_scanner.h — BLE then Classic sequential scan (no nRF)
 */

#ifndef BT_SCANNER_H
#define BT_SCANNER_H

#include "esp_err.h"
#include "scanner_types.h"

esp_err_t bt_scanner_run(scanner_mode_t active_mode);

#endif /* BT_SCANNER_H */
