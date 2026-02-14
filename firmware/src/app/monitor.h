#ifndef APP_MONITOR_H
#define APP_MONITOR_H

/**
 * Start the monitor thread.
 *
 * The thread continuously reads PDM audio blocks, computes RMS/dB,
 * applies threshold comparison with hysteresis (3 consecutive blocks
 * over threshold to trigger, 3 under to release), and drives LED +
 * vibration feedback accordingly. Also sends BLE notifications of
 * current sound level.
 *
 * @return 0 on success, negative errno on failure.
 */
int monitor_start(void);

#endif /* APP_MONITOR_H */
