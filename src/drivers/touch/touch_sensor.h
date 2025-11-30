#ifndef TOUCH_SENSOR_H
#define TOUCH_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

// Maximum number of touch sensors
#define MAX_TOUCH_SENSORS 4

// Touch sensor IDs
#define TOUCH_1 0
#define TOUCH_2 1
#define TOUCH_3 2
#define TOUCH_4 3

/**
 * @brief Initialize a touch sensor on the given GPIO pin
 * @param touch_id The touch sensor ID (0-3)
 * @param gpio_pin GPIO pin connected to touch sensor output
 */
void touch_init(uint8_t touch_id, uint8_t gpio_pin);

/**
 * @brief Initialize all 4 touch sensors
 * @param pin1-pin4 GPIO pins for touch sensors 1-4
 */
void touch_init_all(uint8_t pin1, uint8_t pin2, uint8_t pin3, uint8_t pin4);

/**
 * @brief Read touch sensor state (raw)
 * @param touch_id The touch sensor ID (0-3)
 * @return true if touched, false if not touched
 */
bool touch_is_pressed(uint8_t touch_id);

/**
 * @brief Read touch sensor with debouncing
 * @param touch_id The touch sensor ID (0-3)
 * @return true if touched (debounced), false if not touched
 */
bool touch_read_debounced(uint8_t touch_id);

/**
 * @brief Get which touch sensor is currently pressed
 * @return Touch sensor ID (0-3) or -1 if none pressed
 */
int8_t touch_get_pressed(void);

#endif
