#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/i2c.h"

// MPU6050 I2C address
#define MPU6050_ADDR 0x68

// Register addresses
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_GYRO_CONFIG 0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_ACCEL_XOUT_H 0x3B

// Sensor scaling factors (for raw -> physical units)
#define MPU6050_GYRO_SCALE 131.0f    // LSB/(deg/s) for ±250°/s
#define MPU6050_ACCEL_SCALE 16384.0f // LSB/g for ±2g

// Complementary filter weight
#define MPU6050_ALPHA 0.98f

// Public API

// Initialise I2C, wake and configure MPU6050, and initialise angle state.
// Uses I2C1 on GPIO2 (SDA) and GPIO3 (SCL).
void mpu6050_init(void);

// Read sensor, integrate gyro, apply complementary filter and update
// internal pitch/roll/yaw state. Call regularly (e.g. 50–200 Hz).
void mpu6050_update(void);

// Set the current orientation as the zero reference.
// Call once after ~1s of stillness.
void mpu6050_set_reference(void);

// Calibrate gyro bias. Call while the sensor is completely still
// (typically once after init, before setting the reference).
void mpu6050_calibrate_gyro(void);

// Get change in angles (Δroll, Δpitch, Δyaw) in degrees relative
// to the reference set by mpu6050_set_reference(). Any pointer may be NULL.
void mpu6050_get_deltas(float *dRoll, float *dPitch, float *dYaw);

// Optional helpers
float mpu6050_get_temperature_c(void);
void mpu6050_get_raw_accel(int16_t *ax, int16_t *ay, int16_t *az);
void mpu6050_get_raw_gyro(int16_t *gx, int16_t *gy, int16_t *gz);

#endif // MPU6050_H
