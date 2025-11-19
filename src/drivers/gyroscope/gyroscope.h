/**
 * @file gyroscope.h
 * @brief MPU6050 Gyroscope/Accelerometer driver for RP2040
 * 
 * This driver provides functions to interface with the MPU6050 sensor
 * via I2C and retrieve angle measurements.
 */

#ifndef GYROSCOPE_H
#define GYROSCOPE_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdbool.h>

// MPU6050 I2C Address
#define MPU6050_ADDR 0x68

// MPU6050 Register Addresses
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_GYRO_XOUT_H 0x43
#define MPU6050_REG_WHO_AM_I 0x75

// Gyroscope Configuration
typedef struct {
    i2c_inst_t *i2c_port;  // I2C port (i2c0 or i2c1)
    uint8_t sda_pin;       // SDA pin number
    uint8_t scl_pin;       // SCL pin number
    uint32_t baudrate;     // I2C baudrate (typically 400000)
} gyroscope_config_t;

// Angle data structure
typedef struct {
    float pitch;  // Rotation around X-axis (degrees)
    float roll;   // Rotation around Y-axis (degrees)
    float yaw;    // Rotation around Z-axis (degrees)
} gyroscope_angles_t;

/**
 * @brief Initialize the MPU6050 gyroscope
 * 
 * @param config Configuration structure containing I2C settings
 * @return true if initialization successful, false otherwise
 */
bool gyroscope_init(gyroscope_config_t *config);

/**
 * @brief Get current angles from the gyroscope
 * 
 * @param angles Pointer to structure to store calculated angles
 * @return true if read successful, false otherwise
 */
bool gyroscope_get_angles(gyroscope_angles_t *angles);

/**
 * @brief Read raw accelerometer data
 * 
 * @param accel_x X-axis acceleration
 * @param accel_y Y-axis acceleration
 * @param accel_z Z-axis acceleration
 * @return true if read successful, false otherwise
 */
bool gyroscope_read_accel(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z);

/**
 * @brief Read raw gyroscope data
 * 
 * @param gyro_x X-axis angular velocity
 * @param gyro_y Y-axis angular velocity
 * @param gyro_z Z-axis angular velocity
 * @return true if read successful, false otherwise
 */
bool gyroscope_read_gyro(int16_t *gyro_x, int16_t *gyro_y, int16_t *gyro_z);

/**
 * @brief Calibrate the gyroscope (removes offset)
 * 
 * @param samples Number of samples to use for calibration
 * @return true if calibration successful, false otherwise
 */
bool gyroscope_calibrate(uint16_t samples);

#endif // GYROSCOPE_H
