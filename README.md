# PetCollar-X1 BLE Firmware

智慧寵物項圈的 BLE GATT 韌體 PoC，基於 **Fanstel EVM-BT40 (Nordic nRF5340)** + **Zephyr RTOS (nRF Connect SDK)**。

裝置以 `PetCollar-X1` 廣播，App 透過 BLE GATT 直連，提供定位、生理、行為、狀態等資料，並接收 App 下達的指令與設定。本階段所有感測器資料皆為 **stub（固定假值）**，用於驗證完整 BLE 連線與 GATT 互動流程。

---

## 功能範圍

本子專案聚焦在 **BLE GATT Profile**，已完成：

- 以 `PetCollar-X1` 連線式廣播，可被 nRF Connect for Mobile 探索並連線
- 完整 6 個 Pet Collar Service 特徵（characteristic），含 Notify / Write / Read
- 連線後自動協商 MTU 247、嘗試 2M PHY
- Command 寫入即時記錄於 UART log
- Configuration 可讀寫，連線期間保存於 RAM
- 內建 Device Information Service (DIS) 與 Battery Service (BAS)

**尚未實作**（後續子專案）：真實感測器驅動（MAX30102 / LIS3DH / TMP117）、GNSS、NVS 持久化、BLE DFU、電源管理、Edge ML 行為分類、地理圍欄。

---

## 硬體與工具鏈

| 項目 | 版本 / 型號 |
|------|------------|
| 開發板 | Fanstel EVM-BT40 (nRF5340) — build target `nrf5340dk_nrf5340_cpuapp` |
| SoC | nRF5340：App Core (Cortex-M33 @ 128MHz) + Network Core (BLE controller) |
| RTOS | Zephyr 3.5.99（透過 nRF Connect SDK） |
| BLE | 5.3 GATT Peripheral，Just Works 配對 |
| SDK | NCS，`ZEPHYR_BASE=$HOME/ncs/zephyr` |
| Zephyr SDK | 0.16.8（ARM toolchain） |

---

## 專案結構

```
evm-bt40-firmware/
├── CMakeLists.txt                         # app 來源與 include 路徑
├── prj.conf                               # Kconfig（BLE / log / buffer 設定）
├── boards/
│   └── nrf5340dk_nrf5340_cpuapp.overlay   # UART0 console
├── include/
│   ├── ble.h                              # 連線管理 API
│   └── ble_pet_collar_service.h           # UUID、封包結構、GATT API
├── src/
│   ├── main.c                             # 開機流程 + stub 資料 workqueue
│   ├── ble.c                              # 廣播、連線生命週期、MTU/PHY
│   ├── ble_pet_collar_service.c           # GATT service 定義與 handler
│   ├── sensors.c / led.c / power.c        # stub（尚未接硬體）
├── tests/
│   └── ble_structs/                       # ztest：驗證封包結構大小
└── docs/superpowers/                      # 設計文件與實作計畫
```

兩層架構：`ble.c` 負責連線管理，`ble_pet_collar_service.c` 負責 GATT service，兩者各自以 `BT_CONN_CB_DEFINE` 註冊連線回呼。

---

## GATT Service — Pet Collar Service

Base UUID：`A1B2C3D4-xxxx-1000-8000-00805F9B34FB`，`xxxx` 依特徵變化。

| 特徵 | UUID `xxxx` | 屬性 | 長度 | Stub 內容 |
|------|------------|------|------|-----------|
| Location | `0101` | Notify | 18 B | 台北 25.033°N 121.5654°E，3D fix |
| Health | `0102` | Notify | 8 B | HR 72 BPM、SpO₂ 98%、體溫 38.5°C |
| Behavior | `0103` | Notify | 6 B | WALKING，信心 80，步數 1234 |
| Command | `0104` | Write Without Response | 4 B | 寫入後記錄於 log |
| Device Status | `0105` | Notify | 6 B | 電量 85%、state、uptime |
| Configuration | `0106` | Read / Write | 20 B | RAM 保存 |

封包皆為 `__packed`，欄位定義與單位見 `include/ble_pet_collar_service.h`。

### Command 指令（目前為記錄用 stub）

| 指令 | 值 | 行為 |
|------|----|----|
| `CMD_FIND_MODE_ON` | `0x01` | log |
| `CMD_FIND_MODE_OFF` | `0x02` | log |
| `CMD_SYNC_TIME` | `0x03` | log（時間同步尚未實作） |
| `CMD_SET_CONFIG` | `0x04` | log |
| `CMD_REBOOT` | `0x05` | log only，不重啟 |
| `CMD_DFU_MODE` | `0x06` | log only，不進入 DFU |

### Stub 資料推送間隔

| 資料 | 間隔 |
|------|------|
| Device Status | 5 s |
| Location | 30 s |
| Health | 60 s |
| Behavior | 120 s |

所有 work item 開機後 5 秒首次觸發。

---

## 建置

需先設定 NCS / Zephyr 環境變數：

```bash
export ZEPHYR_BASE=$HOME/ncs/zephyr
export ZEPHYR_SDK_INSTALL_DIR=$HOME/zephyr-sdk-0.16.8
```

建置韌體（`--sysbuild` 會自動處理 Network Core 的 `hci_ipc`）：

```bash
west build -b nrf5340dk_nrf5340_cpuapp --sysbuild -d build
```

燒錄（需連接 nRF5340 DK 或 EVM-BT40 + J-Link）：

```bash
west flash
```

**目前用量**：Flash ≈ 94 KB（約 9%／1 MB），RAM ≈ 35 KB。

---

## 測試

`tests/ble_structs` 以 ztest 在 `native_sim` 上驗證所有 BLE 封包結構的位元組大小（18/8/6/4/6/20 bytes），確保 wire format 不因編譯器 padding 走樣：

```bash
west build -b native_sim tests/ble_structs -d build_test
./build_test/zephyr/zephyr.exe
```

預期 6 個測項全數 PASS。

---

## 手動驗證（需硬體）

1. `west flash` 後開啟 UART monitor（115200 8N1）
2. 用 **nRF Connect for Mobile** 掃描 `PetCollar-X1` 並連線
3. 確認 6 個特徵皆出現，訂閱 Notify 特徵後可週期收到資料
4. 對 Command 特徵寫入（例如 `0x01`），UART log 應印出該指令
5. 讀寫 Configuration，數值在連線期間保持

---

## 授權

PoC / 內部開發用途。
