#include <zephyr/types.h>
#include <stddef.h>
#include <string>
#include <array>
#include <errno.h>
#include <sys/printk.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <usb_hid_keys.h>

#include <hid.h>

enum {
	HIDS_REMOTE_WAKE = BIT(0),
	HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

struct hids_info {
	uint16_t version; /* version number of base USB HID Specification */
	uint8_t code; /* country HID Device hardware is localized for. */
	uint8_t flags;
} __packed;

struct hids_report {
	uint8_t id; /* report id */
	uint8_t type; /* report type */
} __packed;

static struct hids_info info = {
	.version = 0x0000,
	.code = 0x00,
	.flags = HIDS_NORMALLY_CONNECTABLE,
};

enum {
	HIDS_INPUT = 0x01,
	HIDS_OUTPUT = 0x02,
	HIDS_FEATURE = 0x03,
};

static struct hids_report input = {
	.id = 0x01,
	.type = HIDS_INPUT,
};

static uint8_t simulate_input;
static uint8_t ctrl_point;
static uint8_t report_map[] = {
	0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x01,       // Report ID (1)
    0x05, 0x07,       // Usage Page (Key Codes)
    0x19, 0xe0,       // Usage Minimum (224)
    0x29, 0xe7,       // Usage Maximum (231)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x01,       // Logical Maximum (1)
    0x75, 0x01,       // Report Size (1)
    0x95, 0x08,       // Report Count (8)
    0x81, 0x02,       // Input (Data, Variable, Absolute)

    0x95, 0x01,       // Report Count (1)
    0x75, 0x08,       // Report Size (8)
    0x81, 0x03,       // Input (Constant) reserved byte(1)

    0x95, 0x05,       // Report Count (5)
    0x75, 0x01,       // Report Size (1)
    0x05, 0x08,       // Usage Page (Page# for LEDs)
    0x19, 0x01,       // Usage Minimum (1)
    0x29, 0x05,       // Usage Maximum (5)
    0x91, 0x02,       // Output (Data, Variable, Absolute), Led report
    0x95, 0x01,       // Report Count (1)
    0x75, 0x03,       // Report Size (3)
    0x91, 0x03,       // Output (Data, Variable, Absolute), Led report padding

    0x95, 0x06,       // Report Count (6)
    0x75, 0x08,       // Report Size (8)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x65,       // Logical Maximum (101)
    0x05, 0x07,       // Usage Page (Key codes)
    0x19, 0x00,       // Usage Minimum (0)
    0x29, 0x65,       // Usage Maximum (101)
    0x81, 0x00,       // Input (Data, Array) Key array(6 bytes)

    0xC0              // End Collection (Application)
};


static ssize_t read_info(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_info));
}

static ssize_t read_report_map(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map,
				 sizeof(report_map));
}

static ssize_t read_report(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	simulate_input = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static ssize_t write_ctrl_point(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	uint8_t *value = static_cast<uint8_t*>(attr->user_data);

	if (offset + len > sizeof(ctrl_point)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

/* HID Service Declaration */
BT_GATT_SERVICE_DEFINE(hid_keyboard_service,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, 
        BT_GATT_CHRC_READ, 
        BT_GATT_PERM_READ, 
        read_info, NULL, &info),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, 
        BT_GATT_CHRC_READ, 
        BT_GATT_PERM_READ, 
        read_report_map, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ_AUTHEN,
		NULL, NULL, NULL),
	BT_GATT_CCC(input_ccc_changed,
		BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, 
        BT_GATT_PERM_READ,
		read_report, NULL, &input),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
		BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE,
		NULL, write_ctrl_point, &ctrl_point)
);

void notify_keycodes(bt_conn *conn, std::vector<uint8_t> keycodes, std::vector<uint8_t> modifiers) {
    uint8_t modifiers_bitmask = convert_modifiers_to_bitmask(modifiers);
    
    std::array<uint8_t, 8> data {modifiers_bitmask, 0x00};

    auto last = std::min(keycodes.size(), 5u);
    std::copy(keycodes.begin(), keycodes.begin() + last, data.begin() + 2);

    int err = bt_gatt_notify(conn, &hid_keyboard_service.attrs[6], &data[0], 8);
    if(err) {
        printk("notify failed!");
    }
}

void notify_keyrelease(bt_conn *conn) {
    uint8_t keyrelease_data[8] = {0x00};
    bt_gatt_notify(conn, &hid_keyboard_service.attrs[6], &keyrelease_data, 8);
}

uint8_t convert_modifiers_to_bitmask(std::vector<uint8_t> modifiers) {
    uint8_t bitmask = 0x00;

    for(auto modifier : modifiers) {
        if(modifier == KEY_LEFTCTRL) {
            bitmask |= KEY_MOD_LCTRL;
        } else if (modifier == KEY_LEFTSHIFT) {
            bitmask |= KEY_MOD_LSHIFT;
        } else if (modifier == KEY_LEFTALT) {
            bitmask |= KEY_MOD_LALT;
        } else if (modifier == KEY_LEFTMETA) {
            bitmask |= KEY_MOD_LMETA;
        } else if (modifier == KEY_RIGHTCTRL) {
            bitmask |= KEY_MOD_RCTRL;
        } else if (modifier == KEY_RIGHTSHIFT) {
            bitmask |= KEY_MOD_RSHIFT;
        } else if (modifier == KEY_RIGHTALT) {
            bitmask |= KEY_MOD_RALT;
        } else if (modifier == KEY_RIGHTMETA) {
            bitmask |= KEY_MOD_RMETA;
        } 
    }

    return bitmask;
}

void hid_init(void)
{
}

