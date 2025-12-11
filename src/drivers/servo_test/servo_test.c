#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include "servo_test.h"
#include <stdint.h>
#include <stdio.h>
#include <math.h>

// Servo state structure for testing
typedef struct {
    uint gpio_pin;
    uint slice_num;
    uint channel;
    uint32_t wrap_value;
    uint32_t current_pulse_us;
    bool initialized;
    bool enabled;
    // Continuous servo mode tracking
    bool continuous_mode;
    float continuous_angle;      // Current angle in degrees
    float continuous_speed_dps;  // Degrees per second at max speed
} servo_test_state_t;

// Array of servo states
static servo_test_state_t test_servos[MAX_TEST_SERVOS];

// PWM configuration constants
#define SERVO_FREQ      50      // 50 Hz (20ms period)
#define PERIOD_US       20000   // 20ms = 20000 microseconds

// Extended pulse width range for testing various servos
#define SERVO_MIN_US    500     // Minimum pulse width
#define SERVO_MAX_US    2500    // Maximum pulse width
#define SERVO_CENTER_US 1500    // Center/neutral position

// Helper function to get timestamp in milliseconds
static inline uint32_t get_timestamp_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

void servo_test_init(uint8_t servo_id, uint32_t gpio_pin) {
    if (servo_id >= MAX_TEST_SERVOS) {
        printf("Error: servo_id %d exceeds max %d\n", servo_id, MAX_TEST_SERVOS - 1);
        return;
    }
    
    servo_test_state_t *s = &test_servos[servo_id];
    
    s->gpio_pin = gpio_pin;
    s->current_pulse_us = SERVO_CENTER_US;
    s->initialized = true;
    s->enabled = true;
    s->continuous_mode = false;
    s->continuous_angle = 0.0f;
    s->continuous_speed_dps = 0.0f;
    
    // Set GPIO function to PWM
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    
    // Get PWM slice and channel for this GPIO
    s->slice_num = pwm_gpio_to_slice_num(gpio_pin);
    s->channel = pwm_gpio_to_channel(gpio_pin);

    // Configure PWM for 50Hz (20ms period)
    // Using 125MHz sys clock: 125MHz / 100 / 25000 = 50Hz
    float clkdiv = 100.0f;
    s->wrap_value = 24999;  // 25000 counts for 20ms period
    
    pwm_set_clkdiv(s->slice_num, clkdiv);
    pwm_set_wrap(s->slice_num, s->wrap_value);
    pwm_set_enabled(s->slice_num, true);
    
    // Start at center position
    servo_test_set_pulse_us(servo_id, SERVO_CENTER_US);
    
    printf("[%lu ms] === Servo %d Initialized ===\n", get_timestamp_ms(), servo_id);
    printf("[%lu ms]   GPIO Pin: %lu\n", get_timestamp_ms(), gpio_pin);
    printf("[%lu ms]   PWM Slice: %d, Channel: %d\n", get_timestamp_ms(), s->slice_num, s->channel);
    printf("[%lu ms]   PWM Frequency: %d Hz\n", get_timestamp_ms(), SERVO_FREQ);
    printf("[%lu ms]   Initial Pulse: %d µs (center)\n", get_timestamp_ms(), SERVO_CENTER_US);
    printf("[%lu ms] ============================\n\n", get_timestamp_ms());
}

void servo_test_set_pulse_us(uint8_t servo_id, uint32_t pulse_us) {
    if (servo_id >= MAX_TEST_SERVOS || !test_servos[servo_id].initialized) {
        printf("[%lu ms] Error: Servo %d not initialized\n", get_timestamp_ms(), servo_id);
        return;
    }
    
    servo_test_state_t *s = &test_servos[servo_id];
    
    // Clamp pulse width to valid range
    if (pulse_us < SERVO_MIN_US) {
        printf("[%lu ms] Warning: Pulse %lu µs clamped to min %d µs\n", get_timestamp_ms(), pulse_us, SERVO_MIN_US);
        pulse_us = SERVO_MIN_US;
    }
    if (pulse_us > SERVO_MAX_US) {
        printf("[%lu ms] Warning: Pulse %lu µs clamped to max %d µs\n", get_timestamp_ms(), pulse_us, SERVO_MAX_US);
        pulse_us = SERVO_MAX_US;
    }
    
    s->current_pulse_us = pulse_us;
    
    if (s->enabled) {
        // Calculate PWM level from pulse width
        uint16_t level = (uint16_t)(((uint64_t)pulse_us * (s->wrap_value + 1)) / PERIOD_US);
        pwm_set_chan_level(s->slice_num, s->channel, level);
        
    } else {
        printf("[%lu ms] Servo %d: Pulse set to %lu µs but PWM is disabled\n", get_timestamp_ms(), servo_id, pulse_us);
    }
}

