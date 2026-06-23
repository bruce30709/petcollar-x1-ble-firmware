#pragma once

#include <stdbool.h>

int  ble_init(void);
void ble_start_advertising(void);
bool ble_is_connected(void);
