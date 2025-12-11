#ifndef DC_MOTOR_H
#define DC_MOTOR_H

#include "pico/stdlib.h"


// High-level direction states for an H-bridge
typedef enum {
    DC_MOTOR_DIR_COAST = 0,  // IN1 = 0, IN2 = 0
    DC_MOTOR_DIR_FORWARD,    // IN1 = 1, IN2 = 0
    DC_MOTOR_DIR_REVERSE,    // IN1 = 0, IN2 = 1
    DC_MOTOR_DIR_BRAKE       // IN1 = 1, IN2 = 1 (fast brake)
} dc_motor_dir_t;

typedef struct {
    uint pin_in1;
    uint pin_in2;
    uint pin_pwm;       // EN / PWM pin

    uint pwm_slice;
    uint pwm_channel;
    uint16_t pwm_wrap;
} dc_motor_t;

/**
 * @brief Initialize the DC motor driver.
 *
 * @param motor Pointer to dc_motor_t struct.
 * @param pin_in1 H-bridge IN1 pin (GPIO).
 * @param pin_in2 H-bridge IN2 pin (GPIO).
 * @param pin_pwm H-bridge EN/PWM pin (must support PWM).
 * @param pwm_freq_hz Desired PWM frequency (e.g., 20000 for ~20 kHz).
 */
void dc_motor_init(dc_motor_t *motor,
                   uint pin_in1,
                   uint pin_in2,
                   uint pin_pwm,
                   uint32_t pwm_freq_hz);

/**
 * @brief Set motor direction only (speed unchanged).
 */
void dc_motor_set_direction(dc_motor_t *motor, dc_motor_dir_t dir);

/**
 * @brief Set PWM duty cycle only (0.0 to 1.0).
 *
 * @param duty 0.0 = 0%, 1.0 = 100%. Values are clamped.
 */
void dc_motor_set_duty(dc_motor_t *motor, float duty);

/**
 * @brief Convenience: set speed and direction with a single value.
 *
 * value in [-1.0, 1.0]
 *   >0  -> forward, |value| = speed
 *   <0  -> reverse, |value| = speed
 *   =0  -> coast (H-bridge inputs low)
 */
void dc_motor_set(dc_motor_t *motor, float value);

/**
 * @brief Put H-bridge into brake mode (both outputs high).
 */
void dc_motor_brake(dc_motor_t *motor);

/**
 * @brief Put H-bridge into coast mode (both outputs low).
 */
void dc_motor_coast(dc_motor_t *motor);

#endif // DC_MOTOR_H