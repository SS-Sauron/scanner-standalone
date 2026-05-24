/*
 * bt_stack.h — Bluedroid BTDM init/shutdown
 */

#ifndef BT_STACK_H
#define BT_STACK_H

#include <stdbool.h>

#include "esp_err.h"

esp_err_t bt_stack_init(void);
void bt_stack_shutdown(void);
bool bt_stack_is_up(void);

#endif /* BT_STACK_H */
