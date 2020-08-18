#include "keycode_resolver.h"

#include <zephyr.h>

#include <algorithm>
#include <stdexcept>

KeycodeResolver::KeycodeResolver(dynamic_matrix<uint8_t> keycode_matrix,
                                 dynamic_matrix<uint8_t> fn_matrix)
    : keycode_matrix{keycode_matrix}, fn_matrix{fn_matrix} {
    for (uint8_t rowIndex = 0; rowIndex < keycode_matrix.size(); rowIndex++) {
        const auto row = keycode_matrix.at(rowIndex);

        for (uint8_t columnIndex = 0; columnIndex < row.size(); columnIndex++) {
            if (row.at(columnIndex) == KEY_FN) {
                fn_locations.push_back({rowIndex, columnIndex});
            }
        }
    }
}

keycodes KeycodeResolver::resolve_keycodes(std::vector<std::pair<uint8_t, uint8_t>> pressed_keys) {
    std::vector<uint8_t> keycodes;
    std::vector<uint8_t> modifiers;

    auto fn_pressed = false;
    for (auto fn_key : fn_locations) {
        if (std::find(pressed_keys.begin(), pressed_keys.end(), fn_key) != pressed_keys.end()) {
            fn_pressed = true;
            break;
        }
    }

    for (std::pair<uint8_t, uint8_t> key : pressed_keys) {
        try {
            uint8_t keycode;
            if (fn_pressed == true) {
                const auto fn_code = fn_matrix.at(key.first).at(key.second);
                if (fn_code != KEY_NONE) {
                    keycode = fn_code;
                } else {
                    keycode = keycode_matrix.at(key.first).at(key.second);
                }
            } else {
                keycode = keycode_matrix.at(key.first).at(key.second);
            }

            if (keycode >= KEY_LEFTCTRL && keycode <= KEY_RIGHTMETA) {
                modifiers.push_back(keycode);
            } else if (keycode != KEY_FN) {
                keycodes.push_back(keycode);
            }
        } catch (const std::out_of_range&) {
            printk("Key position is not defined in matrix");
        }
    }

    return {keycodes, modifiers};
}