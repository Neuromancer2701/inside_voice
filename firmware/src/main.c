#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/config.h"
#include "app/monitor.h"
#include "audio/pdm_capture.h"
#include "ble/ble_manager.h"
#include "ble/config_service.h"
#include "feedback/led.h"
#include "feedback/vibration.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	int err;

	LOG_INF("InsideVoice firmware starting");

	/* Initialize persistent config (NVS) */
	err = app_config_init();
	if (err) {
		LOG_ERR("Config init failed: %d", err);
		return err;
	}

	/* Initialize audio subsystem */
	err = pdm_capture_init();
	if (err) {
		LOG_ERR("PDM capture init failed: %d", err);
		return err;
	}

	/* Initialize feedback */
	err = led_init();
	if (err) {
		LOG_ERR("LED init failed: %d", err);
		return err;
	}

	err = vibration_init();
	if (err) {
		LOG_ERR("Vibration init failed: %d", err);
		return err;
	}

	/* Initialize BLE */
	err = ble_manager_init();
	if (err) {
		LOG_ERR("BLE init failed: %d", err);
		return err;
	}

	err = config_service_init();
	if (err) {
		LOG_ERR("Config service init failed: %d", err);
		return err;
	}

	/* Start idle LED pattern */
	led_set_pattern(LED_PATTERN_BREATHE_GREEN);

	/* Start monitor thread (reads audio, triggers feedback) */
	err = monitor_start();
	if (err) {
		LOG_ERR("Monitor start failed: %d", err);
		return err;
	}

	LOG_INF("InsideVoice firmware initialized");

	/* main() exits — all work continues in threads */
	return 0;
}
