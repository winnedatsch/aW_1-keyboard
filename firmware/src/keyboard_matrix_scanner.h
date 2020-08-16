#ifndef KEYBOARD_MATRIX_SCANNER
#define KEYBOARD_MATRIX_SCANNER


#include <device.h>
#include <vector>
#include <utility>
#include <memory>


typedef struct keyboard_pins {
    std::vector<uint8_t> rows_left;
    std::vector<uint8_t> columns_left;
    std::vector<uint8_t> rows_right;
    std::vector<uint8_t> columns_right;
} keyboard_pins;

class KeyboardMatrixScanner
{
private:
    std::shared_ptr<device> gpio;
    std::shared_ptr<device> i2c;
    uint8_t left_i2c_id;
    keyboard_pins pins;
    bool i2c_initialised = false;
    
    std::vector<std::pair<uint8_t, uint8_t>> scan_left();
    std::vector<std::pair<uint8_t, uint8_t>> scan_right();

public:
    KeyboardMatrixScanner(std::shared_ptr<device> gpio, std::shared_ptr<device> i2c, uint8_t left_i2c_id, keyboard_pins pins);
    std::vector<std::pair<uint8_t, uint8_t>> scan_matrix();
};

#endif