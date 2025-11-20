#ifndef GYRO_SERVO_H
#define GYRO_SERVO_H

#include <stdint.h>

// Initialize gyro + servo
void gyro_servo_init(uint32_t servo_gpio);

// Call this in a loop to update the servo from the gyro
void gyro_servo_update();

#endif
