#ifndef BLE_CONNECTION_MANAGER
#define BLE_CONNECTION_MANAGER

#include <bluetooth/conn.h>

#include <functional>

void ble_init(std::function<void()> callback = nullptr);

bt_conn* ble_get_connection();

#endif