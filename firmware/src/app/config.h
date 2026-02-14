#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

/* Feedback mode bitmask */
#define FEEDBACK_MODE_LED       BIT(0)
#define FEEDBACK_MODE_VIBRATION BIT(1)
#define FEEDBACK_MODE_ALL       (FEEDBACK_MODE_LED | FEEDBACK_MODE_VIBRATION)

/* Default threshold in dB SPL (approximate) */
#define CONFIG_DEFAULT_THRESHOLD_DB 70
#define CONFIG_DEFAULT_FEEDBACK_MODE FEEDBACK_MODE_ALL

struct app_config {
	uint8_t threshold_db;
	uint8_t feedback_mode;
};

/**
 * Initialize the config subsystem and load saved settings from NVS.
 * Must be called after settings_subsys_init().
 */
int app_config_init(void);

/** Get current config (thread-safe snapshot). */
struct app_config app_config_get(void);

/** Set threshold and persist to NVS. */
int app_config_set_threshold(uint8_t db);

/** Set feedback mode bitmask and persist to NVS. */
int app_config_set_feedback_mode(uint8_t mode);

#endif /* APP_CONFIG_H */
