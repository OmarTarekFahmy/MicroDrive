#ifndef TOUCH_SENSOR_H
#define TOUCH_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

// Initialize the touch sensor
// touch_pin: GPIO pin connected to touch sensor output
void touch_init(uint8_t touch_pin);

// Read touch sensor state
// Returns: true if touched, false if not touched
bool touch_is_pressed(void);

// Read touch sensor with debouncing
// Returns: true if touched (debounced), false if not touched
bool touch_read_debounced(void);

#endif
