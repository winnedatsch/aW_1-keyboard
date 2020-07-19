#include <vector>
#include <zephyr.h>
#include "usb_hid_keys.h"

typedef struct keycodes {
    std::vector<uint8_t> keycodes;
    std::vector<uint8_t> modifiers;
} keycodes;

template <typename T> using dynamic_matrix = std::vector<std::vector<T>>;

class KeycodeResolver {
    private:
        dynamic_matrix<uint8_t> keycode_matrix;
    public:
        KeycodeResolver(dynamic_matrix<uint8_t> keycode_matrix);
        keycodes resolve_keycodes(std::vector<std::pair<uint8_t, uint8_t>> pressed_keys);
};