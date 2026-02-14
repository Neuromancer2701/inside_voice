#include "vibration.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(vibration, LOG_LEVEL_INF);

static const struct pwm_dt_spec vib_pwm =
	PWM_DT_SPEC_GET(DT_NODELABEL(vib0));

static struct k_work_delayable vib_work;
static volatile enum vib_pattern active_vib;
static volatile int vib_step;

/* PWM period: 20 ms (50 Hz — good for coin motors) */
#define VIB_PERIOD_NS  PWM_MSEC(20)

static void vib_set_intensity(uint8_t pct)
{
	uint32_t pulse = (VIB_PERIOD_NS * pct) / 100;

	pwm_set_dt(&vib_pwm, VIB_PERIOD_NS, pulse);
}

static void vib_handler(struct k_work *work)
{
	enum vib_pattern pat = active_vib;
	int step = vib_step;

	switch (pat) {
	case VIB_PATTERN_OFF:
		vib_set_intensity(0);
		return;

	case VIB_PATTERN_GENTLE_TAP:
		/* 60% for 80ms then off */
		if (step == 0) {
			vib_set_intensity(60);
			vib_step = 1;
			k_work_reschedule(&vib_work, K_MSEC(80));
		} else {
			vib_set_intensity(0);
		}
		break;

	case VIB_PATTERN_DOUBLE_TAP:
		/* 60% 60ms, off 80ms, 60% 60ms, off */
		switch (step) {
		case 0:
			vib_set_intensity(60);
			vib_step = 1;
			k_work_reschedule(&vib_work, K_MSEC(60));
			break;
		case 1:
			vib_set_intensity(0);
			vib_step = 2;
			k_work_reschedule(&vib_work, K_MSEC(80));
			break;
		case 2:
			vib_set_intensity(60);
			vib_step = 3;
			k_work_reschedule(&vib_work, K_MSEC(60));
			break;
		default:
			vib_set_intensity(0);
			break;
		}
		break;

	case VIB_PATTERN_SOFT_PULSE:
		/* Ramp up 0→50% over 5 steps, then back down */
		if (step <= 4) {
			vib_set_intensity((step + 1) * 10);
			vib_step = step + 1;
			k_work_reschedule(&vib_work, K_MSEC(60));
		} else if (step <= 8) {
			vib_set_intensity((9 - step) * 10);
			vib_step = step + 1;
			k_work_reschedule(&vib_work, K_MSEC(60));
		} else {
			vib_set_intensity(0);
		}
		break;
	}
}

int vibration_init(void)
{
	if (!pwm_is_ready_dt(&vib_pwm)) {
		LOG_ERR("Vibration PWM device not ready");
		return -ENODEV;
	}

	/* Ensure motor is off */
	vib_set_intensity(0);

	k_work_init_delayable(&vib_work, vib_handler);

	LOG_INF("Vibration motor initialized");
	return 0;
}

void vibration_play(enum vib_pattern pattern)
{
	k_work_cancel_delayable(&vib_work);
	vib_set_intensity(0);

	active_vib = pattern;
	vib_step = 0;

	k_work_reschedule(&vib_work, K_NO_WAIT);
}

void vibration_stop(void)
{
	k_work_cancel_delayable(&vib_work);
	active_vib = VIB_PATTERN_OFF;
	vib_set_intensity(0);
}
