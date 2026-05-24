/*
 * device_cache.h — deduplicated discovery table
 */

#ifndef DEVICE_CACHE_H
#define DEVICE_CACHE_H

#include <stddef.h>
#include <stdint.h>

#include "device_record.h"
#include "esp_err.h"

#define DEVICE_CACHE_MAX_ENTRIES  64

typedef void (*device_cache_foreach_cb_t)(const device_record_t *rec, void *ctx);

void device_cache_init(void);
void device_cache_clear(void);

/* Insert or update by (radio, addr, addr_type). Keeps best RSSI name if longer. */
esp_err_t device_cache_upsert(const device_record_t *rec);

size_t device_cache_count(void);
void device_cache_foreach(device_cache_foreach_cb_t cb, void *ctx);

/* Compact serial summary for debug (not a UI table). */
void device_cache_log_summary(void);

#endif /* DEVICE_CACHE_H */