void servo_test_center(uint8_t servo_id) {
    servo_test_set_pulse_us(servo_id, SERVO_CENTER_US);
}

void servo_test_set_angle(uint8_t servo_id, float angle) {
    // Clamp angle to -90 to +90
    if (angle < -90.0f) angle = -90.0f;
    if (angle > 90.0f) angle = 90.0f;
    
    // Linear mapping: -90° -> 1000µs, 0° -> 1500µs, +90° -> 2000µs
    // Standard servo range
    uint32_t pulse_us = (uint32_t)(1500.0f + (angle / 90.0f) * 1000.0f);
    
    servo_test_set_pulse_us(servo_id, pulse_us);
}

uint32_t servo_test_get_pulse_us(uint8_t servo_id) {
    if (servo_id >= MAX_TEST_SERVOS || !test_servos[servo_id].initialized) {
        return 0;
    }
    return test_servos[servo_id].current_pulse_us;
}

void servo_test_disable(uint8_t servo_id) {
    if (servo_id >= MAX_TEST_SERVOS || !test_servos[servo_id].initialized) return;
    
    servo_test_state_t *s = &test_servos[servo_id];
    s->enabled = false;
    
    // Set PWM level to 0 (no pulse)
    pwm_set_chan_level(s->slice_num, s->channel, 0);
    printf("[%lu ms] Servo %d: PWM disabled\n", get_timestamp_ms(), servo_id);
}

void servo_test_enable(uint8_t servo_id) {
    if (servo_id >= MAX_TEST_SERVOS || !test_servos[servo_id].initialized) return;
    
    servo_test_state_t *s = &test_servos[servo_id];
    s->enabled = true;
    
    // Restore the current pulse width
    uint16_t level = (uint16_t)(((uint64_t)s->current_pulse_us * (s->wrap_value + 1)) / PERIOD_US);
    pwm_set_chan_level(s->slice_num, s->channel, level);
    printf("[%lu ms] Servo %d: PWM enabled, pulse = %lu µs\n", get_timestamp_ms(), servo_id, s->current_pulse_us);
}

void servo_test_print_info(uint8_t servo_id) {
    if (servo_id >= MAX_TEST_SERVOS || !test_servos[servo_id].initialized) {
        printf("[%lu ms] Servo %d: Not initialized\n", get_timestamp_ms(), servo_id);
        return;
    }
    
    servo_test_state_t *s = &test_servos[servo_id];
    
    printf("\n[%lu ms] === Servo %d Info ===\n", get_timestamp_ms(), servo_id);
    printf("[%lu ms]   GPIO Pin: %d\n", get_timestamp_ms(), s->gpio_pin);
    printf("[%lu ms]   PWM Slice: %d, Channel: %d\n", get_timestamp_ms(), s->slice_num, s->channel);
    printf("[%lu ms]   Current Pulse: %lu µs\n", get_timestamp_ms(), s->current_pulse_us);
    printf("[%lu ms]   Enabled: %s\n", get_timestamp_ms(), s->enabled ? "Yes" : "No");
    printf("[%lu ms] =====================\n", get_timestamp_ms());
    
    printf("\n--- Pulse Width Reference ---\n");
    printf("  500 µs  = Extended min (may not work on all servos)\n");
    printf("  1000 µs = Standard min (-90° positional / full CCW continuous)\n");
    printf("  1500 µs = Center (0° positional / STOP continuous)\n");
    printf("  2000 µs = Standard max (+90° positional / full CW continuous)\n");
    printf("  2500 µs = Extended max (may not work on all servos)\n");
    printf("-----------------------------\n\n");
}

