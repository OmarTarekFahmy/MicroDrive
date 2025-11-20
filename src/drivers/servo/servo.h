#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

// Initialize the servo on the given GPIO pin (continuous rotation servo)
void servo_init(uint32_t gpio_pin);

// Stop the servo
void servo_stop(void);

// Set servo speed: -100 (full CCW) to +100 (full CW), 0 = stop
void servo_set_speed(float speed);

// Get current estimated position
float servo_get_position(void);

// Set current position (for calibration)
void servo_set_position(float position);

// Update position based on speed and time (call regularly)
void servo_update_position(float speed, float dt_ms);

#endif
