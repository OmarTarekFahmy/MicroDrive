#include "gyro_servo.h"
#include "mpu6050.h"
#include "servo.h"
#include "pico/stdlib.h"
#include <stdint.h>

// Servo GPIO
#define SERVO_PIN 16

// Simple smoothing factor (0 < alpha <= 1)
// Lower value = smoother but slower response
// Higher value = faster but more jittery
#define SMOOTHING_ALPHA 0.15f

static float last_servo_angle = 90.0f; // start at neutral

void gyro_servo_init(uint32_t servo_gpio) {
    // Initialize MPU
    mpu6050_setup();

    // Initialize Servo
    servo_init(servo_gpio);

    // Move to neutral
    servo_set_angle(last_servo_angle);
}

void gyro_servo_update() {
    // Update gyro (reads both accelerometer and gyroscope)
    mpu6050_update();

    // Read X-axis rotation (tilt angle from accelerometer + gyro fusion)
    float angle_x = mpu6050_get_angle_x(); 

    // Map the tilt angle to servo angle
    // Typical tilt range is about -90 to +90 degrees
    // Map this to servo range 0 to 180 degrees
    float target_angle = angle_x + 90.0f;

    // Ensure the target angle is within servo range
    if (target_angle < 0) target_angle = 0;
    if (target_angle > 180) target_angle = 180;

    // Apply smoothing to reduce jitter
    last_servo_angle = last_servo_angle + SMOOTHING_ALPHA * (target_angle - last_servo_angle);

    // Move servo to match the tilt angle
    servo_set_angle(last_servo_angle);
}
