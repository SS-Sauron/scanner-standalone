/*
 * espnow_proto.h — ESP-NOW command envelope (scanner ↔ panel ↔ attacker)
 *
 * Peers use Wi-Fi STA MAC addresses (see boot log), not Bluetooth MAC.
 */

#pragma once

#include <stdint.h>

#define ESPNOW_PROTO_VERSION     0x01

#define CMD_SEND_DEVICE        0x01
#define CMD_SET_TARGET         0x02
#define CMD_LAUNCH_ATTACK      0x03

#define DEVICE_TYPE_CLASSIC    0
#define DEVICE_TYPE_BLE        1
#define DEVICE_TYPE_WIFI       2

typedef struct __attribute__((packed)) {
    uint8_t  bda[6];
    char     name[32];
    int8_t   rssi;
    uint32_t cod;
    uint8_t  type;
    int8_t   tx_power;
    uint16_t company_id;
    char     vendor[32];
} device_info_t;

typedef struct __attribute__((packed)) {
    uint8_t  version;
    uint8_t  reserved[3];
    uint8_t  cmd_id;
    union {
        device_info_t device;
        uint8_t       mac[6];
    } payload;
} command_t;
