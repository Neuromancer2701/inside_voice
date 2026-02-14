#ifndef FEEDBACK_VIBRATION_H
#define FEEDBACK_VIBRATION_H

/**
 * Vibration feedback patterns for the coin motor on D0 via PWM.
 */
enum vib_pattern {
	VIB_PATTERN_OFF,
	VIB_PATTERN_GENTLE_TAP,   /* Single short gentle buzz */
	VIB_PATTERN_DOUBLE_TAP,   /* Two quick taps */
	VIB_PATTERN_SOFT_PULSE,   /* Longer soft ramp-up/down */
};

/**
 * Initialize PWM for the vibration motor.
 *
 * @return 0 on success, negative errno on failure.
 */
int vibration_init(void);

/**
 * Play a vibration pattern. Cancels any in-progress pattern.
 *
 * @param pattern  The pattern to play.
 */
void vibration_play(enum vib_pattern pattern);

/**
 * Stop vibration immediately.
 */
void vibration_stop(void);

#endif /* FEEDBACK_VIBRATION_H */
