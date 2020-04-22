#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <sys/printk.h>
#include <array>
#include <string>
#include <vector>
#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <usb_hid_keys.h>
#include "hid.h"


namespace {
	const std::array<uint8_t,2> rowPins {2, 3};
	const std::array<uint8_t,2> columnPins {16, 15};
	const std::array<std::array<char,2>,2> characters {{{KEY_A, KEY_B}, {KEY_C, KEY_D}}};
	const uint8_t pollingDelayMs = 200;
	const struct bt_data advertising_data[] = {
		BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
		BT_DATA_BYTES(BT_DATA_UUID16_ALL,
				0x12, 0x18, /* HID Service */
				0x0f, 0x18), /* Battery Service */
	};
	const bt_le_adv_param advertising_parameters {
		.id = BT_ID_DEFAULT,
		.options = BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME,
		.interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
		.interval_max = BT_GAP_ADV_FAST_INT_MAX_2
	};
	bool key_pressed_previously = false;

	bt_conn *bt_connection = nullptr;
}

static void connected(struct bt_conn *conn, u8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s (%u)\n", addr, err);
		return;
	}

	printk("Connected %s\n", addr);
	bt_connection = conn;

	if (bt_conn_set_security(conn, BT_SECURITY_L2)) {
		printk("Failed to set security\n");
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected from %s (reason 0x%02x)\n", addr, reason);

	bt_connection = nullptr;
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

void bt_ready(int err) {
	if(err) {
		printk("Bluetooth initialization failed!");
		return;
	}

	printk("Advertising attempt\n");
	hid_init();

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}
	
	int adv_err = bt_le_adv_start(&advertising_parameters, advertising_data, ARRAY_SIZE(advertising_data), NULL, 0);

	if(adv_err) {
		printk("Bluetooth adv failed to start");
		return;
	} else {
		printk("Advertising succeeded!\n");
	}
}

void main(void)
{
	struct device *gpio0 = device_get_binding("GPIO_0");

	for(auto pin : rowPins) {
		gpio_pin_configure(gpio0, pin, GPIO_PULL_UP | GPIO_INPUT);
	}

	for(auto pin : columnPins) {
		gpio_pin_configure(gpio0, pin, GPIO_DISCONNECTED);
	}

	if(!bt_enable(bt_ready)) {
		bt_conn_cb_register(&conn_callbacks);
	}

	while(1) {
		std::vector<uint8_t> pressed_keys;

		for(uint8_t columnIndex = 0; columnIndex < columnPins.size(); columnIndex++) {
			gpio_pin_configure(gpio0, columnPins[columnIndex], GPIO_OUTPUT_LOW);

			for(uint8_t rowIndex = 0; rowIndex < rowPins.size(); rowIndex++) {

				if(gpio_pin_get(gpio0, rowPins[rowIndex]) == 0) {
					pressed_keys.push_back(characters[rowIndex][columnIndex]);
				}
			}

			gpio_pin_configure(gpio0, columnPins[columnIndex], GPIO_DISCONNECTED);
		}

		if(pressed_keys.size() > 0) {
			key_pressed_previously = true;
			notify_keycodes(bt_connection, pressed_keys, std::vector<uint8_t>());

			std::string outString (pressed_keys.begin(), pressed_keys.end());
			printk((outString + "\n").c_str());
		} else if(key_pressed_previously) {
			notify_keyrelease(bt_connection);
		}
		
		k_sleep(pollingDelayMs);
	}
}