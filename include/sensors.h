#pragma once

#include "ble_pet_collar_service.h"

/**
 * Initialize LIS3DH (I²C) and DS18B20 (1-Wire/UART) sensors.
 * Returns 0 if both ready, negative errno otherwise.
 * Caller should log a warning but continue — partial sensor data is OK.
 */
int sensors_init(void);

/**
 * Read DS18B20 temperature and fill health payload.
 * Sets flags bit2 (temp valid) on success; clears HR/SpO2 bits (no MAX30102).
 * Blocks ~750 ms for DS18B20 12-bit conversion.
 */
int sensors_get_health(struct ble_health_t *out);

/**
 * Read LIS3DH acceleration, classify behavior, and update step count.
 * Non-blocking (fast I²C read).
 */
int sensors_get_behavior(struct ble_behavior_t *out);
