#include "pico/stdlib.h"
#include "drivers/rgb_led/rgb_led.h"
#include <malloc.h>

// Define LED pin here
#ifndef LED_PIN
#define LED_PIN 10  // Default pin is 10
#endif

int main() {
    // Initialize the LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, true);

    RGB_LED* led = malloc(sizeof(RGB_LED));
    rgb_led_init(led, 15, 14, 13);

    while (true) {
        // Generate all 7 colors with a 50ms delay between each
        rgb_led_set_color(led, true, false, false); // Red
        sleep_ms(50);
        rgb_led_set_color(led, false, true, false); // Green
        sleep_ms(50);
        rgb_led_set_color(led, false, false, true); // Blue
        sleep_ms(50);
        rgb_led_set_color(led, true, true, false); // Yellow
        sleep_ms(50);
        rgb_led_set_color(led, true, false, true); // Magenta
        sleep_ms(50);
        rgb_led_set_color(led, false, true, true); // Cyan
        sleep_ms(50);
        rgb_led_set_color(led, true, true, true); // White
        sleep_ms(50);
    }
}
