# BLE GATT Profile Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a fully functional BLE GATT peripheral on nRF5340 (Zephyr/NCS) that advertises as `PetCollar-X1`, exposes all 6 Pet Collar Service characteristics with stub data, and is verifiable with nRF Connect for Mobile.

**Architecture:** Two-layer BLE design — `ble.c` owns connection lifecycle (advertising, MTU/PHY negotiation), `ble_pet_collar_service.c` owns all GATT characteristic definitions via `BT_GATT_SERVICE_DEFINE`. `main.c` drives stub data into the service layer via Zephyr workqueue. Sensors/LED/power are stubs returning fixed values.

**Tech Stack:** nRF Connect SDK v2.7+, Zephyr v3.7 LTS, nRF5340 (Fanstel EVM-BT40), West + CMake + Kconfig, Zephyr BT Host Stack, nRF Connect for Mobile (test tool), ztest on native_sim for struct validation.

---

## File Map

| Action | Path | Responsibility |
|--------|------|----------------|
| Modify | `CMakeLists.txt` | Add `ble_pet_collar_service.c` source |
| Create | `prj.conf` | BLE Kconfig |
| Create | `boards/nrf5340dk_nrf5340_cpuapp.overlay` | Minimal device tree overlay |
| Create | `include/ble.h` | Connection manager API |
| Create | `include/ble_pet_collar_service.h` | GATT service API + packed structs |
| Create | `src/ble.c` | Advertising, connection, MTU, PHY |
| Create | `src/ble_pet_collar_service.c` | `BT_GATT_SERVICE_DEFINE` + notify + handlers |
| Create | `src/main.c` | Init sequence + workqueue stub pump |
| Create | `src/sensors.c` | Stub: returns fixed sensor values |
| Create | `src/led.c` | Stub: log-only LED control |
| Create | `src/power.c` | Stub: log-only power management |
| Create | `tests/ble_structs/CMakeLists.txt` | Test build config |
| Create | `tests/ble_structs/prj.conf` | Test Kconfig |
| Create | `tests/ble_structs/src/main.c` | ztest struct size + endian checks |

---

## Task 1: Build Infrastructure

**Files:**
- Modify: `CMakeLists.txt`
- Create: `prj.conf`
- Create: `boards/nrf5340dk_nrf5340_cpuapp.overlay`

- [ ] **Step 1: Update CMakeLists.txt**

Replace the existing `CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(evm_bt40_firmware)

target_sources(app PRIVATE
    src/main.c
    src/ble.c
    src/ble_pet_collar_service.c
    src/sensors.c
    src/led.c
    src/power.c
)

target_include_directories(app PRIVATE include)
```

- [ ] **Step 2: Create prj.conf**

Create `prj.conf`:

```kconfig
# BLE
CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_DEVICE_NAME="PetCollar-X1"
CONFIG_BT_DEVICE_APPEARANCE=833
CONFIG_BT_DIS=y
CONFIG_BT_DIS_PNP=n
CONFIG_BT_DIS_MODEL="PetCollar-X1"
CONFIG_BT_DIS_MANUF="PetCollar"
CONFIG_BT_BAS=y
CONFIG_BT_GATT_CLIENT=n
CONFIG_BT_MAX_CONN=1
CONFIG_BT_L2CAP_TX_MTU=247
CONFIG_BT_BUF_ACL_TX_SIZE=251
CONFIG_BT_BUF_ACL_RX_SIZE=251

# Logging
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_LOG_BACKEND_UART=y

# System
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048
CONFIG_MAIN_STACK_SIZE=2048
```

- [ ] **Step 3: Create minimal board overlay**

Create `boards/nrf5340dk_nrf5340_cpuapp.overlay`:

```dts
/ {
	chosen {
		zephyr,console = &uart0;
		zephyr,shell-uart = &uart0;
	};
};
```

- [ ] **Step 4: Create empty stub source files so CMake can resolve targets**

Create `src/sensors.c`:
```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensors, LOG_LEVEL_INF);
```

Create `src/led.c`:
```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(led, LOG_LEVEL_INF);
```

Create `src/power.c`:
```c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);
```

Create `src/ble.c` (empty stub so CMake resolves):
```c
#include <zephyr/kernel.h>
```

Create `src/ble_pet_collar_service.c` (empty stub):
```c
#include <zephyr/kernel.h>
```

Create `src/main.c` (empty stub):
```c
#include <zephyr/kernel.h>
int main(void) { return 0; }
```

Create `include/ble.h` (empty stub):
```c
#pragma once
```

Create `include/ble_pet_collar_service.h` (empty stub):
```c
#pragma once
```

