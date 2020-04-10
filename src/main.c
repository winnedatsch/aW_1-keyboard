#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <sys/printk.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

#define PIN 25

void main(void)
{
	struct device *dev;
	bool led_is_on = true;
	int ret;

	dev = device_get_binding("GPIO_0");
	if (dev == NULL) {
		printk("device not found");
		return;
	}

	ret = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("gpio coult not be set to output");
		return;
	}

	while (1) {
		printk("holla");
		gpio_pin_set(dev, PIN, (int)led_is_on);
		led_is_on = !led_is_on;
		k_sleep(SLEEP_TIME_MS);
	}
}