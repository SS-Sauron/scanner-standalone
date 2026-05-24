/*
 * board_config.h — pins and optional peripherals for scanner-standalone
 *
 * DOIT ESP32 DevKit V1 defaults. Change NRF24 pins when using an external
 * nRF24L01+ module on a custom shield.
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "driver/gpio.h"
#include "driver/uart.h"

/* On-board scan-active LED (active high on typical DOIT boards) */
#define SCAN_LED_GPIO           GPIO_NUM_2
#define SCAN_LED_LEVEL_ON       1
#define SCAN_LED_LEVEL_OFF      0

/* -------------------------------------------------------------------------- */
/* Optional nRF24L01 (2.4 GHz SPI radio)                                      */
/* -------------------------------------------------------------------------- */

#define NRF24L01_PRESENT        false

#define NRF24L01_SCLK_GPIO      GPIO_NUM_NC
#define NRF24L01_MOSI_GPIO      GPIO_NUM_NC
#define NRF24L01_MISO_GPIO      GPIO_NUM_NC
#define NRF24L01_CS_GPIO        GPIO_NUM_NC
#define NRF24L01_CE_GPIO       GPIO_NUM_NC

/* nRF24 CONFIG register (read during probe) */
#define NRF24L01_REG_CONFIG     0x08

/* -------------------------------------------------------------------------- */
/* UART telemetry link to CrowPanel (UART1-OUT: cross-wire TX/RX + GND)       */
/* -------------------------------------------------------------------------- */

#define TELEMETRY_UART_NUM      UART_NUM_2
#define TELEMETRY_UART_TX_GPIO  GPIO_NUM_17
#define TELEMETRY_UART_RX_GPIO  GPIO_NUM_16
#define TELEMETRY_UART_BAUD     460800

/* -------------------------------------------------------------------------- */
/* ESP-NOW link to CrowPanel (Wi-Fi STA MAC — not Bluetooth MAC)              */
/* -------------------------------------------------------------------------- */

#define ESPNOW_WIFI_CHANNEL     1

/* CrowPanel ESP32-S3 Wi-Fi MAC (ttyACM0 / esptool) */
#define ESPNOW_PEER_PANEL_MAC   {0x34, 0xCD, 0xB0, 0x0A, 0x5A, 0xE8}

#endif /* BOARD_CONFIG_H */
