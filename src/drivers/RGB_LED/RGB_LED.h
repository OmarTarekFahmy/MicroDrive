#ifndef RGB_LED_H
#define RGB_LED_H

#include "pico/stdlib.h"

// Structure to represent an RGB LED connected to three GPIO pins
typedef struct {
    uint8_t red_pin;
    uint8_t green_pin;
    uint8_t blue_pin;
} RGB_LED;

enum Color {
    COLOR_OFF,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_YELLOW,
    COLOR_CYAN,
    COLOR_MAGENTA,
    COLOR_WHITE
};

// Initialize the RGB LED by setting the GPIO pins as outputs
void rgb_led_init(RGB_LED* led, uint8_t red_pin, uint8_t green_pin, uint8_t blue_pin);
void rgb_led_set_color(RGB_LED* led, bool red, bool green, bool blue);
void rgb_led_preset_color(RGB_LED* led, enum Color color);
void rgb_led_off(RGB_LED* led);

#endif // RGB_LED_H