#include "rgb_led.h"


// Initialize the RGB LED by setting the GPIO pins as outputs
void rgb_led_init(RGB_LED* led, uint8_t red_pin, uint8_t green_pin, uint8_t blue_pin) {

    //Assign pin numbers to the LED structure
    led->red_pin = red_pin;
    led->green_pin = green_pin;
    led->blue_pin = blue_pin;

    //Initialize GPIO pins
    gpio_init(red_pin);
    gpio_init(green_pin);
    gpio_init(blue_pin);

    gpio_set_dir(red_pin, GPIO_OUT);
    gpio_set_dir(green_pin, GPIO_OUT);
    gpio_set_dir(blue_pin, GPIO_OUT);
}

void rgb_led_set_color(RGB_LED* led, bool red, bool green, bool blue){
    gpio_put(led->red_pin, red);
    gpio_put(led->green_pin, green);
    gpio_put(led->blue_pin, blue);
}

void rgb_led_preset_color(RGB_LED* led, enum Color color){

    switch (color)
    {
    case COLOR_OFF:
        rgb_led_set_color(led, 0, 0, 0);
        break;
    case COLOR_RED:
        rgb_led_set_color(led, 1, 0, 0);
        break;
    case COLOR_GREEN:
        rgb_led_set_color(led, 0, 1, 0);
        break;
    case COLOR_BLUE:
        rgb_led_set_color(led, 0, 0, 1);
        break;
    case COLOR_YELLOW:
        rgb_led_set_color(led, 1, 1, 0);
        break;
    case COLOR_CYAN:
        rgb_led_set_color(led, 0, 1, 1);
        break;
    case COLOR_MAGENTA:
        rgb_led_set_color(led, 1, 0, 1);
        break;
    case COLOR_WHITE:
        rgb_led_set_color(led, 1, 1, 1);
        break;
    default:
        break;
    }

}

void rgb_led_off(RGB_LED* led){
    rgb_led_set_color(led, 0, 0, 0);
}