#ifndef SERVO_TEST_H
#define SERVO_TEST_H

#include <stdint.h>
#include <stdbool.h>

// Maximum number of servos supported
#define MAX_TEST_SERVOS 4

/**
 * @brief Initialize a servo on the given GPIO pin for testing
 * @param servo_id The servo ID (0-3)
 * @param gpio_pin The GPIO pin number connected to the servo signal wire
 */
void servo_test_init(uint8_t servo_id, uint32_t gpio_pin);

/**
 * @brief Set the pulse width directly in microseconds
 *        This is the key function for testing servo types:
 *        - Positional servo: will move to a specific angle and hold
 *        - Continuous servo: will rotate at a speed based on pulse width
 * 
 *        Standard pulse widths:
 *        - 500µs  = full CCW (or -90° for positional)
 *        - 1000µs = CCW (or -45° for positional)
 *        - 1500µs = center/stop
 *        - 2000µs = CW (or +45° for positional)
 *        - 2500µs = full CW (or +90° for positional)
 * 
 * @param servo_id The servo ID (0-3)
 * @param pulse_us Pulse width in microseconds (typically 500-2500)
 */
void servo_test_set_pulse_us(uint8_t servo_id, uint32_t pulse_us);

/**
 * @brief Set servo to center position (1500µs)
 *        For positional servos: moves to center
 *        For continuous servos: stops rotation
 * @param servo_id The servo ID (0-3)
 */
void servo_test_center(uint8_t servo_id);

/**
 * @brief Convenience function to set angle for positional servo testing
 *        Converts angle to pulse width internally
 * @param servo_id The servo ID (0-3)
 * @param angle Angle in degrees (-90 to +90)
 */
void servo_test_set_angle(uint8_t servo_id, float angle);

/**
 * @brief Get the current pulse width being sent
 * @param servo_id The servo ID (0-3)
 * @return Current pulse width in microseconds
 */
uint32_t servo_test_get_pulse_us(uint8_t servo_id);

/**
 * @brief Disable PWM output to servo (useful for manual servo adjustment)
 * @param servo_id The servo ID (0-3)
 */
void servo_test_disable(uint8_t servo_id);

/**
 * @brief Enable PWM output to servo
 * @param servo_id The servo ID (0-3)
 */
void servo_test_enable(uint8_t servo_id);

/**
 * @brief Print servo test information
 * @param servo_id The servo ID (0-3)
 */
void servo_test_print_info(uint8_t servo_id);

/**
 * @brief Configure servo for continuous rotation mode with angle tracking
 *        This mode assumes the servo is a continuous rotation servo.
 * @param servo_id The servo ID (0-3)
 * @param speed_dps Speed in degrees per second at maximum pulse (2500µs or 500µs)
 *                  Typical values: 60-180 dps depending on servo model
 */
void servo_test_set_continuous_mode(uint8_t servo_id, float speed_dps);

/**
 * @brief Move continuous servo to a target angle
 *        Calculates rotation time based on configured speed and current position.
 *        This function will block until the movement is complete.
 * @param servo_id The servo ID (0-3)
 * @param target_angle Target angle in degrees (can be any value, not limited to ±90)
 * @return true if movement completed successfully, false if servo not in continuous mode
 */
bool servo_test_move_continuous_angle(uint8_t servo_id, float target_angle);

/**
 * @brief Get current tracked angle for continuous servo
 * @param servo_id The servo ID (0-3)
 * @return Current angle in degrees
 */
float servo_test_get_continuous_angle(uint8_t servo_id);

/**
 * @brief Reset the current angle to zero (recalibrate position)
 * @param servo_id The servo ID (0-3)
 */
void servo_test_reset_continuous_angle(uint8_t servo_id);

#endif
