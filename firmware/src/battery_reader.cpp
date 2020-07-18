/*
 Battery.cpp - Battery library
 Copyright (c) 2014 Roberto Lo Giacco.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as 
 published by the Free Software Foundation, either version 3 of the 
 License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "battery_reader.h"
#include <zephyr.h>
#include <drivers/adc.h>
#include <sys/util.h>
#include <string>

BatteryReader::BatteryReader(device *adc_device, uint16_t ref_voltage,  uint16_t min_voltage, uint16_t max_voltage, uint8_t sense_pin, float divider_ratio, const map_fn &map_function) : 
	adc_device{adc_device}, 
	ref_voltage{ref_voltage},
	min_voltage{min_voltage}, 
	max_voltage{max_voltage},
	divider_ratio{divider_ratio},
	map_function{map_function},
	channel_config{
		.gain = ADC_GAIN_1_6,
		.reference = ADC_REF_INTERNAL,
		.acquisition_time = ADC_ACQ_TIME_DEFAULT,
		.channel_id = sense_pin,
		.differential = 0,
		.input_positive = (uint8_t) (sense_pin + 1)
	}
{
	int error = adc_channel_setup(this->adc_device, &(this->channel_config));
	if(error) {
		printk("ADC setup failed\n");
	} else {
		printk("ADC setup succeeded\n");
	}
}

uint8_t BatteryReader::level() {
	return this->level(this->voltage());
}

uint8_t BatteryReader::level(uint16_t voltage) {
	if (voltage <= min_voltage) {
		return 0;
	} else if (voltage >= max_voltage) {
		return 100;
	} else {
		return (map_function)(voltage, min_voltage, max_voltage);
	}
}

uint16_t BatteryReader::voltage() {
	const adc_sequence sequence = {
		.options     = NULL,				// extra samples and callback
		.channels    = BIT(this->channel_config.channel_id),		// bit mask of channels to read
		.buffer      = &(this->sample_buffer),		// where to put samples read
		.buffer_size = sizeof(this->sample_buffer),
		.resolution  = 10,		// desired resolution
		.oversampling = 0,					// don't oversample
		.calibrate = 0						// don't calibrate
	};

	int error = adc_read(this->adc_device, &sequence);

	if(error) {
		printk("ADC sampling failed\n");
		return 0;
	} else {
		uint16_t reading = this->sample_buffer * divider_ratio * ref_voltage / 1024;
		printk("Battery reading: %umV\n", reading);
		return reading;
	}
}
