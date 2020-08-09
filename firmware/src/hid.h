#ifndef HID
#define HID

#include <vector>
#include <bluetooth/conn.h>

void hid_init(void);

void notify_keycodes(bt_conn *conn, std::vector<uint8_t> keycodes, std::vector<uint8_t> modifiers);

void notify_keyrelease(bt_conn *conn);

uint8_t convert_modifiers_to_bitmask(std::vector<uint8_t> modifiers);

#endif