#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/adc.h>
#include <drivers/i2c.h>
#include <string>
#include <vector>
#include <memory>

#include <bluetooth/conn.h>
#include <bluetooth/services/bas.h>
#include "hid.h"
#include "battery_reader.h"
#include "ble_connection_manager.h"
#include "keyboard_matrix_scanner.h"
#include "keycode_resolver.h"
#include "board_mappings.h"

namespace {
	const uint8_t polling_delay_ms = 2;
	bool key_pressed_previously = false;

	// battery reading configuration
	uint16_t ms_since_last_battery_report = 0; 
    const uint16_t battery_reporting_interval_ms = 30000;

	std::unique_ptr<BatteryReader> battery_reader = nullptr;
	std::unique_ptr<KeyboardMatrixScanner> matrix_scanner = nullptr;
	std::unique_ptr<KeycodeResolver> keycode_resolver = nullptr;
}



void main(void)
{
	device *gpio0 = device_get_binding("GPIO_0");
	device *adc0 = device_get_binding("ADC_0");
	device *i2c0 = device_get_binding("I2C_0");

	battery_reader = std::make_unique<BatteryReader>(adc0, 3600, 3000, 4200, 7, 1.377, sigmoidal);
	matrix_scanner = std::make_unique<KeyboardMatrixScanner>(gpio0, i2c0, expander_i2c, pins);
	keycode_resolver = std::make_unique<KeycodeResolver>(keycode_matrix);

	ble_init([]() {
		hid_init();
	});

	while(1) {
		bt_conn *ble_connection = ble_get_connection();

		std::vector<std::pair<uint8_t, uint8_t>> pressed_keys = matrix_scanner->scan_matrix();
		keycodes keycodes = keycode_resolver->resolve_keycodes(pressed_keys);

		if(ble_connection) {
			if(pressed_keys.size() > 0) {
				key_pressed_previously = true;
				notify_keycodes(ble_connection, keycodes.keycodes, keycodes.modifiers);
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