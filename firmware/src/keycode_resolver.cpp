#include "keycode_resolver.h"
#include <stdexcept>
#include <zephyr.h>

KeycodeResolver::KeycodeResolver(dynamic_matrix<uint8_t> keycode_matrix): keycode_matrix {keycode_matrix} {}

keycodes KeycodeResolver::resolve_keycodes(std::vector<std::pair<uint8_t, uint8_t>> pressed_keys) {
    std::vector<uint8_t> keycodes;
    std::vector<uint8_t> modifiers;

    for(std::pair<uint8_t, uint8_t> key: pressed_keys) {
        try {
            uint8_t keycode = keycode_matrix.at(key.first).at(key.second);
            if(keycode >= KEY_LEFTCTRL && keycode <= KEY_RIGHTMETA) {
                modifiers.push_back(keycode);
            } else {
                keycodes.push_back(keycode);
            }
        } catch (const std::out_of_range&) {
            printk("Key position is not defined in matrix");
        }
    }

    return {
        keycodes,
        modifiers
    };
}