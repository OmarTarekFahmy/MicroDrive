/**
 * @file yassinMpu.h
 * @brief MPU6050 Gyroscope/Accelerometer Driver for Raspberry Pi Pico
 * 
 * Provides initialization, calibration, and orientation (yaw/pitch/roll)
 * output using a complementary filter.
 */

#ifndef YASSIN_MPU_H
#define YASSIN_MPU_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

// I2C Configuration (GPIO 2 = SDA, GPIO 3 = SCL on I2C1)
#define MPU_I2C_PORT        i2c1
#define MPU_SDA_PIN         2
#define MPU_SCL_PIN         3
#define MPU_I2C_FREQ        400000  // 400kHz

// MPU6050 I2C Address (0x68 if AD0=GND, 0x69 if AD0=VCC)
#define MPU6050_ADDR        0x68

// MPU6050 Register Addresses
#define MPU6050_REG_SMPLRT_DIV      0x19
#define MPU6050_REG_CONFIG          0x1A
#define MPU6050_REG_GYRO_CONFIG     0x1B
#define MPU6050_REG_ACCEL_CONFIG    0x1C
#define MPU6050_REG_ACCEL_XOUT_H    0x3B
#define MPU6050_REG_TEMP_OUT_H      0x41
#define MPU6050_REG_GYRO_XOUT_H     0x43
#define MPU6050_REG_PWR_MGMT_1      0x6B
#define MPU6050_REG_PWR_MGMT_2      0x6C
#define MPU6050_REG_WHO_AM_I        0x75

// Sensitivity scale factors
#define GYRO_SCALE_250DPS   131.0f   // LSB/(°/s) for ±250°/s
#define ACCEL_SCALE_2G      16384.0f // LSB/g for ±2g

// Complementary filter coefficient (0.98 = trust gyro 98%, accel 2%)
#define COMPLEMENTARY_ALPHA 0.98f

// Calibration samples (more samples = better offset estimation)
#define CALIBRATION_SAMPLES 2000

// ============================================================================
// DATA TYPES
// ============================================================================

/**
 * @brief Raw sensor data from MPU6050
 */
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temp;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} MPU6050_RawData;

/**
 * @brief Calibration offsets for gyroscope and accelerometer
 */
typedef struct {
    float gyro_x_offset;
    float gyro_y_offset;
    float gyro_z_offset;
    float accel_x_offset;
    float accel_y_offset;
    float accel_z_offset;  // Note: should be ~0 when flat (gravity on Z)
} MPU6050_Calibration;

/**
 * @brief Orientation angles in degrees
 */
typedef struct {
    float roll;   // Rotation around X-axis (-180 to +180)
    float pitch;  // Rotation around Y-axis (-90 to +90)
    float yaw;    // Rotation around Z-axis (accumulates, drifts over time)
} MPU6050_Orientation;

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Initialize I2C and MPU6050 sensor
 * @return true if initialization successful, false if sensor not found
 */
bool mpu6050_init(void);

/**
 * @brief Check if MPU6050 is connected and responding
 * @return true if WHO_AM_I register returns expected value
 */
bool mpu6050_is_connected(void);

/**
 * @brief Calibrate the gyroscope and accelerometer offsets
 *        IMPORTANT: Keep the sensor flat and completely still during calibration!
 * @param num_samples Number of samples to average (default: CALIBRATION_SAMPLES)
 */
void mpu6050_calibrate(uint16_t num_samples);

/**
 * @brief Read raw sensor data from MPU6050
 * @param data Pointer to store raw accelerometer, temperature, and gyroscope data
 */
void mpu6050_read_raw(MPU6050_RawData *data);

/**
 * @brief Update orientation using complementary filter
 *        Call this regularly (e.g., every 10-20ms) for accurate tracking
 * @param dt Time delta since last update in seconds
 */
void mpu6050_update(float dt);

/**
 * @brief Get current orientation (roll, pitch, yaw)
 * @param orientation Pointer to store orientation angles in degrees
 */
void mpu6050_get_orientation(MPU6050_Orientation *orientation);

/**
 * @brief Get individual orientation angles (convenience functions)
 */
float mpu6050_get_roll(void);
float mpu6050_get_pitch(void);
float mpu6050_get_yaw(void);

/**
 * @brief Reset orientation angles to zero
 */
void mpu6050_reset_orientation(void);

/**
 * @brief Reset only the yaw angle to zero (useful since yaw drifts)
 */
void mpu6050_reset_yaw(void);

/**
 * @brief Get temperature from MPU6050 in degrees Celsius
 * @return Temperature in °C
 */
float mpu6050_get_temperature(void);

/**
 * @brief Get current calibration offsets
 * @param cal Pointer to store calibration data
 */
void mpu6050_get_calibration(MPU6050_Calibration *cal);

#endif // YASSIN_MPU_H
