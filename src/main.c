#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "ble.h"
#include "ble_pet_collar_service.h"
#include "sensors.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Stub location — Taipei (no GNSS on collar, filled by phone app) */
static const struct ble_location_t stub_location = {
	.lat_e7      = 250330000,
	.lon_e7      = 1215654000,
	.alt_m       = 10,
	.accuracy_cm = 150,
	.timestamp   = 1719187200,
	.fix_type    = 2,
	.mode        = 1,
};

/* Delayed work items */
static struct k_work_delayable health_work;
static struct k_work_delayable location_work;
static struct k_work_delayable status_work;
static struct k_work_delayable behavior_work;

/* Track runtime for status uptime field */
static uint32_t boot_tick;

static void health_work_fn(struct k_work *work)
{
	struct ble_health_t health = {0};
	/* blocks ~750 ms for DS18B20 12-bit conversion — acceptable at 30s interval */
	sensors_get_health(&health);

	int err = ble_pcs_notify_health(&health);
	if (err && err != -ENOTCONN) {
		LOG_WRN("health notify err: %d", err);
	}
	k_work_reschedule(&health_work, K_SECONDS(30));
}

static void location_work_fn(struct k_work *work)
{
	int err = ble_pcs_notify_location(&stub_location);
	if (err && err != -ENOTCONN) {
		LOG_WRN("location notify err: %d", err);
	}
	k_work_reschedule(&location_work, K_SECONDS(30));
}

static void status_work_fn(struct k_work *work)
{
	uint32_t uptime_s = (k_uptime_get_32() - boot_tick) / 1000;
	struct ble_status_t status = {
		.state       = STATE_CONNECTED,
		.battery_pct = 85,  /* stub — no fuel gauge in PoC */
		.rssi        = 0,
		.uptime      = {
			(uint8_t)(uptime_s & 0xFF),
			(uint8_t)((uptime_s >> 8) & 0xFF),
			(uint8_t)((uptime_s >> 16) & 0xFF),
		},
	};

	int err = ble_pcs_notify_status(&status);
	if (err && err != -ENOTCONN) {
		LOG_WRN("status notify err: %d", err);
	}
	k_work_reschedule(&status_work, K_SECONDS(5));
}

static void behavior_work_fn(struct k_work *work)
{
	struct ble_behavior_t behavior = {0};
	sensors_get_behavior(&behavior);

	int err = ble_pcs_notify_behavior(&behavior);
	if (err && err != -ENOTCONN) {
		LOG_WRN("behavior notify err: %d", err);
	}
	k_work_reschedule(&behavior_work, K_SECONDS(5));
}

int main(void)
{
	LOG_INF("PetCollar-X1 booting...");
	boot_tick = k_uptime_get_32();

	int err = ble_init();
	if (err) {
		LOG_ERR("ble_init failed: %d", err);
		return err;
	}

	err = ble_pcs_init();
	if (err) {
		LOG_ERR("ble_pcs_init failed: %d", err);
		return err;
	}

	/* Sensor init — non-fatal: BLE still works even if sensors fail */
	err = sensors_init();
	if (err) {
		LOG_WRN("sensors_init: %d (BLE running, sensor data unavailable)", err);
	}

	ble_start_advertising();

	k_work_init_delayable(&health_work,   health_work_fn);
	k_work_init_delayable(&location_work, location_work_fn);
	k_work_init_delayable(&status_work,   status_work_fn);
	k_work_init_delayable(&behavior_work, behavior_work_fn);

	k_work_reschedule(&health_work,   K_SECONDS(5));
	k_work_reschedule(&location_work, K_SECONDS(5));
	k_work_reschedule(&status_work,   K_SECONDS(5));
	k_work_reschedule(&behavior_work, K_SECONDS(5));

	LOG_INF("PetCollar-X1 ready");
	return 0;
}
