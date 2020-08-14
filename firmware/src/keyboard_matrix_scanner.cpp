#include "keyboard_matrix_scanner.h"
#include <drivers/gpio.h>
#include <drivers/i2c.h>

KeyboardMatrixScanner::KeyboardMatrixScanner(std::shared_ptr<device> gpio, std::shared_ptr<device> i2c, uint8_t left_i2c_id,  keyboard_pins pins): 
gpio{gpio}, i2c{i2c}, left_i2c_id{left_i2c_id}, pins{pins} {
    for(auto pin : pins.rows_right) {
		gpio_pin_configure(gpio.get(), pin, GPIO_PULL_UP | GPIO_INPUT);
	}

	for(auto pin : pins.columns_right) {
		gpio_pin_configure(gpio.get(), pin, GPIO_DISCONNECTED);
	}
}

std::vector<std::pair<uint8_t, uint8_t>> KeyboardMatrixScanner::scan_matrix() {
    std::vector<std::pair<uint8_t, uint8_t>> pressed_keys = scan_right();
    std::vector<std::pair<uint8_t, uint8_t>> pressed_keys_left = scan_left();
	pressed_keys.insert(std::end(pressed_keys), std::begin(pressed_keys_left), std::end(pressed_keys_left));

    return pressed_keys;
}

std::vector<std::pair<uint8_t, uint8_t>> KeyboardMatrixScanner::scan_right() {
	std::vector<std::pair<uint8_t, uint8_t>> pressed_keys;

	for(uint8_t column = 0; column < pins.columns_right.size(); column++) {
		gpio_pin_configure(gpio.get(), pins.columns_right[column], GPIO_OUTPUT_LOW);

		for(uint8_t row = 0; row < pins.rows_right.size(); row++) {

			if(gpio_pin_get(gpio.get(), pins.rows_right[row]) == 0) {
				pressed_keys.push_back(std::make_pair(row, (pins.columns_right.size() - column - 1) + pins.columns_left.size()));
			}
		}

		gpio_pin_configure(gpio.get(), pins.columns_right[column], GPIO_DISCONNECTED);
	}

	return pressed_keys;
}

std::vector<std::pair<uint8_t, uint8_t>> KeyboardMatrixScanner::scan_left() {
	std::vector<std::pair<uint8_t, uint8_t>> pressed_keys;

	auto err = i2c_reg_write_byte(i2c.get(), left_i2c_id, 0x01, 0xFF); // set port B (rows) to inputs
	if(err == 0) {
		i2c_reg_write_byte(i2c.get(), left_i2c_id, 0x0D, 0xFF); // enable port B pull-ups
		i2c_reg_write_byte(i2c.get(), left_i2c_id, 0x00, 0xFF); // set port A (columns) to inputs
		i2c_reg_write_byte(i2c.get(), left_i2c_id, 0x0C, 0x00); // disable port A pull-ups

		for(uint8_t column = 0; column < pins.columns_left.size(); column++) {
			uint8_t column_pin = pins.columns_left[column];
			i2c_reg_write_byte(i2c.get(), left_i2c_id, 0x00, ~(1 << column_pin)); // set current column to output
			i2c_reg_write_byte(i2c.get(), left_i2c_id, 0x12, ~(1 << column_pin)); // drive current column pin low

			uint8_t value = 255;
			i2c_reg_read_byte(i2c.get(), left_i2c_id, 0x13, &value); // read port B (rows)

			for(uint8_t row = 0; row < pins.rows_left.size(); row++) {
				uint8_t row_pin = pins.rows_left[row];

				if(((value & (1 << row_pin)) >> row_pin) == 0) {
					pressed_keys.push_back(std::make_pair(row, column));
				}
			}

			i2c_reg_write_byte(i2c.get(), left_i2c_id, 0x00, 0xFF); // set columns to input again
		}
	}

	return pressed_keys;
}