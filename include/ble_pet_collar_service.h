#pragma once

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <stdint.h>

/* Pet Collar Service — Custom 128-bit UUIDs
 * Base: A1B2C3D4-xxxx-1000-8000-00805F9B34FB
 * xxxx varies per characteristic (see spec §4.6)
 */
#define PCS_UUID_ENCODE(xxxx) \
    BT_UUID_128_ENCODE(0xA1B2C3D4, (xxxx), 0x1000, 0x8000, 0x00805F9B34FB)

#define PCS_UUID_SERVICE_VAL   PCS_UUID_ENCODE(0x0000)
#define PCS_UUID_LOCATION_VAL  PCS_UUID_ENCODE(0x0101)
#define PCS_UUID_HEALTH_VAL    PCS_UUID_ENCODE(0x0102)
#define PCS_UUID_BEHAVIOR_VAL  PCS_UUID_ENCODE(0x0103)
#define PCS_UUID_COMMAND_VAL   PCS_UUID_ENCODE(0x0104)
#define PCS_UUID_STATUS_VAL    PCS_UUID_ENCODE(0x0105)
#define PCS_UUID_CONFIG_VAL    PCS_UUID_ENCODE(0x0106)

#define PCS_UUID_SERVICE   BT_UUID_DECLARE_128(PCS_UUID_SERVICE_VAL)
#define PCS_UUID_LOCATION  BT_UUID_DECLARE_128(PCS_UUID_LOCATION_VAL)
#define PCS_UUID_HEALTH    BT_UUID_DECLARE_128(PCS_UUID_HEALTH_VAL)
#define PCS_UUID_BEHAVIOR  BT_UUID_DECLARE_128(PCS_UUID_BEHAVIOR_VAL)
#define PCS_UUID_COMMAND   BT_UUID_DECLARE_128(PCS_UUID_COMMAND_VAL)
#define PCS_UUID_STATUS    BT_UUID_DECLARE_128(PCS_UUID_STATUS_VAL)
#define PCS_UUID_CONFIG    BT_UUID_DECLARE_128(PCS_UUID_CONFIG_VAL)

/* Behavior enum — matches spec §4.5 */
enum behavior_type {
    BEHAVIOR_SLEEPING   = 0,
    BEHAVIOR_RESTING    = 1,
    BEHAVIOR_WALKING    = 2,
    BEHAVIOR_RUNNING    = 3,
    BEHAVIOR_PLAYING    = 4,
    BEHAVIOR_SCRATCHING = 5,
    BEHAVIOR_UNKNOWN    = 6,
};

/* Command enum — matches spec §4.6 */
enum pcs_command {
    CMD_FIND_MODE_ON  = 0x01,
    CMD_FIND_MODE_OFF = 0x02,
    CMD_SYNC_TIME     = 0x03,
    CMD_SET_CONFIG    = 0x04,
    CMD_REBOOT        = 0x05,  /* stub: log only, no reboot */
    CMD_DFU_MODE      = 0x06,  /* stub: log only, no DFU */
};

/* 18 bytes — Location characteristic payload */
struct ble_location_t {
    int32_t  lat_e7;        /* latitude  × 10^7 */
    int32_t  lon_e7;        /* longitude × 10^7 */
    int16_t  alt_m;
    uint16_t accuracy_cm;
    uint32_t timestamp;
    uint8_t  fix_type;      /* 0=invalid 1=2D 2=3D 3=GNSS+assisted */
    uint8_t  mode;          /* 0=power-save 1=normal 2=high-freq */
} __packed;

/* 8 bytes — Health characteristic payload */
struct ble_health_t {
    uint16_t heart_rate;    /* BPM × 10 */
    uint8_t  spo2;          /* % 0-100 */
    int16_t  temperature;   /* 0.1°C  e.g. 385 = 38.5°C */
    uint8_t  signal_quality;
    uint16_t flags;         /* bit0=HR valid bit1=SpO2 valid bit2=Temp valid */
} __packed;

/* 6 bytes — Behavior characteristic payload */
struct ble_behavior_t {
    uint8_t  behavior;      /* enum behavior_type */
    uint8_t  confidence;    /* 0-100 */
    uint32_t steps;
} __packed;

/* 4 bytes — Command characteristic payload (Write Without Response) */
struct ble_command_t {
    uint8_t  cmd;           /* enum pcs_command */
    uint8_t  param1;
    uint16_t param2;
} __packed;

/* Device state enum — for ble_status_t.state */
enum device_state {
    STATE_IDLE        = 0,
    STATE_ADVERTISING = 1,
    STATE_CONNECTED   = 2,
    STATE_LOCATING    = 3,
};

/* 6 bytes — Device Status characteristic payload */
struct ble_status_t {
    uint8_t  state;
    uint8_t  battery_pct;
    int8_t   rssi;          /* dBm, typical range -100 to 0 */
    uint8_t  uptime[3];     /* seconds, little-endian 24-bit */
} __packed;

/* 20 bytes — Configuration characteristic payload */
struct ble_config_t {
    uint16_t gnss_interval_s;
    uint16_t health_interval_s;
    uint16_t alert_hr_max;
    uint16_t alert_temp_max;
    uint32_t geofence_radius_m;
    uint8_t  flags[8];
} __packed;

/* API */
int ble_pcs_init(void);
int ble_pcs_notify_health(const struct ble_health_t *data);
int ble_pcs_notify_location(const struct ble_location_t *data);
int ble_pcs_notify_behavior(const struct ble_behavior_t *data);
int ble_pcs_notify_status(const struct ble_status_t *data);
