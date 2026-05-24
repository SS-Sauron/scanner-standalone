/*
 * telemetry_proto.c — shared type string helpers
 */

#include "telemetry_proto.h"

#include <string.h>

const char *telemetry_type_str(telemetry_type_t type)
{
    switch (type) {
    case TELEM_TYPE_WIFI:     return "WIFI";
    case TELEM_TYPE_BLE:      return "BLE";
    case TELEM_TYPE_CLASSIC:  return "CLASSIC";
    case TELEM_TYPE_NRF:      return "NRF";
    default:                  return "UNKNOWN";
    }
}

telemetry_type_t telemetry_type_from_str(const char *s)
{
    if (s == NULL) {
        return TELEM_TYPE_UNKNOWN;
    }
    if (strcmp(s, "WIFI") == 0) {
        return TELEM_TYPE_WIFI;
    }
    if (strcmp(s, "BLE") == 0) {
        return TELEM_TYPE_BLE;
    }
    if (strcmp(s, "CLASSIC") == 0) {
        return TELEM_TYPE_CLASSIC;
    }
    if (strcmp(s, "NRF") == 0) {
        return TELEM_TYPE_NRF;
    }
    return TELEM_TYPE_UNKNOWN;
}
