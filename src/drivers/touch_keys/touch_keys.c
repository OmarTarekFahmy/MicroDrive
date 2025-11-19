/**
 * @file touch_keys.c
 * @brief Capacitive Touch Key driver implementation
 */

#include "touch_keys.h"
#include <string.h>

bool touch_keys_init(touch_keys_t *touch) {
    if (touch == NULL) return false;
    
    memset(touch, 0, sizeof(touch_keys_t));
    touch->num_keys = 0;
    
    return true;
}

int touch_keys_add(touch_keys_t *touch, touch_key_config_t *config) {
    if (touch == NULL || config == NULL) return -1;
    if (touch->num_keys >= TOUCH_MAX_KEYS) return -1;
    
    uint8_t index = touch->num_keys;
    touch_key_t *key = &touch->keys[index];
    
    // Store configuration
    key->gpio_pin = config->gpio_pin;
    key->active_high = config->active_high;
    key->debounce_ms = config->debounce_ms > 0 ? config->debounce_ms : 50;
    key->callback = config->callback;
    key->current_state = TOUCH_STATE_RELEASED;
    key->previous_state = TOUCH_STATE_RELEASED;
    key->last_change_time = get_absolute_time();
    
    // Initialize GPIO pin
    gpio_init(key->gpio_pin);
    gpio_set_dir(key->gpio_pin, GPIO_IN);
    
    // Set pull-up or pull-down based on active level
    if (key->active_high) {
        gpio_pull_down(key->gpio_pin);  // Pull down when active high
    } else {
        gpio_pull_up(key->gpio_pin);    // Pull up when active low
    }
    
    touch->num_keys++;
    return index;
}

void touch_keys_update(touch_keys_t *touch) {
    if (touch == NULL) return;
    
    absolute_time_t current_time = get_absolute_time();
    
    for (uint8_t i = 0; i < touch->num_keys; i++) {
        touch_key_t *key = &touch->keys[i];
        
        // Read current GPIO state
        bool gpio_state = gpio_get(key->gpio_pin);
        
        // Determine touch state based on active level
        touch_state_t new_state;
        if (key->active_high) {
            new_state = gpio_state ? TOUCH_STATE_PRESSED : TOUCH_STATE_RELEASED;
        } else {
            new_state = gpio_state ? TOUCH_STATE_RELEASED : TOUCH_STATE_PRESSED;
        }
        
        // Check if state has changed
        if (new_state != key->current_state) {
            // Check debounce time
            uint64_t time_diff = absolute_time_diff_us(key->last_change_time, current_time);
            
            if (time_diff >= (key->debounce_ms * 1000)) {
                // Update state
                key->previous_state = key->current_state;
                key->current_state = new_state;
                key->last_change_time = current_time;
                
                // Call callback if registered
                if (key->callback != NULL) {
                    key->callback(i, new_state);
                }
            }
        }
    }
}

bool touch_key_is_pressed(touch_keys_t *touch, uint8_t key_index) {
    if (touch == NULL || key_index >= touch->num_keys) return false;
    return touch->keys[key_index].current_state == TOUCH_STATE_PRESSED;
}

bool touch_key_just_pressed(touch_keys_t *touch, uint8_t key_index) {
    if (touch == NULL || key_index >= touch->num_keys) return false;
    
    touch_key_t *key = &touch->keys[key_index];
    return (key->current_state == TOUCH_STATE_PRESSED && 
            key->previous_state == TOUCH_STATE_RELEASED);
}

bool touch_key_just_released(touch_keys_t *touch, uint8_t key_index) {
    if (touch == NULL || key_index >= touch->num_keys) return false;
    
    touch_key_t *key = &touch->keys[key_index];
    return (key->current_state == TOUCH_STATE_RELEASED && 
            key->previous_state == TOUCH_STATE_PRESSED);
}

uint8_t touch_keys_get_pressed_count(touch_keys_t *touch) {
    if (touch == NULL) return 0;
    
    uint8_t count = 0;
    for (uint8_t i = 0; i < touch->num_keys; i++) {
        if (touch->keys[i].current_state == TOUCH_STATE_PRESSED) {
            count++;
        }
    }
    return count;
}

void touch_key_set_callback(touch_keys_t *touch, uint8_t key_index, touch_callback_t callback) {
    if (touch == NULL || key_index >= touch->num_keys) return;
    touch->keys[key_index].callback = callback;
}

void touch_key_set_enabled(touch_keys_t *touch, uint8_t key_index, bool enabled) {
    if (touch == NULL || key_index >= touch->num_keys) return;
    
    if (enabled) {
        gpio_set_dir(touch->keys[key_index].gpio_pin, GPIO_IN);
    } else {
        // Disable by setting to output low
        gpio_set_dir(touch->keys[key_index].gpio_pin, GPIO_OUT);
        gpio_put(touch->keys[key_index].gpio_pin, 0);
    }
}
