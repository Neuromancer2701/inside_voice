#include "led.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led, LOG_LEVEL_INF);

/*
 * XIAO nRF52840 Sense onboard RGB LED — accent LEDs active-low.
 * led0 = red, led1 = green, led2 = blue
 */
static const struct gpio_dt_spec led_red =
	GPIO_DT_SPEC_GET(DT_ALIAS(led_red), gpios);
static const struct gpio_dt_spec led_green =
	GPIO_DT_SPEC_GET(DT_ALIAS(led_green), gpios);
static const struct gpio_dt_spec led_blue =
	GPIO_DT_SPEC_GET(DT_ALIAS(led_blue), gpios);

static struct k_work_delayable pattern_work;
static volatile enum led_pattern active_pattern;
static volatile int pattern_step;

/* --- helpers --- */

static void leds_all_off(void)
{
	gpio_pin_set_dt(&led_red, 0);
	gpio_pin_set_dt(&led_green, 0);
	gpio_pin_set_dt(&led_blue, 0);
}

static void pattern_handler(struct k_work *work)
{
	enum led_pattern pat = active_pattern;
	int step = pattern_step;

	switch (pat) {
	case LED_PATTERN_OFF:
		leds_all_off();
		return;

	case LED_PATTERN_PULSE_WARM:
		/* Simple on/off pulsing of red LED at ~2 Hz */
		gpio_pin_set_dt(&led_red, step % 2);
		pattern_step = step + 1;
		k_work_reschedule(&pattern_work, K_MSEC(250));
		break;

	case LED_PATTERN_BREATHE_GREEN:
		/* Toggle green at ~1 Hz (on 700ms, off 300ms) */
		if (step % 2 == 0) {
			gpio_pin_set_dt(&led_green, 1);
			pattern_step = step + 1;
			k_work_reschedule(&pattern_work, K_MSEC(700));
		} else {
			gpio_pin_set_dt(&led_green, 0);
			pattern_step = step + 1;
			k_work_reschedule(&pattern_work, K_MSEC(300));
		}
		break;

	case LED_PATTERN_FLASH_BLUE:
		/* Three quick flashes then stop */
		if (step < 6) {
			gpio_pin_set_dt(&led_blue, step % 2);
			pattern_step = step + 1;
			k_work_reschedule(&pattern_work, K_MSEC(100));
		} else {
			gpio_pin_set_dt(&led_blue, 0);
			/* Return to idle breathe after BLE flash */
			active_pattern = LED_PATTERN_BREATHE_GREEN;
			pattern_step = 0;
			k_work_reschedule(&pattern_work, K_MSEC(200));
		}
		break;
	}
}

/* --- public API --- */

int led_init(void)
{
	int err;

	if (!gpio_is_ready_dt(&led_red) ||
	    !gpio_is_ready_dt(&led_green) ||
	    !gpio_is_ready_dt(&led_blue)) {
		LOG_ERR("LED GPIO devices not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
	if (err) return err;
	err = gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
	if (err) return err;
	err = gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
	if (err) return err;

	k_work_init_delayable(&pattern_work, pattern_handler);

	LOG_INF("LED subsystem initialized");
	return 0;
}

void led_set_pattern(enum led_pattern pattern)
{
	/* Cancel any pending work before switching */
	k_work_cancel_delayable(&pattern_work);
	leds_all_off();

	active_pattern = pattern;
	pattern_step = 0;

	/* Submit immediately */
	k_work_reschedule(&pattern_work, K_NO_WAIT);
}
