#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "servo.h"
#include <stdint.h>
#include <math.h>

static uint servo_gpio;       // GPIO pin used for servo
static uint slice_num;        // PWM slice for the GPIO
static uint channel_num;      // PWM channel
static uint32_t wrap_value;   // Store wrap value for calculations

// Continuous rotation servo parameters (SG90 360°)
#define SERVO_STOP_US   1500    // 1.5ms = stop
#define SERVO_CW_MAX_US 1000    // 1ms = full speed clockwise
#define SERVO_CCW_MAX_US 2000   // 2ms = full speed counter-clockwise
#define SERVO_FREQ   50         // 50 Hz

// Track current estimated position
static float current_position = 0.0f;

void servo_init(uint32_t gpio_pin) {
    servo_gpio = gpio_pin;
    
    // Set GPIO function to PWM
    gpio_set_function(servo_gpio, GPIO_FUNC_PWM);
    
    // Find out which PWM slice is connected to the specified GPIO
    slice_num = pwm_gpio_to_slice_num(servo_gpio);
    channel_num = pwm_gpio_to_channel(servo_gpio);

    // Get the system clock frequency
    uint32_t sys_clk = clock_get_hz(clk_sys);
    
    // Calculate values for 50Hz PWM signal
    // PWM frequency = sys_clk / (div * (wrap + 1))
    // For 50Hz with 125MHz clock:
    // 50 = 125000000 / (div * (wrap + 1))
    // Using div = 64 and wrap = 39062 gives us ~50Hz
    float divider = 64.0f;
    wrap_value = (sys_clk / (divider * SERVO_FREQ)) - 1;
    
    pwm_set_clkdiv(slice_num, divider);
    pwm_set_wrap(slice_num, wrap_value);
    
    // Enable PWM
    pwm_set_enabled(slice_num, true);
    
    // Start with servo stopped
    servo_stop();
    
    // Initialize position to 0
    current_position = 0.0f;
}

// Stop the servo (neutral position)
void servo_stop(void) {
    float pulse_us = SERVO_STOP_US;
    float pulse_ratio = pulse_us / 20000.0f;
    uint16_t level = (uint16_t)(pulse_ratio * (wrap_value + 1));
    pwm_set_gpio_level(servo_gpio, level);
}

// Set servo speed: -100 (full CCW) to +100 (full CW), 0 = stop
void servo_set_speed(float speed) {
    // Constrain speed to -100 to +100
    if (speed < -100.0f) speed = -100.0f;
    if (speed > 100.0f) speed = 100.0f;
    
    float pulse_us;
    
    if (speed > 0) {
        // Clockwise: 1500us (stop) to 1000us (full speed)
        pulse_us = SERVO_STOP_US - (speed / 100.0f) * (SERVO_STOP_US - SERVO_CW_MAX_US);
    } else if (speed < 0) {
        // Counter-clockwise: 1500us (stop) to 2000us (full speed)
        pulse_us = SERVO_STOP_US + (-speed / 100.0f) * (SERVO_CCW_MAX_US - SERVO_STOP_US);
    } else {
        // Stop
        pulse_us = SERVO_STOP_US;
    }
    
    float pulse_ratio = pulse_us / 20000.0f;
    uint16_t level = (uint16_t)(pulse_ratio * (wrap_value + 1));
    pwm_set_gpio_level(servo_gpio, level);
}

// Get current estimated position
float servo_get_position(void) {
    return current_position;
}

// Set current position (for calibration)
void servo_set_position(float position) {
    current_position = position;
}

// Update position based on speed and time
void servo_update_position(float speed, float dt_ms) {
    // Estimate: at full speed (100), servo rotates ~60 degrees per second
    // This is an approximation and may need calibration
    float degrees_per_second = (speed / 100.0f) * 60.0f;
    float degrees_moved = degrees_per_second * (dt_ms / 1000.0f);
    current_position += degrees_moved;
    
    // Keep position in 0-180 range (or adjust as needed)
    if (current_position < 0) current_position = 0;
    if (current_position > 180) current_position = 180;
}
