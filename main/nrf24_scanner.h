/*
 * nrf24_scanner.h — nRF24L01 presence probe and future scan hook
 */

#ifndef NRF24_SCANNER_H
#define NRF24_SCANNER_H

#include <stdbool.h>

#include "esp_err.h"

void nrf24_probe_on_boot(void);
bool nrf24_is_attached(void);

esp_err_t nrf24_scanner_run(void);

#endif /* NRF24_SCANNER_H */
