#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <sys/printk.h>
#include <array>
#include <string>
#include <vector>

namespace {
	const std::array<uint8_t,2> rowPins {16,17};
	const std::array<uint8_t,2> columnPins {15, 14};
	const std::array<std::array<char,2>,2> characters {{{'A', 'B'}, {'C', 'D'}}};
	const uint8_t pollingDelayMs = 200;
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

	while(1) {
		std::vector<char> pressedCharacters;

		for(uint8_t columnIndex = 0; columnIndex < columnPins.size(); columnIndex++) {
			gpio_pin_configure(gpio0, columnPins[columnIndex], GPIO_OUTPUT_LOW);

			for(uint8_t rowIndex = 0; rowIndex < rowPins.size(); rowIndex++) {

				if(gpio_pin_get(gpio0, rowPins[rowIndex]) == 0) {
					pressedCharacters.push_back(characters[rowIndex][columnIndex]);
				}
			}

			gpio_pin_configure(gpio0, columnPins[columnIndex], GPIO_DISCONNECTED);
		}

		if(pressedCharacters.size() > 0) {
			std::string outString (pressedCharacters.begin(), pressedCharacters.end());
			printk((outString + "\n").c_str());
		}
		
		k_sleep(pollingDelayMs);
	}
}