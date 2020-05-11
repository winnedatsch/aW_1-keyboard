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

#include "battery.h"
#include <zephyr.h>
#include <drivers/adc.h>
#include <sys/util.h>
#include <string>

Battery::Battery(uint16_t minVoltage, uint16_t maxVoltage, device *adcDevice, uint8_t sensePin) : 
	channelConfig{
		.gain = ADC_GAIN_1_6,
		.reference = ADC_REF_INTERNAL,
		.acquisition_time = ADC_ACQ_TIME_DEFAULT,
		.channel_id = sensePin,
		.differential = 0,
		.input_positive = (uint8_t) (sensePin + 1)
	},
	adcDevice{adcDevice}, 
	minVoltage{minVoltage}, 
	maxVoltage{maxVoltage}
{}

void Battery::begin(uint16_t refVoltage, float dividerRatio, mapFn_t mapFunction) {
	this->refVoltage = refVoltage;
	this->dividerRatio = dividerRatio;
	int error = adc_channel_setup(this->adcDevice, &(this->channelConfig));
	if(error) {
		printk("ADC setup failed\n");
	} else {
		printk("ADC setup succeeded\n");
	}
	this->mapFunction = mapFunction ? mapFunction : &linear;

}

uint8_t Battery::level() {
	return this->level(this->voltage());
}

uint8_t Battery::level(uint16_t voltage) {
	if (voltage <= minVoltage) {
		return 0;
	} else if (voltage >= maxVoltage) {
		return 100;
	} else {
		return (*mapFunction)(voltage, minVoltage, maxVoltage);
	}
}

uint16_t Battery::voltage() {
	const adc_sequence sequence = {
		.options     = NULL,				// extra samples and callback
		.channels    = BIT(this->channelConfig.channel_id),		// bit mask of channels to read
		.buffer      = &(this->sampleBuffer),		// where to put samples read
		.buffer_size = sizeof(this->sampleBuffer),
		.resolution  = 10,		// desired resolution
		.oversampling = 0,					// don't oversample
		.calibrate = 0						// don't calibrate
	};

	int error = adc_read(this->adcDevice, &sequence);

	if(error) {
		printk("ADC sampling failed\n");
		return 0;
	} else {
		uint16_t reading = this->sampleBuffer * dividerRatio * refVoltage / 1024;
		printk("Battery reading: %umV\n", reading);
		return reading;
	}
}
