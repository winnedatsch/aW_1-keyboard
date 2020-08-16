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
	const uint8_t polling_delay_ms = 5;
	const auto polling_delay_disconnected_ms = 1000;
	std::vector<std::pair<uint8_t, uint8_t>> previous_keys;

	// battery reading configuration
	uint16_t ms_since_last_battery_report = 0; 
    const uint16_t battery_reporting_interval_ms = 30000;
}

void main(void)
{
	std::shared_ptr<device> gpio0 (device_get_binding("GPIO_0"));
	std::shared_ptr<device> adc0 (device_get_binding("ADC_0"));
	std::shared_ptr<device> i2c0 (device_get_binding("I2C_0"));

	auto battery_reader = std::make_unique<BatteryReader>(adc0, 3700, 3000, 4200, battery_reading_pin_analogue, 1.485, sigmoidal);
	auto matrix_scanner = std::make_unique<KeyboardMatrixScanner>(gpio0, i2c0, expander_i2c, pins);
	auto keycode_resolver = std::make_unique<KeycodeResolver>(keycode_matrix, fn_matrix);

	ble_init([]() {
		hid_init();
	});

	while(1) {
		bt_conn *ble_connection = ble_get_connection();
 
		if(ms_since_last_battery_report > battery_reporting_interval_ms) {
			uint8_t battery_level_stepped_5 = static_cast<uint8_t>(round(battery_reader->level() / 5.0) * 5.0);
			printk("Battery level (rounded): %d%%\n", battery_level_stepped_5);
			bt_gatt_bas_set_battery_level(battery_level_stepped_5);
			ms_since_last_battery_report = 0;
		}

		if(ble_connection) {
			s64_t time_stamp = k_uptime_get();
			std::vector<std::pair<uint8_t, uint8_t>> pressed_keys = matrix_scanner->scan_matrix();
			keycodes keycodes = keycode_resolver->resolve_keycodes(pressed_keys);

			if(pressed_keys != previous_keys) {
				if(pressed_keys.size() > 0) {
					printk("notifying %d keypresses\n", pressed_keys.size());
					notify_keycodes(ble_connection, keycodes.keycodes, keycodes.modifiers);
				} else {
					printk("notifying keyrelease\n");
					notify_keyrelease(ble_connection);
				}
				previous_keys = std::move(pressed_keys);
			}

			const auto delta = static_cast<int>(k_uptime_delta(&time_stamp));
			k_sleep(polling_delay_ms);
			ms_since_last_battery_report = ms_since_last_battery_report + polling_delay_ms + delta;
		} else {
			k_sleep(polling_delay_disconnected_ms);
			ms_since_last_battery_report += polling_delay_disconnected_ms;
		}
	}
}