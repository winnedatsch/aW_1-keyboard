#ifndef BLE_CONNECTION_MANAGER
#define BLE_CONNECTION_MANAGER

#include <bluetooth/conn.h>

#include <functional>

void ble_init(std::function<void()> callback = nullptr);

struct settings_handler *get_paired_conf();
bt_conn *ble_get_connection();
void reset_paired_device();

#endif