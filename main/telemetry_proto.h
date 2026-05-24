/*
 * telemetry_proto.h — NDJSON line protocol (scanner ↔ CrowPanel)
 */

#ifndef TELEMETRY_PROTO_H
#define TELEMETRY_PROTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define TELEMETRY_LINE_MAX    256
#define TELEMETRY_FIELD_MAX   64
#define TELEMETRY_VENDOR_MAX  48
#define TELEMETRY_MAC_MAX     18

typedef enum {
    TELEM_TYPE_WIFI = 0,
    TELEM_TYPE_BLE,
    TELEM_TYPE_CLASSIC,
    TELEM_TYPE_NRF,
    TELEM_TYPE_UNKNOWN,
} telemetry_type_t;

typedef struct {
    telemetry_type_t type;
    char             mac[TELEMETRY_MAC_MAX];
    int8_t           rssi;
    uint32_t         ts_ms;
    char             field1[TELEMETRY_FIELD_MAX];
    char             field2[TELEMETRY_FIELD_MAX];
    char             field3[TELEMETRY_FIELD_MAX];
    char             vendor[TELEMETRY_VENDOR_MAX];
} telemetry_event_t;

const char *telemetry_type_str(telemetry_type_t type);
telemetry_type_t telemetry_type_from_str(const char *s);

#endif /* TELEMETRY_PROTO_H */
