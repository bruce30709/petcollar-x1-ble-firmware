#include <zephyr/ztest.h>
#include "ble_pet_collar_service.h"

ZTEST(ble_structs, test_location_size)
{
	zassert_equal(sizeof(struct ble_location_t), 18,
		"ble_location_t must be 18 bytes, got %zu",
		sizeof(struct ble_location_t));
}

ZTEST(ble_structs, test_health_size)
{
	zassert_equal(sizeof(struct ble_health_t), 8,
		"ble_health_t must be 8 bytes, got %zu",
		sizeof(struct ble_health_t));
}

ZTEST(ble_structs, test_behavior_size)
{
	zassert_equal(sizeof(struct ble_behavior_t), 6,
		"ble_behavior_t must be 6 bytes, got %zu",
		sizeof(struct ble_behavior_t));
}

ZTEST(ble_structs, test_command_size)
{
	zassert_equal(sizeof(struct ble_command_t), 4,
		"ble_command_t must be 4 bytes, got %zu",
		sizeof(struct ble_command_t));
}

ZTEST(ble_structs, test_status_size)
{
	zassert_equal(sizeof(struct ble_status_t), 6,
		"ble_status_t must be 6 bytes, got %zu",
		sizeof(struct ble_status_t));
}

ZTEST(ble_structs, test_config_size)
{
	zassert_equal(sizeof(struct ble_config_t), 20,
		"ble_config_t must be 20 bytes, got %zu",
		sizeof(struct ble_config_t));
}

ZTEST_SUITE(ble_structs, NULL, NULL, NULL, NULL, NULL);
