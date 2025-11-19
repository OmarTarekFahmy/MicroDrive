/**
 * @file servo.h
 * @brief SG90 Servo Motor driver for RP2040
 * 
 * This driver provides PWM-based control for SG90 servo motors
 * with 360-degree rotation capability.
 */

#ifndef SERVO_H
#define SERVO_H

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <stdbool.h>

// SG90 Servo specifications
#define SERVO_MIN_PULSE_US 500   // Minimum pulse width (microseconds)
#define SERVO_MAX_PULSE_US 2500  // Maximum pulse width (microseconds)
#define SERVO_PWM_FREQ 50        // PWM frequency in Hz (50Hz = 20ms period)

// Servo configuration structure
typedef struct {
    uint gpio_pin;        // GPIO pin connected to servo signal
    uint slice_num;       // PWM slice number
    uint channel;         // PWM channel (A or B)
    float current_angle;  // Current angle of servo
} servo_t;

/**
 * @brief Initialize a servo motor on specified GPIO pin
 * 
 * @param servo Pointer to servo structure
 * @param gpio_pin GPIO pin number for servo signal
 * @return true if initialization successful, false otherwise
 */
bool servo_init(servo_t *servo, uint gpio_pin);

/**
 * @brief Set servo to specific angle (0-180 degrees)
 * 
 * @param servo Pointer to servo structure
 * @param angle Desired angle (0-180 degrees)
 * @return true if successful, false otherwise
 */
bool servo_set_angle(servo_t *servo, float angle);

/**
 * @brief Set servo pulse width directly in microseconds
 * 
 * @param servo Pointer to servo structure
 * @param pulse_us Pulse width in microseconds (500-2500)
 * @return true if successful, false otherwise
 */
bool servo_set_pulse_us(servo_t *servo, uint16_t pulse_us);

/**
 * @brief Get current angle of servo
 * 
 * @param servo Pointer to servo structure
 * @return Current angle in degrees
 */
float servo_get_angle(servo_t *servo);

/**
 * @brief Disable servo (stops PWM signal)
 * 
 * @param servo Pointer to servo structure
 */
void servo_disable(servo_t *servo);

/**
 * @brief Enable servo (resumes PWM signal)
 * 
 * @param servo Pointer to servo structure
 */
void servo_enable(servo_t *servo);

/**
 * @brief Sweep servo smoothly from current angle to target angle
 * 
 * @param servo Pointer to servo structure
 * @param target_angle Target angle (0-180 degrees)
 * @param speed Speed in degrees per step
 * @param delay_ms Delay between steps in milliseconds
 * @return true if successful, false otherwise
 */
bool servo_sweep(servo_t *servo, float target_angle, float speed, uint32_t delay_ms);

#endif // SERVO_H
