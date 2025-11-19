/**
 * @file touch_keys.h
 * @brief Capacitive Touch Key driver for RP2040
 * 
 * This driver supports capacitive touch sensor modules (e.g., TTP223)
 * with digital output via GPIO pins.
 */

#ifndef TOUCH_KEYS_H
#define TOUCH_KEYS_H

#include "pico/stdlib.h"
#include <stdbool.h>

// Maximum number of touch keys supported
#define TOUCH_MAX_KEYS 8

// Touch key states
typedef enum {
    TOUCH_STATE_RELEASED = 0,
    TOUCH_STATE_PRESSED = 1
} touch_state_t;

// Touch key event callback type
typedef void (*touch_callback_t)(uint8_t key_index, touch_state_t state);

// Touch key configuration
typedef struct {
    uint gpio_pin;           // GPIO pin connected to touch sensor
    bool active_high;        // true if sensor outputs HIGH when touched
    uint32_t debounce_ms;    // Debounce time in milliseconds
    touch_callback_t callback; // Optional callback function
} touch_key_config_t;

// Touch key instance
typedef struct {
    uint gpio_pin;
    bool active_high;
    touch_state_t current_state;
    touch_state_t previous_state;
    absolute_time_t last_change_time;
    uint32_t debounce_ms;
    touch_callback_t callback;
} touch_key_t;

// Touch keys manager
typedef struct {
    touch_key_t keys[TOUCH_MAX_KEYS];
    uint8_t num_keys;
} touch_keys_t;

/**
 * @brief Initialize touch keys manager
 * 
 * @param touch Pointer to touch keys manager structure
 * @return true if initialization successful
 */
bool touch_keys_init(touch_keys_t *touch);

/**
 * @brief Add a touch key to the manager
 * 
 * @param touch Pointer to touch keys manager
 * @param config Configuration for the touch key
 * @return Key index if successful, -1 if failed
 */
int touch_keys_add(touch_keys_t *touch, touch_key_config_t *config);

/**
 * @brief Update touch key states (call regularly in main loop)
 * 
 * @param touch Pointer to touch keys manager
 */
void touch_keys_update(touch_keys_t *touch);

/**
 * @brief Check if a specific key is currently pressed
 * 
 * @param touch Pointer to touch keys manager
 * @param key_index Index of the key to check
 * @return true if key is pressed, false otherwise
 */
bool touch_key_is_pressed(touch_keys_t *touch, uint8_t key_index);

/**
 * @brief Check if a specific key was just pressed (rising edge)
 * 
 * @param touch Pointer to touch keys manager
 * @param key_index Index of the key to check
 * @return true if key was just pressed
 */
bool touch_key_just_pressed(touch_keys_t *touch, uint8_t key_index);

/**
 * @brief Check if a specific key was just released (falling edge)
 * 
 * @param touch Pointer to touch keys manager
 * @param key_index Index of the key to check
 * @return true if key was just released
 */
bool touch_key_just_released(touch_keys_t *touch, uint8_t key_index);

/**
 * @brief Get the number of keys currently pressed
 * 
 * @param touch Pointer to touch keys manager
 * @return Number of pressed keys
 */
uint8_t touch_keys_get_pressed_count(touch_keys_t *touch);

/**
 * @brief Set callback function for a specific key
 * 
 * @param touch Pointer to touch keys manager
 * @param key_index Index of the key
 * @param callback Callback function
 */
void touch_key_set_callback(touch_keys_t *touch, uint8_t key_index, touch_callback_t callback);

/**
 * @brief Enable/disable a specific touch key
 * 
 * @param touch Pointer to touch keys manager
 * @param key_index Index of the key
 * @param enabled true to enable, false to disable
 */
void touch_key_set_enabled(touch_keys_t *touch, uint8_t key_index, bool enabled);

#endif // TOUCH_KEYS_H
