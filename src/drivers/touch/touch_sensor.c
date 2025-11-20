#include "touch_sensor.h"
#include "pico/stdlib.h"

static uint8_t touch_pin;
static bool last_state = false;
static uint32_t last_change_time = 0;

#define DEBOUNCE_TIME_MS 50  // 50ms debounce time

void touch_init(uint8_t pin) {
    touch_pin = pin;
    
    // Initialize GPIO as input with pull-down
    gpio_init(touch_pin);
    gpio_set_dir(touch_pin, GPIO_IN);
    gpio_pull_down(touch_pin);  // Pull-down so it reads LOW when not touched
}

bool touch_is_pressed(void) {
    // Read the pin state
    // Capacitive touch sensors typically output HIGH when touched
    return gpio_get(touch_pin);
}

bool touch_read_debounced(void) {
    bool current_state = touch_is_pressed();
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    // Check if state has changed
    if (current_state != last_state) {
        // Check if enough time has passed since last change (debouncing)
        if ((current_time - last_change_time) > DEBOUNCE_TIME_MS) {
            last_state = current_state;
            last_change_time = current_time;
            return current_state;
        }
    }
    
    return last_state;
}
