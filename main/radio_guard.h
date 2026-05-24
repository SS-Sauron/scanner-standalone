/*
 * radio_guard.h — one-radio-active policy (Wi-Fi vs Bluetooth)
 */

#ifndef RADIO_GUARD_H
#define RADIO_GUARD_H

#include "esp_err.h"

void radio_guard_all_off(void);
esp_err_t radio_guard_prepare_wifi(void);
esp_err_t radio_guard_prepare_bt(void);
void radio_guard_log_heap(const char *tag);

void radio_guard_wifi_mark_up(void);
void radio_guard_wifi_mark_down(void);

#endif /* RADIO_GUARD_H */
