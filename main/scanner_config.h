/*
 * scanner_config.h — scan timing (tune here)
 */

#ifndef SCANNER_CONFIG_H
#define SCANNER_CONFIG_H

/* BLE GAP scan window (active scan, 100% duty). */
#define SCAN_BLE_DURATION_MS       30000

/* Classic inquiry; driver rounds to 1.28 s units (see classic_scanner.c). */
#define SCAN_CLASSIC_DURATION_MS   30000

/* Pause between BLE and Classic when stack is torn down (bt / bt-full). */
#define SCAN_BT_PHASE_PAUSE_MS     300

#endif /* SCANNER_CONFIG_H */
