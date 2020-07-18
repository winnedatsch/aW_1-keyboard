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
#include <battery_reader.h>
#include <memory>

#include <bluetooth/conn.h>
#include <usb_hid_keys.h>
#include "hid.h"
#include "ble_connection_manager.h"
#include "keyboard_matrix_scanner.h"

namespace {
	// keyboard configuration
	const keyboard_pins pins {
		{0, 1}, // rows_left
		{7, 6}, // columns_left
		{2, 3}, // rows_right
		{16, 15} // columns_right
	};
	const std::array<std::array<char,2>,2> characters_right {{{KEY_A, KEY_B}, {KEY_C, KEY_D}}};
	const std::array<std::array<char,2>,2> characters_left {{{KEY_E, KEY_F}, {KEY_G, KEY_H}}};
	const uint16_t expander_i2c = 0x20;
	const uint8_t polling_delay_ms = 2;
	bool key_pressed_previously = false;

	// battery reading configuration
	uint16_t ms_since_last_battery_report = 0; 
    const uint16_t battery_reporting_interval_ms = 30000;
	std::unique_ptr<BatteryReader> battery_reader = nullptr;
	std::unique_ptr<KeyboardMatrixScanner> matrix_scanner = nullptr;
}



void main(void)
{
	device *gpio0 = device_get_binding("GPIO_0");
	device *adc0 = device_get_binding("ADC_0");
	device *i2c0 = device_get_binding("I2C_0");

	battery_reader = std::make_unique<BatteryReader>(adc0, 3600, 3000, 4200, 7, 1.377, &sigmoidal);
	matrix_scanner = std::make_unique<KeyboardMatrixScanner>(gpio0, i2c0, expander_i2c, pins);
	

	ble_init([]() {
		hid_init();
	});

	while(1) {

		bt_conn *ble_connection = ble_get_connection();
		if(ble_connection) {
			std::vector<uint8_t> pressed_keys = scan_right(gpio0);
			std::vector<uint8_t> pressed_keys_left = scan_left(i2c0);
			pressed_keys.insert(std::end(pressed_keys), std::begin(pressed_keys_left), std::end(pressed_keys_left));

			if(pressed_keys.size() > 0) {
				key_pressed_previously = true;
				notify_keycodes(ble_connection, pressed_keys, std::vector<uint8_t>());
			} else if(key_pressed_previously) {
				notify_keyrelease(ble_connection);
				key_pressed_previously = false;
			}
		}

		if(ms_since_last_battery_report > battery_reporting_interval_ms) {
			uint8_t battery_level_stepped_5 = static_cast<uint8_t>(round(battery_reader->level() / 5.0) * 5.0);
			printk("Battery level (rounded): %d%%\n", battery_level_stepped_5);
			bt_gatt_bas_set_battery_level(battery_level_stepped_5);
			ms_since_last_battery_report = 0;
		}
		
		k_sleep(polling_delay_ms);
		ms_since_last_battery_report += polling_delay_ms;
	}
}