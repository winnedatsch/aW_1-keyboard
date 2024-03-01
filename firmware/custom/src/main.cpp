#include <bluetooth/conn.h>
#include <bluetooth/services/bas.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/adc.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <settings/settings.h>
#include <zephyr.h>
#include <logging/log.h>

#include <memory>
#include <string>
#include <vector>

#include "battery_reader.h"
#include "ble_connection_manager.h"
#include "board_mappings.h"
#include "hid.h"
#include "keyboard_matrix_scanner.h"
#include "keycode_resolver.h"

LOG_MODULE_REGISTER(main);

namespace {
const uint8_t polling_delay_ms = 2;
const uint16_t polling_delay_disconnected_ms = 200;
std::vector<std::pair<uint8_t, uint8_t>> previous_keys;

// battery reading configuration
uint16_t ms_since_last_battery_report = 0;
const uint16_t battery_reporting_interval_ms = 30000;
int16_t button_debounce = 0;
}  // namespace

void decrease_button_debounce(uint16_t by) {
    button_debounce = button_debounce - by;
    if (button_debounce < 0) {
        button_debounce = 0;
    }
}

void main(void) {
    std::shared_ptr<device> gpio0(device_get_binding("GPIO_0"));
    std::shared_ptr<device> adc0(device_get_binding("ADC_0"));
    std::shared_ptr<device> i2c0(device_get_binding("I2C_0"));

    gpio_pin_configure(gpio0.get(), button_pin, GPIO_PULL_UP | GPIO_INPUT);

    auto battery_reader = std::make_unique<BatteryReader>(
        adc0, 3700, 3000, 4200, battery_reading_pin_analogue, 1.485, sigmoidal);
    auto matrix_scanner = std::make_unique<KeyboardMatrixScanner>(gpio0, i2c0, expander_i2c, pins);
    auto keycode_resolver = std::make_unique<KeycodeResolver>(keycode_matrix, fn_matrix);

    settings_subsys_init();
    settings_register(get_paired_conf());
    ble_init([]() { hid_init(); });

    while (1) {
        bt_conn *ble_connection = ble_get_connection();

        if (ms_since_last_battery_report > battery_reporting_interval_ms) {
            uint8_t battery_level_stepped_5 =
                static_cast<uint8_t>(round(battery_reader->level() / 5.0) * 5.0);
            LOG_INF("Battery level (rounded): %d%%", battery_level_stepped_5);
            bt_gatt_bas_set_battery_level(battery_level_stepped_5);
            ms_since_last_battery_report = 0;
        }

        auto reset_connections_pressed = gpio_pin_get(gpio0.get(), button_pin) == 0;
        if (reset_connections_pressed) {
            if (button_debounce == 0) {
                LOG_INF("pressed reset pairing button");
                reset_paired_device();
            }
            button_debounce = 500;
        } else {
        }

        if (ble_connection) {
            s64_t time_stamp = k_uptime_get();
            std::vector<std::pair<uint8_t, uint8_t>> pressed_keys = matrix_scanner->scan_matrix();
            keycodes keycodes = keycode_resolver->resolve_keycodes(pressed_keys);

            if (pressed_keys != previous_keys) {
                if (pressed_keys.size() > 0) {
                    notify_keycodes(ble_connection, keycodes.keycodes, keycodes.modifiers);
                } else {
                    notify_keyrelease(ble_connection);
                }
                previous_keys = std::move(pressed_keys);
            }

            const auto delta = static_cast<int>(k_uptime_delta(&time_stamp));
            k_sleep(K_MSEC(polling_delay_ms));
            ms_since_last_battery_report = ms_since_last_battery_report + polling_delay_ms + delta;
            if (!reset_connections_pressed) {
                decrease_button_debounce(polling_delay_ms + delta);
            }
        } else {
            k_sleep(K_MSEC(polling_delay_disconnected_ms));
            ms_since_last_battery_report += polling_delay_disconnected_ms;
            if (!reset_connections_pressed) {
                decrease_button_debounce(polling_delay_disconnected_ms);
            }
        }
    }
}