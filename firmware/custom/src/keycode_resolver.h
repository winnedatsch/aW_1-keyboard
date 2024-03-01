#ifndef KEYCODE_RESOLVER
#define KEYCODE_RESOLVER

#include <zephyr.h>

#include <vector>

#include "usb_hid_keys.h"

typedef struct keycodes {
    std::vector<uint8_t> keycodes;
    std::vector<uint8_t> modifiers;
} keycodes;

template <typename T>
using dynamic_matrix = std::vector<std::vector<T>>;

class KeycodeResolver {
   private:
    dynamic_matrix<uint8_t> keycode_matrix;
    dynamic_matrix<uint8_t> fn_matrix;
    std::vector<std::pair<uint8_t, uint8_t>> fn_locations;

   public:
    KeycodeResolver(dynamic_matrix<uint8_t> keycode_matrix, dynamic_matrix<uint8_t> fn_matrix);
    keycodes resolve_keycodes(std::vector<std::pair<uint8_t, uint8_t>> pressed_keys);
};

#endif