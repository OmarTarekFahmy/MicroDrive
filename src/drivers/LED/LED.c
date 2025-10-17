#include "LED.h"

void led_init(uint8_t pin){
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
}

void led_on(uint8_t pin){
    gpio_put(pin, 1);
}

void led_off(uint8_t pin){
    gpio_put(pin, 0);
}

void led_toggle(uint8_t pin){
    gpio_put(pin, !gpio_get(pin));
}