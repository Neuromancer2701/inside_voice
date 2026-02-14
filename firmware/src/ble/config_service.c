#include "config_service.h"
#include "../app/config.h"

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(config_service, LOG_LEVEL_INF);

/* --- Service UUID: 4f490000-2ff1-4a5e-a683-4de2c5a10100 --- */
#define IV_SVC_UUID_VAL \
	BT_UUID_128_ENCODE(0x4f490000, 0x2ff1, 0x4a5e, 0xa683, 0x4de2c5a10100)

/* Characteristic UUIDs */
#define IV_THRESHOLD_UUID_VAL \
	BT_UUID_128_ENCODE(0x4f490001, 0x2ff1, 0x4a5e, 0xa683, 0x4de2c5a10100)
#define IV_LEVEL_UUID_VAL \
	BT_UUID_128_ENCODE(0x4f490002, 0x2ff1, 0x4a5e, 0xa683, 0x4de2c5a10100)
#define IV_FB_MODE_UUID_VAL \
	BT_UUID_128_ENCODE(0x4f490003, 0x2ff1, 0x4a5e, 0xa683, 0x4de2c5a10100)

static struct bt_uuid_128 iv_svc_uuid = BT_UUID_INIT_128(IV_SVC_UUID_VAL);
static struct bt_uuid_128 iv_threshold_uuid = BT_UUID_INIT_128(IV_THRESHOLD_UUID_VAL);
static struct bt_uuid_128 iv_level_uuid = BT_UUID_INIT_128(IV_LEVEL_UUID_VAL);
static struct bt_uuid_128 iv_fb_mode_uuid = BT_UUID_INIT_128(IV_FB_MODE_UUID_VAL);

/* Current sound level (updated from monitor thread) */
static uint8_t current_level_db;

/* --- Threshold characteristic --- */

static ssize_t threshold_read(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr,
			      void *buf, uint16_t len, uint16_t offset)
{
	struct app_config cfg = app_config_get();

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &cfg.threshold_db, sizeof(cfg.threshold_db));
}

static ssize_t threshold_write(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr,
			       const void *buf, uint16_t len,
			       uint16_t offset, uint8_t flags)
{
	if (len != sizeof(uint8_t) || offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t val = *((const uint8_t *)buf);

	app_config_set_threshold(val);
	LOG_INF("Threshold set via BLE: %u dB", val);

	return len;
}

/* --- Feedback mode characteristic --- */

static ssize_t fb_mode_read(struct bt_conn *conn,
			    const struct bt_gatt_attr *attr,
			    void *buf, uint16_t len, uint16_t offset)
{
	struct app_config cfg = app_config_get();

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &cfg.feedback_mode, sizeof(cfg.feedback_mode));
}

static ssize_t fb_mode_write(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     const void *buf, uint16_t len,
			     uint16_t offset, uint8_t flags)
{
	if (len != sizeof(uint8_t) || offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t val = *((const uint8_t *)buf);

	app_config_set_feedback_mode(val);
	LOG_INF("Feedback mode set via BLE: 0x%02x", val);

	return len;
}

/* --- Sound level characteristic (read + notify) --- */

static ssize_t level_read(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr,
			  void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &current_level_db, sizeof(current_level_db));
}

static void level_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	LOG_INF("Sound level notifications %s",
		value == BT_GATT_CCC_NOTIFY ? "enabled" : "disabled");
}

/* --- Service definition --- */

BT_GATT_SERVICE_DEFINE(iv_svc,
	BT_GATT_PRIMARY_SERVICE(&iv_svc_uuid),

	/* Threshold (R/W) */
	BT_GATT_CHARACTERISTIC(&iv_threshold_uuid.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       threshold_read, threshold_write, NULL),

	/* Sound Level (R/Notify) */
	BT_GATT_CHARACTERISTIC(&iv_level_uuid.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ,
			       level_read, NULL, NULL),
	BT_GATT_CCC(level_ccc_changed,
		     BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	/* Feedback Mode (R/W) */
	BT_GATT_CHARACTERISTIC(&iv_fb_mode_uuid.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       fb_mode_read, fb_mode_write, NULL),
);

int config_service_init(void)
{
	LOG_INF("InsideVoice GATT service registered");
	return 0;
}

void config_service_notify_level(uint8_t db)
{
	current_level_db = db;

	/* Notify attribute is at index 4 (after svc + threshold char/val) */
	bt_gatt_notify(NULL, &iv_svc.attrs[4], &current_level_db,
		       sizeof(current_level_db));
}
