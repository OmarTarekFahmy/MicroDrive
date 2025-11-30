#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "servo.h"
#include <stdint.h>
#include <stdio.h>
#include <math.h>

// Servo state structure
typedef struct {
    uint gpio_pin;
    uint slice_num;
    uint channel;
    uint32_t wrap_value;
    float current_angle;    // Current tracked angle
    float target_angle;     // Target angle
    uint8_t servo_type;     // SERVO_TYPE_180 or SERVO_TYPE_360
    bool initialized;
} servo_state_t;

// Array of servo states
static servo_state_t servos[MAX_SERVOS];

// MG996R servo parameters (from datasheet)
// Standard servo PWM: 50Hz, pulse width 1000-2000µs
#define SERVO_FREQ      50      // 50 Hz (20ms period)
#define PERIOD_US       20000   // 20ms = 20000 microseconds

// MG996R pulse widths from datasheet:
// -90° = 1.0ms, 0° = 1.5ms (center), +90° = 2.0ms
#define SERVO_MIN_US    1000    // 1.0ms = -90 degrees (or full speed CCW for 360)
#define SERVO_MAX_US    2000    // 2.0ms = +90 degrees (or full speed CW for 360)
#define SERVO_CENTER_US 1500    // 1.5ms = 0 degrees (or stop for 360)

// Helper function to set PWM pulse width in microseconds for a specific servo
static void set_pulse_width(uint8_t servo_id, uint32_t pulse_us) {
    if (servo_id >= MAX_SERVOS || !servos[servo_id].initialized) return;
    
    // Clamp pulse width to valid range
    if (pulse_us < SERVO_MIN_US) pulse_us = SERVO_MIN_US;
    if (pulse_us > SERVO_MAX_US) pulse_us = SERVO_MAX_US;
    
    uint16_t level = (uint16_t)(((uint64_t)pulse_us * (servos[servo_id].wrap_value + 1)) / PERIOD_US);
    pwm_set_chan_level(servos[servo_id].slice_num, servos[servo_id].channel, level);
}

// Convert angle to pulse width for 180° positional servo
// Angle range: -90° to +90°
static uint32_t angle_to_pulse_180(float angle) {
    if (angle < -90.0f) angle = -90.0f;
    if (angle > 90.0f) angle = 90.0f;
    
    // Linear: -90° -> 1000µs, 0° -> 1500µs, +90° -> 2000µs
    float pulse_us = SERVO_CENTER_US + (angle / 90.0f) * (SERVO_MAX_US - SERVO_CENTER_US);
    return (uint32_t)pulse_us;
}

// Convert speed (-1 to +1) to pulse width for 360° continuous rotation servo
// speed: -1.0 = full CCW, 0.0 = stop, +1.0 = full CW
static uint32_t speed_to_pulse_360(float speed) {
    if (speed < -1.0f) speed = -1.0f;
    if (speed > 1.0f) speed = 1.0f;
    
    // Linear: -1.0 -> 1000µs, 0.0 -> 1500µs, +1.0 -> 2000µs
    float pulse_us = SERVO_CENTER_US + speed * (SERVO_MAX_US - SERVO_CENTER_US);
    return (uint32_t)pulse_us;
}

void servo_init(uint8_t servo_id, uint32_t gpio_pin, uint8_t servo_type) {
    if (servo_id >= MAX_SERVOS) return;
    
    servo_state_t *s = &servos[servo_id];
    
    s->gpio_pin = gpio_pin;
    s->servo_type = servo_type;
    s->current_angle = 0.0f;
    s->target_angle = 0.0f;
    s->initialized = true;
    
    // Set GPIO function to PWM
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    
    // Get PWM slice and channel for this GPIO
    s->slice_num = pwm_gpio_to_slice_num(gpio_pin);
    s->channel = pwm_gpio_to_channel(gpio_pin);

    // Configure PWM for 50Hz (20ms period)
    uint32_t sys_clk = clock_get_hz(clk_sys);
    float clkdiv = 100.0f;
    s->wrap_value = 24999;  // 25000 counts for 20ms period
    
    printf("Servo %d init: GPIO=%lu, type=%s\n",
           servo_id, gpio_pin, servo_type == SERVO_TYPE_360 ? "360-cont" : "180-pos");
    
    pwm_set_clkdiv(s->slice_num, clkdiv);
    pwm_set_wrap(s->slice_num, s->wrap_value);
    pwm_set_enabled(s->slice_num, true);
    
    // Start at center/stop position
    set_pulse_width(servo_id, SERVO_CENTER_US);
}

void servo_set_angle(uint8_t servo_id, float angle) {
    if (servo_id >= MAX_SERVOS || !servos[servo_id].initialized) return;
    
    servo_state_t *s = &servos[servo_id];
    
    // Clamp target angle
    if (angle < -90.0f) angle = -90.0f;
    if (angle > 90.0f) angle = 90.0f;
    
    s->target_angle = angle;
    
    if (s->servo_type == SERVO_TYPE_180) {
        // 180° POSITIONAL SERVO: Direct position control
        s->current_angle = angle;
        uint32_t pulse_us = angle_to_pulse_180(angle);
        set_pulse_width(servo_id, pulse_us);
        printf("Servo %d (180): angle=%.1f -> pulse=%lu\n", servo_id, angle, pulse_us);
    } 
    else {
        // 360° CONTINUOUS ROTATION SERVO: Proportional speed control
        // Speed proportional to angle difference - no blocking!
        float angle_diff = angle - s->current_angle;
        
        // Very small dead zone (0.5°)
        if (fabsf(angle_diff) < 0.5f) {
            set_pulse_width(servo_id, SERVO_CENTER_US);  // Stop
            s->current_angle = angle;
            return;
        }
        
        // Speed proportional to error: full speed at 45° diff, scaled down for smaller
        float speed = angle_diff / 45.0f;
        if (speed > 1.0f) speed = 1.0f;
        if (speed < -1.0f) speed = -1.0f;
        
        // Minimum speed to overcome friction (lower threshold for sensitivity)
        if (speed > 0 && speed < 0.15f) speed = 0.15f;
        if (speed < 0 && speed > -0.15f) speed = -0.15f;
        
        // Set speed - don't block, let main loop call repeatedly
        uint32_t pulse_us = speed_to_pulse_360(speed);
        set_pulse_width(servo_id, pulse_us);
        
        // Estimate position change per call (assuming ~50ms update interval)
        float angle_change = speed * SERVO_360_SPEED_DPS * 0.05f;
        s->current_angle += angle_change;
        
        printf("360[%d]: diff=%.1f spd=%.2f\\n", servo_id, angle_diff, speed);
    }
}

float servo_get_angle(uint8_t servo_id) {
    if (servo_id >= MAX_SERVOS || !servos[servo_id].initialized) return 0.0f;
    return servos[servo_id].current_angle;
}

void servo_stop(uint8_t servo_id) {
    if (servo_id >= MAX_SERVOS || !servos[servo_id].initialized) return;
    set_pulse_width(servo_id, SERVO_CENTER_US);
}

void servo_center(uint8_t servo_id) {
    if (servo_id >= MAX_SERVOS || !servos[servo_id].initialized) return;
    servo_set_angle(servo_id, 0.0f);
}

void servo_center_all(void) {
    for (uint8_t i = 0; i < MAX_SERVOS; i++) {
        if (servos[i].initialized) {
            servo_center(i);
        }
    }
}
