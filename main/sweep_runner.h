/*
 * sweep_runner.h — sweep_all macro orchestration
 */

#ifndef SWEEP_RUNNER_H
#define SWEEP_RUNNER_H

#include <stdbool.h>

#include "esp_err.h"

void sweep_runner_set_include_nrf(bool include);
esp_err_t sweep_runner_run(void);

#endif /* SWEEP_RUNNER_H */
