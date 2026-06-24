# BLE GATT Profile Design — PetCollar-X1 PoC

**Date:** 2026-06-24
**Project:** PetCollar-X1 (EVM-BT40 / nRF5340)
**Scope:** BLE GATT Profile sub-project (sensors stubbed with fake values)
**Source spec:** PetCollar_BLE_PoC.md v0.2

---

## 1. Goals

Implement a verifiable BLE GATT Profile on nRF5340 (Zephyr RTOS) that:
- Advertises as `PetCollar-X1` and accepts connections from nRF Connect for Mobile
- Exposes all 6 Pet Collar Service characteristics with stub data
- Receives and logs Command writes from the App
- Persists Configuration across the connection session (RAM only, no NVS yet)

Sensor integration, GNSS, power management, and Edge ML are out of scope for this sub-project.

---

## 2. Architecture

Two-layer separation:

| Layer | File | Responsibility |
|-------|------|----------------|
| Connection Manager | `src/ble.c` | Advertising, connection lifecycle, MTU/PHY negotiation |
| GATT Service | `src/ble_pet_collar_service.c` | `BT_GATT_SERVICE_DEFINE`, notify, write/read handlers |
| App Orchestration | `src/main.c` | Init sequence, stub data workqueue pump |
| Stubs | `src/sensors.c`, `src/led.c`, `src/power.c` | Return fixed values, no hardware access |

Data flow:
```
main.c workqueue → ble_pcs_notify_*() → BLE notification → App
App Write → pcs_cmd_write_cb() / pcs_cfg_write_cb() → LOG / memcpy
```

---

## 3. File Structure

```
evm-bt40-firmware/
├── CMakeLists.txt
├── prj.conf
├── boards/
│   └── evm_bt40.overlay
├── include/
│   ├── ble.h
│   └── ble_pet_collar_service.h
└── src/
    ├── main.c
    ├── ble.c
    ├── ble_pet_collar_service.c
    ├── sensors.c
    ├── led.c
    └── power.c
```

---

## 4. BLE Connection Manager (`ble.c`)

### Advertising
- Type: connectable undirected (`BT_LE_ADV_CONN`)
- Name in AD: `PetCollar-X1`
- Interval: 100 ms (fast) → 1000 ms after 30 s without connection
- No whitelist for PoC

### Connection Parameters
- Requested interval: 80–100 ms (normal mode)
- Slave latency: 4
- Supervision timeout: 4000 ms

### PHY
- Request 2M PHY on connect via `bt_conn_le_phy_update()`
- Fall back to 1M silently if central refuses

### MTU
- Request MTU 247 via `bt_gatt_exchange_mtu()` on connect

### Security
- Just Works (no `BT_SECURITY_L2` requirement)
- No pairing callbacks registered

### API
```c
int  ble_init(void);        // call once at boot
void ble_start_advertising(void);
bool ble_is_connected(void);
```

---

## 5. Pet Collar GATT Service (`ble_pet_collar_service.c`)

### UUIDs

```c
// Base: A1B2C3D4-0000-1000-8000-00805F9B34FB
#define PCS_UUID_BASE \
    BT_UUID_128_ENCODE(0xA1B2C3D4, 0x0000, 0x1000, 0x8000, 0x00805F9B34FB)

#define PCS_UUID_SERVICE   BT_UUID_DECLARE_128(PCS_UUID_BASE)
#define PCS_UUID_LOCATION  BT_UUID_DECLARE_128(/* 0x0101 variant */)
#define PCS_UUID_HEALTH    BT_UUID_DECLARE_128(/* 0x0102 variant */)
#define PCS_UUID_BEHAVIOR  BT_UUID_DECLARE_128(/* 0x0103 variant */)
#define PCS_UUID_COMMAND   BT_UUID_DECLARE_128(/* 0x0104 variant */)
#define PCS_UUID_STATUS    BT_UUID_DECLARE_128(/* 0x0105 variant */)
#define PCS_UUID_CONFIG    BT_UUID_DECLARE_128(/* 0x0106 variant */)
```

### Characteristics

| Characteristic | Properties | Length | Notes |
|----------------|-----------|--------|-------|
| Location | Notify + CCCD | 18 B | Stub: 25.033°N 121.565°E |
| Health | Notify + CCCD | 8 B | Stub: HR=72, SpO2=98, Temp=38.5°C |
| Behavior | Notify + CCCD | 6 B | Stub: WALKING, confidence=80 |
| Command | Write Without Response | 4 B | Logged, not executed (stub) |
| Device Status | Notify + CCCD | 6 B | Stub: battery=85%, state=IDLE |
| Configuration | Read + Write | 20 B | Stored in RAM g_config |

