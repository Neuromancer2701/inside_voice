#include "config.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_INF);

static struct app_config current_cfg = {
	.threshold_db = CONFIG_DEFAULT_THRESHOLD_DB,
	.feedback_mode = CONFIG_DEFAULT_FEEDBACK_MODE,
};

static K_MUTEX_DEFINE(cfg_mutex);

/* --- Zephyr settings callbacks --- */

static int config_set(const char *name, size_t len,
		      settings_read_cb read_cb, void *cb_arg)
{
	if (!strcmp(name, "threshold")) {
		if (len != sizeof(current_cfg.threshold_db)) {
			return -EINVAL;
		}
		return read_cb(cb_arg, &current_cfg.threshold_db, len);
	}

	if (!strcmp(name, "fb_mode")) {
		if (len != sizeof(current_cfg.feedback_mode)) {
			return -EINVAL;
		}
		return read_cb(cb_arg, &current_cfg.feedback_mode, len);
	}

	return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(inside_voice, "iv", NULL, config_set, NULL,
			       NULL);

int app_config_init(void)
{
	int err = settings_subsys_init();

	if (err) {
		LOG_ERR("settings_subsys_init failed: %d", err);
		return err;
	}

	err = settings_load();
	if (err) {
		LOG_ERR("settings_load failed: %d", err);
		return err;
	}

	LOG_INF("Config loaded: threshold=%u dB, feedback_mode=0x%02x",
		current_cfg.threshold_db, current_cfg.feedback_mode);
	return 0;
}

struct app_config app_config_get(void)
{
	struct app_config copy;

	k_mutex_lock(&cfg_mutex, K_FOREVER);
	copy = current_cfg;
	k_mutex_unlock(&cfg_mutex);

	return copy;
}

int app_config_set_threshold(uint8_t db)
{
	k_mutex_lock(&cfg_mutex, K_FOREVER);
	current_cfg.threshold_db = db;
	k_mutex_unlock(&cfg_mutex);

	int err = settings_save_one("iv/threshold", &db, sizeof(db));

	if (err) {
		LOG_ERR("Failed to save threshold: %d", err);
	}
	return err;
}

int app_config_set_feedback_mode(uint8_t mode)
{
	k_mutex_lock(&cfg_mutex, K_FOREVER);
	current_cfg.feedback_mode = mode;
	k_mutex_unlock(&cfg_mutex);

	int err = settings_save_one("iv/fb_mode", &mode, sizeof(mode));

	if (err) {
		LOG_ERR("Failed to save feedback mode: %d", err);
	}
	return err;
}
