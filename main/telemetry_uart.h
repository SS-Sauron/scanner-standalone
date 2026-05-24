#ifndef TELEMETRY_UART_H
#define TELEMETRY_UART_H

#include <stddef.h>

#include "esp_err.h"

esp_err_t telemetry_uart_init(void);
esp_err_t telemetry_uart_write_line(const char *line);

#endif
