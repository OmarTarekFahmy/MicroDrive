#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include <stdbool.h>

// Maximum number of servos supported
#define MAX_SERVOS 3

// Servo IDs
#define SERVO_X 0
#define SERVO_Y 1
#define SERVO_Z 2

// Servo types
#define SERVO_TYPE_180 0  // Standard 180-degree positional servo
#define SERVO_TYPE_360 1  // 360-degree continuous rotation servo

// Speed for 360° continuous rotation servos (degrees per second)
// Measure this by timing a full rotation and calculate: 360 / time_seconds
#define SERVO_360_SPEED_DPS 90.0f  // Adjust based on your servo speed

/**
 * @brief Initialize a servo on the given GPIO pin
 * @param servo_id The servo ID (0, 1, or 2)
 * @param gpio_pin The GPIO pin number connected to the servo signal wire
 * @param servo_type SERVO_TYPE_180 or SERVO_TYPE_360
 */
void servo_init(uint8_t servo_id, uint32_t gpio_pin, uint8_t servo_type);

/**
 * @brief Set the angle for a servo
 *        For 180° servo: direct position control
 *        For 360° servo: rotates by speed*time to reach target angle
 * @param servo_id The servo ID (0, 1, or 2)
 * @param angle The target angle (-90 to +90 degrees)
 */
void servo_set_angle(uint8_t servo_id, float angle);

/**
 * @brief Get the current angle of a servo
 * @param servo_id The servo ID (0, 1, or 2)
 * @return The current angle in degrees
 */
float servo_get_angle(uint8_t servo_id);

/**
 * @brief Set servo to neutral/center position (stop for 360°)
 * @param servo_id The servo ID (0, 1, or 2)
 */
void servo_center(uint8_t servo_id);

/**
 * @brief Stop a 360° servo (set to center pulse)
 * @param servo_id The servo ID
 */
void servo_stop(uint8_t servo_id);

/**
 * @brief Center all servos
 */
void servo_center_all(void);

#endif
