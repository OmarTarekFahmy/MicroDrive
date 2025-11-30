#include "touch_sensor.h"
#include "pico/stdlib.h"

typedef struct {
    uint8_t gpio_pin;
    bool initialized;
} touch_state_t;

static touch_state_t touch_sensors[MAX_TOUCH_SENSORS];

void touch_init(uint8_t touch_id, uint8_t gpio_pin) {
    if (touch_id >= MAX_TOUCH_SENSORS) return;
    
    touch_state_t *t = &touch_sensors[touch_id];
    t->gpio_pin = gpio_pin;
    t->initialized = true;
    
    // Initialize GPIO as input with pull-down
    gpio_init(gpio_pin);
    gpio_set_dir(gpio_pin, GPIO_IN);
    gpio_pull_down(gpio_pin);
}

void touch_init_all(uint8_t pin1, uint8_t pin2, uint8_t pin3, uint8_t pin4) {
    touch_init(TOUCH_1, pin1);
    touch_init(TOUCH_2, pin2);
    touch_init(TOUCH_3, pin3);
    touch_init(TOUCH_4, pin4);
}

bool touch_is_pressed(uint8_t touch_id) {
    if (touch_id >= MAX_TOUCH_SENSORS || !touch_sensors[touch_id].initialized) 
        return false;
    return gpio_get(touch_sensors[touch_id].gpio_pin);
}

bool touch_read_debounced(uint8_t touch_id) {
    // Just use raw read - capacitive touch modules have built-in debounce
    return touch_is_pressed(touch_id);
}

int8_t touch_get_pressed(void) {
    for (uint8_t i = 0; i < MAX_TOUCH_SENSORS; i++) {
        if (touch_is_pressed(i)) {
            return i;
        }
    }
    return -1;  // None pressed
}