- [ ] **Step 5: Verify the project builds**

```bash
cd /mnt/e/board/evm-bt40-firmware
west build -b nrf5340dk/nrf5340/cpuapp --sysbuild
```

Expected: `build/zephyr/zephyr.elf` generated with no errors.
If West environment not activated: `source ~/ncs/zephyr/zephyr-env.sh` first.

- [ ] **Step 6: Commit**

```bash
git init   # only if not already a repo
git add CMakeLists.txt prj.conf boards/ src/ include/
git commit -m "feat: add project scaffold with empty stubs"
```

---

## Task 2: Header Files — Data Structs & APIs

**Files:**
- Create: `include/ble_pet_collar_service.h`
- Create: `include/ble.h`

- [ ] **Step 1: Write `include/ble_pet_collar_service.h`**

```c
#pragma once

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <stdint.h>

/* Pet Collar Service — Custom 128-bit UUIDs
 * Base: A1B2C3D4-xxxx-1000-8000-00805F9B34FB
 * xxxx varies per characteristic (see spec §4.6)
 */
#define PCS_UUID_ENCODE(xxxx) \
    BT_UUID_128_ENCODE(0xA1B2C3D4, (xxxx), 0x1000, 0x8000, 0x00805F9B34FBULL)

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

/* 6 bytes — Device Status characteristic payload */
struct ble_status_t {
    uint8_t  state;
    uint8_t  battery_pct;
    uint8_t  rssi;
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
```

- [ ] **Step 2: Write `include/ble.h`**

```c
#pragma once

#include <stdbool.h>

int  ble_init(void);
void ble_start_advertising(void);
bool ble_is_connected(void);
```

- [ ] **Step 3: Verify headers compile cleanly**

```bash
west build -b nrf5340dk/nrf5340/cpuapp --sysbuild
```

