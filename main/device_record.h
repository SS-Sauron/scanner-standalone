/*
 * device_record.h — structured discovery entry for BLE / Classic / nRF
 */

#ifndef DEVICE_RECORD_H
#define DEVICE_RECORD_H

#include <stdint.h>
#include <stdbool.h>

#define DEVICE_NAME_MAX         32
#define DEVICE_MFG_DATA_MAX     8
#define DEVICE_ADV_RAW_MAX      31

typedef enum {
    SCAN_RADIO_BLE = 0,
    SCAN_RADIO_CLASSIC = 1,
    SCAN_RADIO_NRF24 = 2,
} scan_radio_t;

typedef struct {
    scan_radio_t radio;
    uint8_t      addr[6];
    uint8_t      addr_type;     /* BLE addr type; 0 for Classic */
    int8_t       rssi;
    char         name[DEVICE_NAME_MAX + 1];
    uint8_t      name_len;
    uint32_t     cod;           /* Classic Class of Device */
    uint16_t     appearance;    /* BLE appearance if present */
    uint16_t     mfg_company_id;
    uint8_t      mfg_data_len;
    uint8_t      mfg_data[DEVICE_MFG_DATA_MAX];
    uint8_t      adv_raw_len;
    uint8_t      adv_raw[DEVICE_ADV_RAW_MAX];
    uint32_t     last_seen_ms;
    bool         valid;
} device_record_t;

#endif /* DEVICE_RECORD_H */