### Data Structures (in `ble_pet_collar_service.h`)

```c
struct ble_location_t {
    int32_t  lat_e7;       // e.g. 250330000 = 25.033°
    int32_t  lon_e7;       // e.g. 1215650000 = 121.565°
    int16_t  alt_m;
    uint16_t accuracy_cm;
    uint32_t timestamp;
    uint8_t  fix_type;
    uint8_t  mode;
} __packed;                // 18 bytes

struct ble_health_t {
    uint16_t heart_rate;   // BPM × 10
    uint8_t  spo2;         // %
    int16_t  temperature;  // 0.1°C (385 = 38.5°C)
    uint8_t  signal_quality;
    uint16_t flags;        // bit0=HR valid, bit1=SpO2 valid, bit2=Temp valid
} __packed;                // 8 bytes

struct ble_behavior_t {
    uint8_t  behavior;     // enum behavior_t
    uint8_t  confidence;   // 0-100
    uint32_t steps;
} __packed;                // 6 bytes

struct ble_status_t {
    uint8_t  state;        // system state enum
    uint8_t  battery_pct;
    uint8_t  rssi;
    uint8_t  uptime[3];    // seconds, 3-byte little-endian
} __packed;                // 6 bytes

struct ble_config_t {
    uint16_t gnss_interval_s;
    uint16_t health_interval_s;
    uint16_t alert_hr_max;
    uint16_t alert_temp_max;
    uint32_t geofence_radius_m;
    uint8_t  flags[8];
} __packed;                // 20 bytes
```

### Notify behaviour
- All `ble_pcs_notify_*()` functions return `-ENOTCONN` silently if not connected or CCCD disabled.
- No internal state; caller owns the data buffer.

### Command handler (stub)
- Receives 4-byte write, logs `CMD=0x%02x param1=0x%02x param2=0x%04x`.
- `CMD_REBOOT (0x05)`: **stub only — does not reboot in this sub-project.**
- `CMD_DFU_MODE (0x06)`: **stub only — does not enter DFU.**

---

## 6. Stub Data Pump (`main.c`)

```
Boot sequence:
  1. ble_init()
  2. ble_pcs_init()
  3. ble_start_advertising()
  4. Schedule workqueue items

Workqueue items:
  health_work   → every 60 s  → ble_pcs_notify_health(&stub_health)
  location_work → every 30 s  → ble_pcs_notify_location(&stub_location)
  status_work   → every 5 s   → ble_pcs_notify_status(&stub_status)
  behavior_work → every 120 s → ble_pcs_notify_behavior(&stub_behavior)
```

All stub values are compile-time constants defined in `main.c`.

---

## 7. Kconfig (`prj.conf`)

```
CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_DEVICE_NAME="PetCollar-X1"
CONFIG_BT_DEVICE_APPEARANCE=833     # Generic: Watch (closest for wearable PoC)
CONFIG_BT_DIS=y                     # Device Information Service
CONFIG_BT_DIS_PNP=n
CONFIG_BT_BAS=y                     # Battery Service
CONFIG_BT_GATT_CLIENT=n
CONFIG_BT_MAX_CONN=1
CONFIG_BT_L2CAP_TX_MTU=247
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3          # INFO
```

---

## 8. Board Overlay (`boards/evm_bt40.overlay`)

Minimal overlay — no sensor pins needed for stub phase. Reserves UART0 for logging.

```dts
/ {
    chosen {
        zephyr,console = &uart0;
        zephyr,shell-uart = &uart0;
    };
};
```

Full sensor pin mapping (I²C, INT pins) will be added in the Sensor Driver sub-project.

---

## 9. Out of Scope (this sub-project)

- Real sensor reads (MAX30102, LIS3DH, TMP117)
- GNSS integration
- NVS persistent storage
- BLE DFU / MCUboot
- Power management (PSM, tickless idle)
- Edge ML inference
- Geofencing logic
- BLE Long Range (Coded PHY advertising)

---

## 10. Success Criteria

- `nRF Connect for Mobile` can discover `PetCollar-X1`, connect, and subscribe to all 5 notify characteristics
- Health/Location/Status notifications arrive at expected intervals
- Command write (e.g. `CMD_FIND_MODE_ON`) is logged on UART
- Configuration can be read and written; value persists for the connection session
- No watchdog resets or hard faults under 30-minute continuous connection
