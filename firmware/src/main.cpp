#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/adc.h>
#include <drivers/i2c.h>
#include <sys/printk.h>
#include <array>
#include <string>
#include <vector>
#include <settings/settings.h>
#include <battery.h>
#include <memory>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/services/bas.h>
#include <usb_hid_keys.h>
#include "hid.h"

namespace {
	// keyboard configuration
	const std::array<uint8_t,2> row_pins_right {2, 3};
	const std::array<uint8_t,2> column_pins_right {16, 15};
	const std::array<std::array<char,2>,2> characters_right {{{KEY_A, KEY_B}, {KEY_C, KEY_D}}};
	const std::array<uint8_t,2> row_pins_left {0, 1};
	const std::array<uint8_t,2> column_pins_left {7, 6};
	const std::array<std::array<char,2>,2> characters_left {{{KEY_E, KEY_F}, {KEY_G, KEY_H}}};
	const uint16_t expander_i2c = 0x20;
	const uint8_t polling_delay_ms = 2;
	bool key_pressed_previously = false;

	// BLE configuration
	const int passkey = 1234;
	const bt_le_conn_param *connection_paramters = BT_LE_CONN_PARAM(1, 1, 20, 400);
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
	bt_conn *bt_connection = nullptr;

	// battery reading configuration
	uint16_t ms_since_last_battery_report = 0; 
    const uint16_t battery_reporting_interval_ms = 30000;
	std::shared_ptr<Battery> battery = nullptr;
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
	bt_connection = bt_conn_ref(conn);

	if (bt_conn_set_security(conn, BT_SECURITY_L2)) {
		printk("Failed to set security\n");
	}

	if(bt_conn_le_param_update(conn, connection_paramters)) {
		printk("Failed to update connection paramters\n");
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected from %s (reason 0x%02x)\n", addr, reason);

	bt_conn_unref(bt_connection);
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

void pairing_confirm(bt_conn *conn) {
	printk("Asking for pairing confirmation");
	if(bt_conn_auth_pairing_confirm(conn)) {
		printk("Pairing confirmation failed");
	}
}

static struct bt_conn_auth_cb auth_callbacks = {
	.passkey_display = NULL,
	.passkey_entry = NULL,
	.passkey_confirm = NULL,
	.pairing_confirm = pairing_confirm
};

std::vector<uint8_t> scan_right(device *gpio0) {
	std::vector<uint8_t> pressed_keys;

	for(uint8_t column_index = 0; column_index < column_pins_right.size(); column_index++) {
		gpio_pin_configure(gpio0, column_pins_right[column_index], GPIO_OUTPUT_LOW);

		for(uint8_t row_index = 0; row_index < row_pins_right.size(); row_index++) {

			if(gpio_pin_get(gpio0, row_pins_right[row_index]) == 0) {
				pressed_keys.push_back(characters_right[row_index][column_index]);
			}
		}

		gpio_pin_configure(gpio0, column_pins_right[column_index], GPIO_DISCONNECTED);
	}

	return pressed_keys;
}

std::vector<uint8_t> scan_left(device *i2c0) {
	std::vector<uint8_t> pressed_keys;

	i2c_reg_write_byte(i2c0, expander_i2c, 0x01, 0xFF); // set port B (rows) to inputs
	i2c_reg_write_byte(i2c0, expander_i2c, 0x0D, 0xFF); // enable port B pull-ups
	i2c_reg_write_byte(i2c0, expander_i2c, 0x00, 0xFF); // set port A (columns) to inputs
	i2c_reg_write_byte(i2c0, expander_i2c, 0x0C, 0x00); // disable port A pull-ups

	for(uint8_t column_index = 0; column_index < column_pins_left.size(); column_index++) {
		uint8_t column_pin = column_pins_left[column_index];
		i2c_reg_write_byte(i2c0, expander_i2c, 0x00, ~(1 << column_pin)); // set current column to output
		i2c_reg_write_byte(i2c0, expander_i2c, 0x12, ~(1 << column_pin)); // drive current column pin low

		uint8_t value = 255;
		i2c_reg_read_byte(i2c0, expander_i2c, 0x13, &value); // read port B (rows)

		for(uint8_t row_index = 0; row_index < row_pins_left.size(); row_index++) {
			uint8_t row_pin = row_pins_left[row_index];

			if(((value & (1 << row_pin)) >> row_pin) == 0) {
				pressed_keys.push_back(characters_left[row_index][column_index]);
			}
		}

		i2c_reg_write_byte(i2c0, expander_i2c, 0x00, 0xFF); // set columns to input again
	}

	return pressed_keys;
}

void main(void)
{
	device *gpio0 = device_get_binding("GPIO_0");
	device *adc0 = device_get_binding("ADC_0");
	device *i2c0 = device_get_binding("I2C_0");

	k_sleep(3000);

	battery = std::make_shared<Battery>(3000, 4200, adc0, 7);
	battery->begin(3600, 1.377, &sigmoidal);

	for(auto pin : row_pins_right) {
		gpio_pin_configure(gpio0, pin, GPIO_PULL_UP | GPIO_INPUT);
	}

	for(auto pin : column_pins_right) {
		gpio_pin_configure(gpio0, pin, GPIO_DISCONNECTED);
	}

	if(!bt_enable(bt_ready)) {
		bt_conn_cb_register(&conn_callbacks);
		bt_conn_auth_cb_register(&auth_callbacks);
		bt_passkey_set(passkey);
	}

	while(1) {

		if(bt_connection) {
			std::vector<uint8_t> pressed_keys = scan_right(gpio0);
			std::vector<uint8_t> pressed_keys_left = scan_left(i2c0);
			pressed_keys.insert(std::end(pressed_keys), std::begin(pressed_keys_left), std::end(pressed_keys_left));

			if(pressed_keys.size() > 0) {
				key_pressed_previously = true;
				notify_keycodes(bt_connection, pressed_keys, std::vector<uint8_t>());
			} else if(key_pressed_previously) {
				notify_keyrelease(bt_connection);
				key_pressed_previously = false;
			}
		}

		if(ms_since_last_battery_report > battery_reporting_interval_ms) {
			uint8_t battery_level_stepped_5 = static_cast<uint8_t>(round(battery->level() / 5.0) * 5.0);
			printk("Battery level (rounded): %d%%\n", battery_level_stepped_5);
			bt_gatt_bas_set_battery_level(battery_level_stepped_5);
			ms_since_last_battery_report = 0;
		}
		
		k_sleep(polling_delay_ms);
		ms_since_last_battery_report += polling_delay_ms;
	}
}