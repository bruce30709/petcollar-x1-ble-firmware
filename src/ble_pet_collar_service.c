#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "ble_pet_collar_service.h"

LOG_MODULE_REGISTER(pcs, LOG_LEVEL_INF);

/* CCCD tracking — one flag per notifiable characteristic */
static volatile bool notify_location_enabled;
static volatile bool notify_health_enabled;
static volatile bool notify_behavior_enabled;
static volatile bool notify_status_enabled;

/* Mutex protecting g_config for cross-thread access */
K_MUTEX_DEFINE(g_config_mutex);

/* RAM-backed configuration (persists for connection session) */
static struct ble_config_t g_config = {
	.gnss_interval_s   = 1800,
	.health_interval_s = 3600,
	.alert_hr_max      = 200,
	.alert_temp_max    = 410,
	.geofence_radius_m = 500,
};

/* ---- Connection callback — reset CCCD shadow flags on new connection ---- */

static void pcs_conn_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		return;
	}
	/* Reset shadow CCCD flags — client must re-subscribe each connection */
	notify_location_enabled  = false;
	notify_health_enabled    = false;
	notify_behavior_enabled  = false;
	notify_status_enabled    = false;
}

BT_CONN_CB_DEFINE(pcs_conn_callbacks) = {
	.connected = pcs_conn_connected,
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
	if (offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	if (len != sizeof(struct ble_command_t)) {
		LOG_WRN("CMD: unexpected length %u", len);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	const struct ble_command_t *cmd = buf;
	LOG_INF("CMD: 0x%02x param1=0x%02x param2=0x%04x",
		cmd->cmd, cmd->param1, cmd->param2);

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
	k_mutex_lock(&g_config_mutex, K_FOREVER);
	ssize_t ret = bt_gatt_attr_read(conn, attr, buf, len, offset,
					&g_config, sizeof(g_config));
	k_mutex_unlock(&g_config_mutex);
	return ret;
}

static ssize_t config_write_cb(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len,
				uint16_t offset, uint8_t flags)
{
	if (offset + len > sizeof(g_config)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	k_mutex_lock(&g_config_mutex, K_FOREVER);
	memcpy((uint8_t *)&g_config + offset, buf, len);
	k_mutex_unlock(&g_config_mutex);
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

/* ---- Cached attribute pointers (populated at init) ---- */

static const struct bt_gatt_attr *location_attr;
static const struct bt_gatt_attr *health_attr;
static const struct bt_gatt_attr *behavior_attr;
static const struct bt_gatt_attr *status_attr;

/* ---- Notify helpers ---- */

int ble_pcs_notify_health(const struct ble_health_t *data)
{
	if (!notify_health_enabled) {
		return -ENOTCONN;
	}
	if (!health_attr) {
		return -ENOENT;
	}
	return bt_gatt_notify(NULL, health_attr, data, sizeof(*data));
}

int ble_pcs_notify_location(const struct ble_location_t *data)
{
	if (!notify_location_enabled) {
		return -ENOTCONN;
	}
	if (!location_attr) {
		return -ENOENT;
	}
	return bt_gatt_notify(NULL, location_attr, data, sizeof(*data));
}

int ble_pcs_notify_behavior(const struct ble_behavior_t *data)
{
	if (!notify_behavior_enabled) {
		return -ENOTCONN;
	}
	if (!behavior_attr) {
		return -ENOENT;
	}
	return bt_gatt_notify(NULL, behavior_attr, data, sizeof(*data));
}

int ble_pcs_notify_status(const struct ble_status_t *data)
{
	if (!notify_status_enabled) {
		return -ENOTCONN;
	}
	if (!status_attr) {
		return -ENOENT;
	}
	return bt_gatt_notify(NULL, status_attr, data, sizeof(*data));
}

int ble_pcs_init(void)
{
	/* Cache value attribute pointers by UUID for use in notify helpers.
	 * bt_gatt_find_by_uuid() searches for value attrs (not chrc decls),
	 * so it returns the correct handle for bt_gatt_notify().
	 */
	location_attr = bt_gatt_find_by_uuid(pet_collar_svc.attrs,
					     pet_collar_svc.attr_count,
					     PCS_UUID_LOCATION);
	health_attr   = bt_gatt_find_by_uuid(pet_collar_svc.attrs,
					     pet_collar_svc.attr_count,
					     PCS_UUID_HEALTH);
	behavior_attr = bt_gatt_find_by_uuid(pet_collar_svc.attrs,
					     pet_collar_svc.attr_count,
					     PCS_UUID_BEHAVIOR);
	status_attr   = bt_gatt_find_by_uuid(pet_collar_svc.attrs,
					     pet_collar_svc.attr_count,
					     PCS_UUID_STATUS);

	if (!location_attr || !health_attr || !behavior_attr || !status_attr) {
		LOG_ERR("Failed to find one or more GATT characteristic attrs");
		return -ENOENT;
	}

	LOG_INF("Pet Collar Service registered");
	return 0;
}
