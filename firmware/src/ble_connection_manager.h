#include <bluetooth/conn.h>


void ble_init(std::function<void()> callback = nullptr);

bt_conn * ble_get_connection();
