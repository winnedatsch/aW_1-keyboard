#ifndef BOARD_MAPPINGS
#define BOARD_MAPPINGS

#include "keyboard_matrix_scanner.h"
#include "keycode_resolver.h"
#include "usb_hid_keys.h"

// AW_1
#ifdef BOARD_AW_1

const keyboard_pins pins{
    {0, 1, 2, 3, 4},              // rows_left
    {6, 5, 4, 3, 2, 1, 0},        // columns_left
    {16, 14, 12, 11, 10},         // rows_right
    {15, 17, 19, 31, 30, 29, 28}  // columns_right
};

const dynamic_matrix<uint8_t> keycode_matrix{
    {{KEY_ESC, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_GRAVE, KEY_BACKSPACE, KEY_6, KEY_7, KEY_8,
      KEY_9, KEY_0, KEY_MINUS},
     {KEY_TAB, KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_SLASH, KEY_LEFTBRACE, KEY_Y, KEY_U, KEY_I,
      KEY_O, KEY_P, KEY_EQUAL},
     {KEY_ENTER, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_NONE, KEY_NONE, KEY_H, KEY_J, KEY_K, KEY_L,
      KEY_SEMICOLON, KEY_APOSTROPHE},
     {KEY_LEFTSHIFT, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_BACKSLASH, KEY_RIGHTBRACE, KEY_COMMA,
      KEY_DOT, KEY_UP, KEY_RIGHTSHIFT},
     {KEY_LEFTCTRL, KEY_LEFTALT, KEY_LEFTMETA, KEY_FN, KEY_INSERT, KEY_SPACE, KEY_NONE, KEY_NONE,
      KEY_SPACE, KEY_DELETE, KEY_RIGHTALT, KEY_LEFT, KEY_DOWN, KEY_RIGHT}}};

const dynamic_matrix<uint8_t> fn_matrix{
    {{KEY_NONE, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_MEDIA_MUTE, KEY_MEDIA_PLAYPAUSE, KEY_F6,
      KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11},
     {KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_MEDIA_VOLUMEUP,
      KEY_MEDIA_NEXTSONG, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_F12},
     {KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE,
      KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE},
     {KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_MEDIA_VOLUMEDOWN,
      KEY_MEDIA_PREVIOUSSONG, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_PAGEUP, KEY_NONE},
     {KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE,
      KEY_NONE, KEY_NONE, KEY_HOME, KEY_PAGEDOWN, KEY_END}}};

const uint8_t battery_reading_pin_analogue = 2;
const uint16_t expander_i2c = 0x20;

#endif

// DEVELOPMENT BOARD
#ifdef BOARD_FEATHER
const keyboard_pins pins{
    {0, 1},   // rows_left
    {7, 6},   // columns_left
    {2, 3},   // rows_right
    {16, 15}  // columns_right
};

const dynamic_matrix<uint8_t> keycode_matrix{
    {{KEY_A, KEY_B, KEY_C, KEY_D}, {KEY_LEFTSHIFT, KEY_E, KEY_F, KEY_G}}};

const dynamic_matrix<uint8_t> fn_matrix{
    {{KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE}, {KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE}}};

const uint8_t battery_reading_pin_analogue = 7;
const uint16_t expander_i2c = 0x20;

#endif

#endif
