#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "ble.h"
#include "ble_pet_collar_service.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* ---- Stub data (compile-time constants) ---- */

/* Taipei: 25.0330°N, 121.5654°E */
static const struct ble_location_t stub_location = {
    .lat_e7      = 250330000,
    .lon_e7      = 1215654000,
    .alt_m       = 10,
    .accuracy_cm = 150,
    .timestamp   = 1719187200,  /* 2024-06-24 00:00:00 UTC */
    .fix_type    = 2,           /* 3D fix */
    .mode        = 1,           /* normal */
};

/* HR=72 BPM, SpO2=98%, Temp=38.5°C, all valid */
static const struct ble_health_t stub_health = {
    .heart_rate     = 720,   /* 72.0 BPM × 10 */
    .spo2           = 98,
    .temperature    = 385,   /* 38.5°C × 10 */
    .signal_quality = 85,
    .flags          = 0x07,  /* bit0|bit1|bit2 = HR+SpO2+Temp valid */
};

static const struct ble_behavior_t stub_behavior = {
    .behavior   = BEHAVIOR_WALKING,
    .confidence = 80,
    .steps      = 1234,
};

static const struct ble_status_t stub_status = {
    .state       = STATE_ADVERTISING,
    .battery_pct = 85,
    .rssi        = 0,
    .uptime      = {0, 0, 0},
};

/* ---- Delayed work items ---- */

static struct k_work_delayable health_work;
static struct k_work_delayable location_work;
static struct k_work_delayable status_work;
static struct k_work_delayable behavior_work;

static void health_work_fn(struct k_work *work)
{
    int err = ble_pcs_notify_health(&stub_health);
    if (err && err != -ENOTCONN) {
        LOG_WRN("health notify err: %d", err);
    }
    k_work_reschedule(&health_work, K_SECONDS(60));
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
    int err = ble_pcs_notify_status(&stub_status);
    if (err && err != -ENOTCONN) {
        LOG_WRN("status notify err: %d", err);
    }
    k_work_reschedule(&status_work, K_SECONDS(5));
}

static void behavior_work_fn(struct k_work *work)
{
    int err = ble_pcs_notify_behavior(&stub_behavior);
    if (err && err != -ENOTCONN) {
        LOG_WRN("behavior notify err: %d", err);
    }
    k_work_reschedule(&behavior_work, K_SECONDS(120));
}

/* ---- Boot sequence ---- */

int main(void)
{
    LOG_INF("PetCollar-X1 booting...");

    int err = ble_init();
    if (err) {
        LOG_ERR("ble_init failed: %d", err);
        /* Zephyr ignores main() return value — device requires reset to recover */
        return err;
    }

    err = ble_pcs_init();
    if (err) {
        LOG_ERR("ble_pcs_init failed: %d", err);
        /* Zephyr ignores main() return value — device requires reset to recover */
        return err;
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
