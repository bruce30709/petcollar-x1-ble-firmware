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
                          struct bt_conn_le_phy_info *param)
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
    int ret = bt_conn_le_phy_update(conn, &phy_param);
    if (ret && ret != -EALREADY) {
        LOG_WRN("PHY update request failed: %d", ret);
    }

    /* Request MTU 247 */
    ret = bt_gatt_exchange_mtu(conn, &mtu_params);
    if (ret) {
        LOG_ERR("MTU exchange request failed: %d", ret);
    }
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
    if (err && err != -EALREADY) {
        LOG_ERR("Advertising start failed: %d", err);
    }
    LOG_INF("Advertising started as \"%s\"", CONFIG_BT_DEVICE_NAME);
}

bool ble_is_connected(void)
{
    return current_conn != NULL;
}
