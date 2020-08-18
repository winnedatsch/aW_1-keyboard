#ifndef BATTERY_READER
#define BATTERY_READER

#include <device.h>
#include <drivers/adc.h>
#include <drivers/gpio.h>
#include <math.h>
#include <zephyr.h>

#include <functional>
#include <memory>

typedef std::function<uint8_t(uint16_t, uint16_t, uint16_t)> map_fn;

class BatteryReader {
   private:
    std::shared_ptr<device> adc_device;
    uint16_t ref_voltage;
    uint16_t min_voltage;
    uint16_t max_voltage;
    float divider_ratio;
    const map_fn map_function;
    adc_channel_cfg channel_config;
    int16_t sample_buffer;

   public:
    /**
     * Creates an instance to monitor battery voltage and level.
     * Initialization parameters depend on battery type and configuration.
     *
     * @param adc_device is the zephyr ADC device that the sense_pin is bound to
     * @param ref_voltage is the board reference voltage, expressed in millivolts
     * @param min_voltage is the voltage, expressed in millivolts, corresponding to an empty battery
     * @param max_voltage is the voltage, expressed in millivolts, corresponding to a full battery
     * @param sense_pin is the analog pin used for sensing the battery voltage
     * @param divider_ratio is the multiplier used to obtain the real battery voltage
     * @param map_fn is the function that will map the voltage reading to a battery percentage
     */
    BatteryReader(std::shared_ptr<device> adc_device, uint16_t ref_voltage, uint16_t min_voltage,
                  uint16_t max_voltage, uint8_t sense_pin, float divider_ratio,
                  const map_fn map_function);

    /**
     * Returns the current battery level as a number between 0 and 100, with 0 indicating an empty
     * battery and 100 a full battery.
     */
    uint8_t level();
    uint8_t level(uint16_t voltage);

    /**
     * Returns the current battery voltage in millivolts.
     */
    uint16_t voltage();
};

//
// Plots of the functions below available at
// https://www.desmos.com/calculator/x0esk5bsrk
//

/**
 * Symmetric sigmoidal approximation
 * https://www.desmos.com/calculator/7m9lu26vpy
 *
 * c - c / (1 + k*x/v)^3
 */
static inline uint8_t sigmoidal(uint16_t voltage, uint16_t min_voltage, uint16_t max_voltage) {
    // slow
    // uint8_t result = 110 - (110 / (1 + pow(1.468 * (voltage - min_voltage)/(max_voltage -
    // min_voltage), 6)));

    // steep
    // uint8_t result = 102 - (102 / (1 + pow(1.621 * (voltage - min_voltage)/(max_voltage -
    // min_voltage), 8.1)));

    // normal
    uint8_t result =
        105 - (105 / (1 + pow(1.724 * (voltage - min_voltage) / (max_voltage - min_voltage), 5.5)));
    return result >= 100 ? 100 : result;
}

/**
 * Asymmetric sigmoidal approximation
 * https://www.desmos.com/calculator/oyhpsu8jnw
 *
 * c - c / [1 + (k*x/v)^4.5]^3
 */
static inline uint8_t asigmoidal(uint16_t voltage, uint16_t min_voltage, uint16_t max_voltage) {
    uint8_t result =
        101 -
        (101 / pow(1 + pow(1.33 * (voltage - min_voltage) / (max_voltage - min_voltage), 4.5), 3));
    return result >= 100 ? 100 : result;
}

/**
 * Linear mapping
 * https://www.desmos.com/calculator/sowyhttjta
 *
 * x * 100 / v
 */
static inline uint8_t linear(uint16_t voltage, uint16_t min_voltage, uint16_t max_voltage) {
    return (unsigned long)(voltage - min_voltage) * 100 / (max_voltage - min_voltage);
}

#endif