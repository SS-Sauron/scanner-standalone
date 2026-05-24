/*
 * scanner_types.h — shared "mode" API for main.c and scanner modules
 *
 * Full walkthrough: see README.md § "Walkthrough: the mode system"
 */

#ifndef SCANNER_TYPES_H
#define SCANNER_TYPES_H

#include <stdint.h>

/* What the chip is doing right now (only one at a time). */
typedef enum {
    MODE_IDLE = 0,  /* Waiting — no scanner running */
    MODE_WIFI = 1,  /* Supervisor is inside wifi_scanner_run() */
    MODE_BT   = 2,  /* Supervisor is in BT path (placeholder today) */
} scanner_mode_t;

/*
 * Read the current mode safely from any task.
 * Uses a spinlock inside main.c (needed on ESP32 dual-core).
 */
scanner_mode_t mode_get(void);

/*
 * Change mode, update LED, and wake tasks blocked in mode_wait_change().
 * Do not call from an interrupt handler.
 */
void mode_set(scanner_mode_t new_mode);

/*
 * Sleep until mode changes or timeout_ms expires; returns the new mode.
 * Scanner loops use this so "stop" is noticed without busy-waiting.
 */
scanner_mode_t mode_wait_change(uint32_t timeout_ms);

#endif /* SCANNER_TYPES_H */
