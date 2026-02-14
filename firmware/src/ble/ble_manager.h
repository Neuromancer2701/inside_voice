#ifndef BLE_BLE_MANAGER_H
#define BLE_BLE_MANAGER_H

/**
 * Initialize the BLE stack, register connection callbacks,
 * and begin advertising as "InsideVoice" peripheral.
 *
 * @return 0 on success, negative errno on failure.
 */
int ble_manager_init(void);

#endif /* BLE_BLE_MANAGER_H */