Expected: build succeeds (stubs include the headers but don't call anything yet).

- [ ] **Step 4: Commit**

```bash
git add include/
git commit -m "feat: add BLE GATT header files with packed structs and UUIDs"
```

---

## Task 3: Struct Size Tests

**Files:**
- Create: `tests/ble_structs/CMakeLists.txt`
- Create: `tests/ble_structs/prj.conf`
- Create: `tests/ble_structs/src/main.c`

- [ ] **Step 1: Create `tests/ble_structs/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_ble_structs)

target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE ../../include)
```

- [ ] **Step 2: Create `tests/ble_structs/prj.conf`**

```kconfig
CONFIG_ZTEST=y
CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
```

- [ ] **Step 3: Write the failing tests in `tests/ble_structs/src/main.c`**

```c
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
```

- [ ] **Step 4: Run tests on native_sim — expect PASS**

```bash
cd /mnt/e/board/evm-bt40-firmware
west build -b native_sim tests/ble_structs && ./build/zephyr/zephyr.exe
```

Expected output:
```
Running TESTSUITE ble_structs
===================================================================
START - test_location_size
 PASS - test_location_size in 0ms
START - test_health_size
 PASS - test_health_size in 0ms
START - test_behavior_size
 PASS - test_behavior_size in 0ms
START - test_command_size
 PASS - test_command_size in 0ms
START - test_status_size
 PASS - test_status_size in 0ms
START - test_config_size
 PASS - test_config_size in 0ms
===================================================================
TESTSUITE ble_structs succeeded
```

- [ ] **Step 5: Commit**

```bash
git add tests/
git commit -m "test: add struct size assertions for all BLE GATT payloads"
```

---

## Task 4: BLE Connection Manager (`ble.c`)

**Files:**
- Modify: `src/ble.c`

- [ ] **Step 1: Implement `src/ble.c`**

```c
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include "ble.h"

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

static struct bt_conn *current_conn;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void mtu_exchange_cb(struct bt_conn *conn, uint8_t err,
                            struct bt_gatt_exchange_params *params)
{
    if (err) {
        LOG_WRN("MTU exchange failed: %d", err);
    } else {
        LOG_INF("MTU exchanged: %u", bt_gatt_get_mtu(conn));
    }
}

static struct bt_gatt_exchange_params mtu_params = {
    .func = mtu_exchange_cb,
};

static void phy_update_cb(struct bt_conn *conn,
                          const struct bt_conn_le_phy_info *param)
{
    LOG_INF("PHY updated: TX=%u RX=%u", param->tx_phy, param->rx_phy);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed: %u", err);
        return;
    }

    current_conn = bt_conn_ref(conn);
    LOG_INF("Connected");

    /* Request 2M PHY */
    static const struct bt_conn_le_phy_param phy_param = {
        .options = BT_CONN_LE_PHY_OPT_NONE,
        .pref_tx_phy = BT_GAP_LE_PHY_2M,
        .pref_rx_phy = BT_GAP_LE_PHY_2M,
    };
    bt_conn_le_phy_update(conn, &phy_param);

    /* Request MTU 247 */
    bt_gatt_exchange_mtu(conn, &mtu_params);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected, reason: %u", reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    ble_start_advertising();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected    = connected,
    .disconnected = disconnected,
    .le_phy_updated = phy_update_cb,
};

int ble_init(void)
{
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("bt_enable failed: %d", err);
        return err;
    }
    LOG_INF("BLE enabled");
    return 0;
}

void ble_start_advertising(void)
{
    int err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising start failed: %d", err);
        return;
    }
    LOG_INF("Advertising started as \"%s\"", CONFIG_BT_DEVICE_NAME);
}

bool ble_is_connected(void)
{
    return current_conn != NULL;
}
```

- [ ] **Step 2: Build to verify no compile errors**

```bash
cd /mnt/e/board/evm-bt40-firmware
west build -b nrf5340dk/nrf5340/cpuapp --sysbuild
```

Expected: builds successfully.

- [ ] **Step 3: Commit**

```bash
git add src/ble.c
git commit -m "feat: implement BLE connection manager with advertising, MTU, PHY"
```

---

## Task 5: Pet Collar GATT Service (`ble_pet_collar_service.c`)

**Files:**
- Modify: `src/ble_pet_collar_service.c`

- [ ] **Step 1: Implement `src/ble_pet_collar_service.c`**

```c
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include "ble_pet_collar_service.h"

LOG_MODULE_REGISTER(pcs, LOG_LEVEL_INF);

/* CCCD tracking — one flag per notifiable characteristic */
static bool notify_location_enabled;
static bool notify_health_enabled;
static bool notify_behavior_enabled;
static bool notify_status_enabled;

/* RAM-backed configuration (persists for connection session) */
static struct ble_config_t g_config = {
    .gnss_interval_s   = 1800,  /* 30 minutes */
    .health_interval_s = 3600,  /* 60 minutes */
    .alert_hr_max      = 200,
    .alert_temp_max    = 410,   /* 41.0°C */
    .geofence_radius_m = 500,
};

/* ---- CCCD write callbacks ---- */

static void location_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_location_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Location notify: %s", notify_location_enabled ? "on" : "off");
}

static void health_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_health_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Health notify: %s", notify_health_enabled ? "on" : "off");
}

static void behavior_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_behavior_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Behavior notify: %s", notify_behavior_enabled ? "on" : "off");
}

static void status_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_status_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Status notify: %s", notify_status_enabled ? "on" : "off");
}

/* ---- Command write handler ---- */

static ssize_t cmd_write_cb(struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            const void *buf, uint16_t len,
                            uint16_t offset, uint8_t flags)
{
    if (len != sizeof(struct ble_command_t)) {
        LOG_WRN("CMD: unexpected length %u", len);
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    const struct ble_command_t *cmd = buf;
    LOG_INF("CMD: 0x%02x param1=0x%02x param2=0x%04x",
            cmd->cmd, cmd->param1, cmd->param2);

    /* Stub: all commands are logged but not executed */
    switch (cmd->cmd) {
    case CMD_FIND_MODE_ON:
        LOG_INF("CMD_FIND_MODE_ON (stub)");
        break;
    case CMD_FIND_MODE_OFF:
        LOG_INF("CMD_FIND_MODE_OFF (stub)");
        break;
    case CMD_SYNC_TIME:
        LOG_INF("CMD_SYNC_TIME ts=%u (stub)", cmd->param2);
        break;
    case CMD_SET_CONFIG:
        LOG_INF("CMD_SET_CONFIG (stub)");
        break;
    case CMD_REBOOT:
        LOG_WRN("CMD_REBOOT requested — stub, NOT rebooting");
        break;
    case CMD_DFU_MODE:
        LOG_WRN("CMD_DFU_MODE requested — stub, NOT entering DFU");
        break;
    default:
        LOG_WRN("Unknown CMD: 0x%02x", cmd->cmd);
        break;
    }

    return len;
}

/* ---- Config read/write handlers ---- */

static ssize_t config_read_cb(struct bt_conn *conn,
                              const struct bt_gatt_attr *attr,
                              void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             &g_config, sizeof(g_config));
}

static ssize_t config_write_cb(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               const void *buf, uint16_t len,
                               uint16_t offset, uint8_t flags)
{
    if (offset + len > sizeof(g_config)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    memcpy((uint8_t *)&g_config + offset, buf, len);
    LOG_INF("Config updated");
    return len;
}

/* ---- Static GATT table ---- */

BT_GATT_SERVICE_DEFINE(pet_collar_svc,
    BT_GATT_PRIMARY_SERVICE(PCS_UUID_SERVICE),

    /* Location — Notify */
    BT_GATT_CHARACTERISTIC(PCS_UUID_LOCATION,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL, NULL, NULL),
    BT_GATT_CCC(location_ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Health — Notify */
    BT_GATT_CHARACTERISTIC(PCS_UUID_HEALTH,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL, NULL, NULL),
    BT_GATT_CCC(health_ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Behavior — Notify */
    BT_GATT_CHARACTERISTIC(PCS_UUID_BEHAVIOR,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL, NULL, NULL),
    BT_GATT_CCC(behavior_ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Command — Write Without Response */
    BT_GATT_CHARACTERISTIC(PCS_UUID_COMMAND,
        BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_WRITE,
        NULL, cmd_write_cb, NULL),

    /* Device Status — Notify */
    BT_GATT_CHARACTERISTIC(PCS_UUID_STATUS,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL, NULL, NULL),
    BT_GATT_CCC(status_ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Configuration — Read + Write */
    BT_GATT_CHARACTERISTIC(PCS_UUID_CONFIG,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        config_read_cb, config_write_cb, &g_config),
);

/* ---- Notify helpers ---- */

/* Returns the attribute pointer for a characteristic value by searching the
 * static GATT table by UUID. */
static const struct bt_gatt_attr *find_attr(const struct bt_uuid *uuid)
{
    return bt_gatt_find_by_uuid(pet_collar_svc.attrs,
                                pet_collar_svc.attr_count, uuid);
}

int ble_pcs_notify_health(const struct ble_health_t *data)
{
    if (!notify_health_enabled) {
        return -ENOTCONN;
    }
    const struct bt_gatt_attr *attr = find_attr(PCS_UUID_HEALTH);
    if (!attr) {
        return -ENOENT;
    }
    return bt_gatt_notify(NULL, attr, data, sizeof(*data));
}

int ble_pcs_notify_location(const struct ble_location_t *data)
{
    if (!notify_location_enabled) {
        return -ENOTCONN;
    }
    const struct bt_gatt_attr *attr = find_attr(PCS_UUID_LOCATION);
    if (!attr) {
        return -ENOENT;
    }
    return bt_gatt_notify(NULL, attr, data, sizeof(*data));
}

int ble_pcs_notify_behavior(const struct ble_behavior_t *data)
{
    if (!notify_behavior_enabled) {
        return -ENOTCONN;
    }
    const struct bt_gatt_attr *attr = find_attr(PCS_UUID_BEHAVIOR);
    if (!attr) {
        return -ENOENT;
    }
    return bt_gatt_notify(NULL, attr, data, sizeof(*data));
}

int ble_pcs_notify_status(const struct ble_status_t *data)
{
    if (!notify_status_enabled) {
        return -ENOTCONN;
    }
    const struct bt_gatt_attr *attr = find_attr(PCS_UUID_STATUS);
    if (!attr) {
        return -ENOENT;
    }
    return bt_gatt_notify(NULL, attr, data, sizeof(*data));
}

int ble_pcs_init(void)
{
    LOG_INF("Pet Collar Service registered");
    return 0;
}
```

- [ ] **Step 2: Build to verify no compile errors**

```bash
west build -b nrf5340dk/nrf5340/cpuapp --sysbuild
```

Expected: builds successfully.

- [ ] **Step 3: Commit**

```bash
git add src/ble_pet_collar_service.c
git commit -m "feat: implement Pet Collar GATT service with all 6 characteristics"
```

---

## Task 6: Main + Stub Workqueue Pump (`main.c`)

**Files:**
- Modify: `src/main.c`

- [ ] **Step 1: Implement `src/main.c`**

```c
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
    .state       = 0,   /* IDLE */
    .battery_pct = 85,
    .rssi        = 0,   /* updated dynamically in real firmware */
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
        return err;
    }

    err = ble_pcs_init();
    if (err) {
        LOG_ERR("ble_pcs_init failed: %d", err);
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
```

- [ ] **Step 2: Build**

```bash
west build -b nrf5340dk/nrf5340/cpuapp --sysbuild
```

Expected: full build succeeds. Binary at `build/zephyr/zephyr.hex`.

- [ ] **Step 3: Commit**

```bash
git add src/main.c src/sensors.c src/led.c src/power.c
git commit -m "feat: main init sequence and stub workqueue data pump"
```

---

## Task 7: Flash & Manual Verification

**Prerequisites:** nRF5340 DK or EVM-BT40 connected via USB, J-Link driver installed, `nRF Connect for Mobile` installed on phone.

- [ ] **Step 1: Flash the firmware**

```bash
west flash
```

Expected: `Flashing nrf5340dk_nrf5340_cpuapp` — no errors.

- [ ] **Step 2: Open UART monitor**

```bash
west espresso
# or
minicom -D /dev/ttyACM0 -b 115200
# or use nRF Terminal in VS Code
```

Expected boot log:
```
[00:00:00.000] PetCollar-X1 booting...
[00:00:00.XXX] BLE enabled
[00:00:00.XXX] Pet Collar Service registered
[00:00:00.XXX] Advertising started as "PetCollar-X1"
[00:00:00.XXX] PetCollar-X1 ready
```

- [ ] **Step 3: Connect with nRF Connect for Mobile**

1. Open nRF Connect for Mobile → Scanner tab
2. Find `PetCollar-X1` → tap Connect
3. Expected UART log: `Connected`, `PHY updated: TX=2 RX=2`, `MTU exchanged: 247`

- [ ] **Step 4: Verify all 6 characteristics are visible**

In nRF Connect → Connected device → expand Pet Collar Service (UUID `A1B2C3D4-...`). Expected: 6 characteristics listed with correct UUIDs.

- [ ] **Step 5: Subscribe to Health notifications**

Tap the subscribe button on Health (0x0102). Wait 60 seconds (or power cycle to trigger the 5-second initial delay).

Expected UART log:
```
[00:00:05.XXX] Health notify: on
```

Expected nRF Connect: Health characteristic shows value `D002620185 0700` (hex for HR=720, SpO2=98, Temp=385, quality=85, flags=0x0007).

- [ ] **Step 6: Verify Location notification**

Subscribe to Location (0x0101). Wait for notification.

Expected: 18-byte value with `lat_e7=0x0EE9A550` (250330000 LE) visible in hex view.

- [ ] **Step 7: Send a Command**

Write `01 00 00 00` (CMD_FIND_MODE_ON) to Command characteristic (0x0104), Write Without Response.

Expected UART log:
```
CMD: 0x01 param1=0x00 param2=0x0000
CMD_FIND_MODE_ON (stub)
```

- [ ] **Step 8: Read and write Configuration**

Read Configuration (0x0106). Expected: 20-byte value matching `g_config` defaults.  
Write `B004 ...` (change gnss_interval to 0x04B0 = 1200s). Read back to confirm.

Expected UART log: `Config updated`

- [ ] **Step 9: 30-minute soak test**

Leave connected for 30 minutes. Monitor UART for watchdog resets or hard faults.

Expected: continuous `status notify` every 5 seconds, no resets.

- [ ] **Step 10: Final commit**

```bash
git add .
git commit -m "feat: complete BLE GATT Profile PoC — all characteristics verified"
```

---

## Self-Review

**Spec coverage check:**

| Spec requirement | Covered by |
|-----------------|-----------|
| BLE 5.3 peripheral advertising as `PetCollar-X1` | Task 4 (ble.c) |
| All 6 characteristics with correct UUIDs | Task 5 (ble_pet_collar_service.c) |
| Location notify (18B) | Task 5 + Task 7 verify Step 6 |
| Health notify (8B) | Task 5 + Task 7 verify Step 5 |
| Behavior notify (6B) | Task 5 + Task 6 stub pump |
| Command write handler (log only, no execute) | Task 5 |
| CMD_REBOOT stub (log, no reboot) | Task 5 |
| CMD_DFU_MODE stub (log, no DFU) | Task 5 |
| Device Status notify (6B) | Task 5 + Task 6 stub pump |
| Configuration read + write (20B) | Task 5 |
| Just Works security (no LESC) | Task 4 (no security callbacks) |
| MTU 247 negotiation | Task 4 |
| 2M PHY preferred | Task 4 |
| Stub data: Taipei lat/lon | Task 6 main.c |
| Stub data: HR=72, SpO2=98, Temp=38.5°C | Task 6 main.c |
| All notify return -ENOTCONN silently when disabled | Task 5 |
| Struct size validation tests | Task 3 |

**Gaps:** None identified.

**Placeholder scan:** No TBD/TODO in plan.

**Type consistency:** All struct names (`ble_health_t`, `ble_location_t`, etc.) and function signatures (`ble_pcs_notify_health`, etc.) are consistent across Tasks 2, 5, and 6.
