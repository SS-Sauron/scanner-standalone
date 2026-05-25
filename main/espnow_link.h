#ifndef ESPNOW_LINK_H
#define ESPNOW_LINK_H

#include <stdbool.h>
#include "esp_err.h"
#include "device_record.h"
#include "esp_wifi_types.h"

esp_err_t espnow_link_init(void);
esp_err_t espnow_link_recover(void);
bool espnow_link_ready(void);
bool espnow_link_wifi_held(void);
void espnow_link_on_wifi_down(void);

esp_err_t espnow_link_send_device(const device_record_t *rec);
esp_err_t espnow_link_send_wifi_ap(const wifi_ap_record_t *ap, uint32_t ts_ms);

#endif
