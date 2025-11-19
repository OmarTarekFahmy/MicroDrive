/**
 * @file servo.c
 * @brief SG90 Servo Motor driver implementation
 */

#include "servo.h"
#include <math.h>

// PWM wrap value for 50Hz (20ms period)
// System clock is 125MHz, we'll use a divider
#define PWM_CLOCK_DIV 64.0f
#define PWM_WRAP 39062  // (125MHz / 64 / 50Hz) - 1

/**
 * @brief Convert angle to PWM duty cycle
 */
static uint16_t angle_to_duty(float angle) {
    // Clamp angle to valid range
    if (angle < 0.0f) angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;
    
    // Map angle (0-180) to pulse width (500-2500 us)
    uint16_t pulse_us = SERVO_MIN_PULSE_US + 
                        (uint16_t)((angle / 180.0f) * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US));
    
    // Convert pulse width to duty cycle count
    // Period = 20000us, pulse_us is our on-time
    // duty = (pulse_us / 20000) * PWM_WRAP
    uint16_t duty = (uint16_t)((pulse_us * PWM_WRAP) / 20000);
    
    return duty;
}

/**
 * @brief Convert pulse width to duty cycle
 */
static uint16_t pulse_to_duty(uint16_t pulse_us) {
    // Clamp pulse width
    if (pulse_us < SERVO_MIN_PULSE_US) pulse_us = SERVO_MIN_PULSE_US;
    if (pulse_us > SERVO_MAX_PULSE_US) pulse_us = SERVO_MAX_PULSE_US;
    
    uint16_t duty = (uint16_t)((pulse_us * PWM_WRAP) / 20000);
    return duty;
}

bool servo_init(servo_t *servo, uint gpio_pin) {
    if (servo == NULL) return false;
    
    // Store GPIO pin
    servo->gpio_pin = gpio_pin;
    servo->current_angle = 90.0f;  // Start at center position
    
    // Set GPIO function to PWM
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    
    // Get PWM slice and channel for this GPIO
    servo->slice_num = pwm_gpio_to_slice_num(gpio_pin);
    servo->channel = pwm_gpio_to_channel(gpio_pin);
    
    // Configure PWM
    pwm_config config = pwm_get_default_config();
    
    // Set clock divider for 50Hz operation
    pwm_config_set_clkdiv(&config, PWM_CLOCK_DIV);
    
    // Set wrap value (period)
    pwm_config_set_wrap(&config, PWM_WRAP);
    
    // Initialize PWM with config
    pwm_init(servo->slice_num, &config, true);
    
    // Set initial position to 90 degrees (center)
    servo_set_angle(servo, 90.0f);
    
    return true;
}

bool servo_set_angle(servo_t *servo, float angle) {
    if (servo == NULL) return false;
    
    // Clamp angle
    if (angle < 0.0f) angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;
    
    // Convert angle to duty cycle
    uint16_t duty = angle_to_duty(angle);
    
    // Set PWM duty cycle
    pwm_set_chan_level(servo->slice_num, servo->channel, duty);
    
    // Update current angle
    servo->current_angle = angle;
    
    return true;
}

bool servo_set_pulse_us(servo_t *servo, uint16_t pulse_us) {
    if (servo == NULL) return false;
    
    // Convert pulse width to duty cycle
    uint16_t duty = pulse_to_duty(pulse_us);
    
    // Set PWM duty cycle
    pwm_set_chan_level(servo->slice_num, servo->channel, duty);
    
    // Calculate and update angle
    servo->current_angle = ((float)(pulse_us - SERVO_MIN_PULSE_US) / 
                            (float)(SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) * 180.0f;
    
    return true;
}

float servo_get_angle(servo_t *servo) {
    if (servo == NULL) return 0.0f;
    return servo->current_angle;
}

void servo_disable(servo_t *servo) {
    if (servo == NULL) return;
    pwm_set_enabled(servo->slice_num, false);
}

void servo_enable(servo_t *servo) {
    if (servo == NULL) return;
    pwm_set_enabled(servo->slice_num, true);
}

bool servo_sweep(servo_t *servo, float target_angle, float speed, uint32_t delay_ms) {
    if (servo == NULL) return false;
    
    // Clamp target angle
    if (target_angle < 0.0f) target_angle = 0.0f;
    if (target_angle > 180.0f) target_angle = 180.0f;
    
    float current = servo->current_angle;
    
    if (current < target_angle) {
        // Sweep up
        while (current < target_angle) {
            current += speed;
            if (current > target_angle) current = target_angle;
            servo_set_angle(servo, current);
            sleep_ms(delay_ms);
        }
    } else {
        // Sweep down
        while (current > target_angle) {
            current -= speed;
            if (current < target_angle) current = target_angle;
            servo_set_angle(servo, current);
            sleep_ms(delay_ms);
        }
    }
    
    return true;
}
