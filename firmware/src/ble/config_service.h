#ifndef BLE_CONFIG_SERVICE_H
#define BLE_CONFIG_SERVICE_H

#include <stdint.h>

/**
 * Custom BLE GATT service UUID:
 *   Base: 4f490000-2ff1-4a5e-a683-4de2c5a10100
 *
 * Characteristics:
 *   - Threshold  (R/W):       4f490001-...  uint8 dB value
 *   - Sound Level (R/Notify): 4f490002-...  uint8 current dB
 *   - Feedback Mode (R/W):    4f490003-...  uint8 bitmask
 *   - Sample Count (R):       4f490004-...  uint32 cached sample count
 *   - Sync Control (W):       4f490005-...  uint8 command (0x01=sync, 0x02=clear)
 *   - Sync Data (Notify):     4f490006-...  5-byte records [uptime_ms_le32, db]
 */

/**
 * Register the InsideVoice config GATT service.
 * Must be called after bt_enable().
 *
 * @return 0 on success, negative errno on failure.
 */
int config_service_init(void);

/**
 * Update the sound level characteristic and send a BLE notification
 * if a client is subscribed.
 *
 * @param db  Current sound level in dB.
 */
void config_service_notify_level(uint8_t db);

/* Sync: start streaming cached samples to connected BLE client */
void config_service_start_sync(void);

/* Sync: clear the sample cache (called after client acks) */
void config_service_clear_cache(void);

#endif /* BLE_CONFIG_SERVICE_H */
