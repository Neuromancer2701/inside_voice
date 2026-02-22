#include "config_service.h"
#include "../app/config.h"
#include "../app/data_cache.h"

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
#define IV_SAMPLE_COUNT_UUID_VAL \
	BT_UUID_128_ENCODE(0x4f490004, 0x2ff1, 0x4a5e, 0xa683, 0x4de2c5a10100)
#define IV_SYNC_CTRL_UUID_VAL \
	BT_UUID_128_ENCODE(0x4f490005, 0x2ff1, 0x4a5e, 0xa683, 0x4de2c5a10100)
#define IV_SYNC_DATA_UUID_VAL \
	BT_UUID_128_ENCODE(0x4f490006, 0x2ff1, 0x4a5e, 0xa683, 0x4de2c5a10100)

static struct bt_uuid_128 iv_svc_uuid = BT_UUID_INIT_128(IV_SVC_UUID_VAL);
static struct bt_uuid_128 iv_threshold_uuid = BT_UUID_INIT_128(IV_THRESHOLD_UUID_VAL);
static struct bt_uuid_128 iv_level_uuid = BT_UUID_INIT_128(IV_LEVEL_UUID_VAL);
static struct bt_uuid_128 iv_fb_mode_uuid = BT_UUID_INIT_128(IV_FB_MODE_UUID_VAL);
static struct bt_uuid_128 iv_sample_count_uuid = BT_UUID_INIT_128(IV_SAMPLE_COUNT_UUID_VAL);
static struct bt_uuid_128 iv_sync_ctrl_uuid    = BT_UUID_INIT_128(IV_SYNC_CTRL_UUID_VAL);
static struct bt_uuid_128 iv_sync_data_uuid    = BT_UUID_INIT_128(IV_SYNC_DATA_UUID_VAL);

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

/* Forward-declare service so sync_work_handler can reference attrs */
extern const struct bt_gatt_service_static iv_svc;

/* --- Sample Count characteristic (Read) --- */

static ssize_t sample_count_read(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr,
				 void *buf, uint16_t len, uint16_t offset)
{
	uint32_t count = data_cache_count();
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &count, sizeof(count));
}

/* --- Sync Data characteristic (Notify) --- */

static void sync_data_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	LOG_INF("Sync data notifications %s",
		value == BT_GATT_CCC_NOTIFY ? "enabled" : "disabled");
}

/* Sync work: streams cache samples then sends sentinel */
static struct k_work_delayable sync_work;
static uint32_t sync_start_idx;

static void sync_work_handler(struct k_work *work)
{
	uint32_t count = data_cache_count();
	/* Find the sync_data notify attribute — index 13 in iv_svc
	 * Layout: [0]svc [1]thresh_decl [2]thresh_val [3]level_decl [4]level_val
	 *         [5]level_ccc [6]fbmode_decl [7]fbmode_val
	 *         [8]scount_decl [9]scount_val [10]sctrl_decl [11]sctrl_val
	 *         [12]sdata_decl [13]sdata_val [14]sdata_ccc
	 */
	const struct bt_gatt_attr *notify_attr = &iv_svc.attrs[13];

	for (uint32_t i = sync_start_idx; i < count; i++) {
		struct iv_sample s;
		if (!data_cache_get(i, &s)) {
			continue;
		}
		uint8_t record[5];
		record[0] = (uint8_t)(s.uptime_ms & 0xFF);
		record[1] = (uint8_t)((s.uptime_ms >> 8) & 0xFF);
		record[2] = (uint8_t)((s.uptime_ms >> 16) & 0xFF);
		record[3] = (uint8_t)((s.uptime_ms >> 24) & 0xFF);
		record[4] = s.db;
		int ret = bt_gatt_notify(NULL, notify_attr, record, sizeof(record));
		if (ret == -ENOMEM) {
			/* Congestion — resume from this index after 20 ms */
			sync_start_idx = i;
			k_work_schedule(&sync_work, K_MSEC(20));
			return;
		}
	}
	/* Send sentinel */
	uint8_t sentinel[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	bt_gatt_notify(NULL, notify_attr, sentinel, sizeof(sentinel));
	sync_start_idx = 0;
}

/* --- Sync Control characteristic (Write) --- */

static ssize_t sync_ctrl_write(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr,
			       const void *buf, uint16_t len,
			       uint16_t offset, uint8_t flags)
{
	if (len != 1 || offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	uint8_t cmd = *((const uint8_t *)buf);
	if (cmd == 0x01) {
		sync_start_idx = 0;
		k_work_schedule(&sync_work, K_NO_WAIT);
		LOG_INF("Sync started");
	} else if (cmd == 0x02) {
		data_cache_clear();
		LOG_INF("Cache cleared");
	}
	return len;
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

	/* Sample Count (Read) */
	BT_GATT_CHARACTERISTIC(&iv_sample_count_uuid.uuid,
			       BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ,
			       sample_count_read, NULL, NULL),

	/* Sync Control (Write) */
	BT_GATT_CHARACTERISTIC(&iv_sync_ctrl_uuid.uuid,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE,
			       NULL, sync_ctrl_write, NULL),

	/* Sync Data (Notify) */
	BT_GATT_CHARACTERISTIC(&iv_sync_data_uuid.uuid,
			       BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_NONE,
			       NULL, NULL, NULL),
	BT_GATT_CCC(sync_data_ccc_changed,
		     BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

int config_service_init(void)
{
	k_work_init_delayable(&sync_work, sync_work_handler);
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

void config_service_start_sync(void)
{
	sync_start_idx = 0;
	k_work_schedule(&sync_work, K_NO_WAIT);
}

void config_service_clear_cache(void)
{
	data_cache_clear();
}