void servo_test_set_continuous_mode(uint8_t servo_id, float speed_dps) {
    if (servo_id >= MAX_TEST_SERVOS || !test_servos[servo_id].initialized) {
        printf("[%lu ms] Error: Servo %d not initialized\n", get_timestamp_ms(), servo_id);
        return;
    }
    
    servo_test_state_t *s = &test_servos[servo_id];
    s->continuous_mode = true;
    s->continuous_angle = 0.0f;
    s->continuous_speed_dps = speed_dps;
    
    printf("[%lu ms] Servo %d: Continuous mode enabled\n", get_timestamp_ms(), servo_id);
    printf("[%lu ms]   Speed: %.1f degrees/second\n", get_timestamp_ms(), speed_dps);
    printf("[%lu ms]   Starting angle: 0.0°\n", get_timestamp_ms());
    
    // Center servo to stop any rotation
    servo_test_center(servo_id);
}

bool servo_test_move_continuous_angle(uint8_t servo_id, float target_angle) {
    if (servo_id >= MAX_TEST_SERVOS || !test_servos[servo_id].initialized) {
        printf("[%lu ms] Error: Servo %d not initialized\n", get_timestamp_ms(), servo_id);
        return false;
    }
    
    servo_test_state_t *s = &test_servos[servo_id];
    
    if (!s->continuous_mode) {
        printf("[%lu ms] Error: Servo %d not in continuous mode. Call servo_test_set_continuous_mode() first.\n", 
               get_timestamp_ms(), servo_id);
        return false;
    }
    
    if (s->continuous_speed_dps <= 0.0f) {
        printf("[%lu ms] Error: Invalid speed configuration\n", get_timestamp_ms());
        return false;
    }
    
    float current_angle = s->continuous_angle;
    float angle_diff = target_angle - current_angle;
    
    // If already at target, no movement needed
    if (fabsf(angle_diff) < 0.5f) {
        s->continuous_angle = target_angle;
        return true;
    }
    
    // Determine rotation direction
    bool clockwise = angle_diff > 0;
    float abs_angle_diff = fabsf(angle_diff);
    
    // Calculate movement time based on speed
    // Time = angle / speed
    uint32_t move_time_ms = (uint32_t)((abs_angle_diff / s->continuous_speed_dps) * 1000.0f);
    
    // Determine pulse width for rotation
    // Clockwise = higher pulse (2500µs max), Counter-clockwise = lower pulse (500µs min)
    uint32_t pulse_us = clockwise ? SERVO_MAX_US : SERVO_MIN_US;
    
    // Start rotation
    servo_test_set_pulse_us(servo_id, pulse_us);
    
    // Wait for movement to complete
    sleep_ms(move_time_ms);
    
    // Stop servo
    servo_test_center(servo_id);
    
    // Update current angle
    s->continuous_angle = target_angle;
        
    return true;
}

float servo_test_get_continuous_angle(uint8_t servo_id) {
    if (servo_id >= MAX_TEST_SERVOS || !test_servos[servo_id].initialized) {
        return 0.0f;
    }
    return test_servos[servo_id].continuous_angle;
}

void servo_test_reset_continuous_angle(uint8_t servo_id) {
    if (servo_id >= MAX_TEST_SERVOS || !test_servos[servo_id].initialized) {
        printf("[%lu ms] Error: Servo %d not initialized\n", get_timestamp_ms(), servo_id);
        return;
    }
    
    servo_test_state_t *s = &test_servos[servo_id];
    
    if (!s->continuous_mode) {
        printf("[%lu ms] Warning: Servo %d not in continuous mode\n", get_timestamp_ms(), servo_id);
    }
    
    s->continuous_angle = 0.0f;
    printf("[%lu ms] Servo %d: Angle reset to 0.0°\n", get_timestamp_ms(), servo_id);
}
