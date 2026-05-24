#ifndef TELEMETRY_ENCODE_H
#define TELEMETRY_ENCODE_H

#include <stddef.h>

#include "esp_err.h"
#include "esp_wifi_types.h"

#include "device_record.h"

esp_err_t telemetry_encode_wifi(const wifi_ap_record_t *ap,
                                uint32_t ts_ms,
                                char *out,
                                size_t out_len);

esp_err_t telemetry_encode_record(const device_record_t *rec,
                                  char *out,
                                  size_t out_len);

#endif
