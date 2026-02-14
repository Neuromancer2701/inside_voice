#ifndef FEEDBACK_LED_H
#define FEEDBACK_LED_H

/**
 * LED feedback patterns for the onboard RGB LED.
 */
enum led_pattern {
	LED_PATTERN_OFF,
	LED_PATTERN_PULSE_WARM,    /* Warm red/orange pulse — over threshold */
	LED_PATTERN_BREATHE_GREEN, /* Slow green breathe — idle / under threshold */
	LED_PATTERN_FLASH_BLUE,    /* Quick blue flash — BLE event */
};

/**
 * Initialize LED GPIOs.
 *
 * @return 0 on success, negative errno on failure.
 */
int led_init(void);

/**
 * Set the active LED pattern. Cancels any running pattern and starts the
 * new one asynchronously via a system work queue item.
 *
 * @param pattern  The pattern to display.
 */
void led_set_pattern(enum led_pattern pattern);

#endif /* FEEDBACK_LED_H */
