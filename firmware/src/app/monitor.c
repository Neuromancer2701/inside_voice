#include "monitor.h"
#include "config.h"
#include "data_cache.h"
#include "../audio/pdm_capture.h"
#include "../audio/sound_level.h"
#include "../feedback/led.h"
#include "../feedback/vibration.h"
#include "../ble/config_service.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(monitor, LOG_LEVEL_INF);

#define MONITOR_STACK_SIZE 2048
#define MONITOR_PRIORITY   5

/* Hysteresis: require N consecutive blocks over/under threshold */
#define HYSTERESIS_COUNT 3

static void monitor_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int over_count = 0;
	int under_count = 0;
	bool feedback_active = false;
	static int block_count = 0;
	static uint32_t db_accum = 0;

	int err = pdm_capture_start();

	if (err) {
		LOG_ERR("Failed to start PDM capture: %d", err);
		return;
	}

	LOG_INF("Monitor thread running");

	while (1) {
		void *buf;
		size_t size;

		err = pdm_capture_read(&buf, &size);
		if (err) {
			k_sleep(K_MSEC(100));
			continue;
		}

		size_t sample_count = size / sizeof(int16_t);
		uint16_t rms = sound_level_rms((const int16_t *)buf, sample_count);
		uint8_t db = sound_level_rms_to_db(rms);

		pdm_capture_buf_free(buf);

		/* Notify BLE clients of current level */
		config_service_notify_level(db);

		db_accum += db;
		block_count++;
		if (block_count >= 10) {
			uint8_t avg_db = (uint8_t)(db_accum / block_count);
			data_cache_push(avg_db);
			block_count = 0;
			db_accum = 0;
		}

		/* Get current config */
		struct app_config cfg = app_config_get();

		/* Threshold comparison with hysteresis */
		if (db >= cfg.threshold_db) {
			over_count++;
			under_count = 0;

			if (!feedback_active && over_count >= HYSTERESIS_COUNT) {
				feedback_active = true;
				LOG_INF("Over threshold (%u dB >= %u dB)",
					db, cfg.threshold_db);

				if (cfg.feedback_mode & FEEDBACK_MODE_LED) {
					led_set_pattern(LED_PATTERN_PULSE_WARM);
				}
				if (cfg.feedback_mode & FEEDBACK_MODE_VIBRATION) {
					vibration_play(VIB_PATTERN_GENTLE_TAP);
				}
			}
		} else {
			under_count++;
			over_count = 0;

			if (feedback_active && under_count >= HYSTERESIS_COUNT) {
				feedback_active = false;
				LOG_INF("Under threshold (%u dB < %u dB)",
					db, cfg.threshold_db);

				led_set_pattern(LED_PATTERN_BREATHE_GREEN);
				vibration_stop();
			}
		}
	}
}

K_THREAD_STACK_DEFINE(monitor_stack, MONITOR_STACK_SIZE);
static struct k_thread monitor_thread_data;

int monitor_start(void)
{
	data_cache_init();

	k_thread_create(&monitor_thread_data, monitor_stack,
			K_THREAD_STACK_SIZEOF(monitor_stack),
			monitor_thread_fn, NULL, NULL, NULL,
			MONITOR_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&monitor_thread_data, "monitor");

	return 0;
}
