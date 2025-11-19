/**
 * @file gyroscope.c
 * @brief MPU6050 Gyroscope/Accelerometer driver implementation
 */

#include "gyroscope.h"
#include <math.h>
#include <string.h>

// Static variables for gyroscope state
static i2c_inst_t *gyro_i2c = NULL;
static int16_t accel_offset_x = 0;
static int16_t accel_offset_y = 0;
static int16_t accel_offset_z = 0;

// Complementary filter coefficient (0-1, higher = more accelerometer)
#define ALPHA 0.98f

// Previous angles for complementary filter
static float prev_pitch = 0.0f;
static float prev_roll = 0.0f;
static absolute_time_t prev_time;

/**
 * @brief Write a byte to MPU6050 register
 */
static bool mpu6050_write_register(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    int result = i2c_write_blocking(gyro_i2c, MPU6050_ADDR, data, 2, false);
    return result == 2;
}

/**
 * @brief Read bytes from MPU6050 register
 */
static bool mpu6050_read_registers(uint8_t reg, uint8_t *buffer, uint8_t len) {
    int result = i2c_write_blocking(gyro_i2c, MPU6050_ADDR, &reg, 1, true);
    if (result != 1) return false;
    
    result = i2c_read_blocking(gyro_i2c, MPU6050_ADDR, buffer, len, false);
    return result == len;
}

bool gyroscope_init(gyroscope_config_t *config) {
    if (config == NULL) return false;
    
    // Store I2C instance
    gyro_i2c = config->i2c_port;
    
    // Initialize I2C pins
    gpio_set_function(config->sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(config->scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(config->sda_pin);
    gpio_pull_up(config->scl_pin);
    
    // Initialize I2C
    i2c_init(gyro_i2c, config->baudrate);
    
    // Wait for sensor to stabilize
    sleep_ms(100);
    
    // Check WHO_AM_I register
    uint8_t who_am_i;
    if (!mpu6050_read_registers(MPU6050_REG_WHO_AM_I, &who_am_i, 1)) {
        return false;
    }
    
    if (who_am_i != 0x68) {
        return false;  // Wrong device ID
    }
    
    // Wake up MPU6050 (reset sleep bit)
    if (!mpu6050_write_register(MPU6050_REG_PWR_MGMT_1, 0x00)) {
        return false;
    }
    
    sleep_ms(100);
    
    // Initialize time tracking for complementary filter
    prev_time = get_absolute_time();
    prev_pitch = 0.0f;
    prev_roll = 0.0f;
    
    return true;
}

bool gyroscope_read_accel(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z) {
    uint8_t buffer[6];
    
    if (!mpu6050_read_registers(MPU6050_REG_ACCEL_XOUT_H, buffer, 6)) {
        return false;
    }
    
    *accel_x = (int16_t)((buffer[0] << 8) | buffer[1]) - accel_offset_x;
    *accel_y = (int16_t)((buffer[2] << 8) | buffer[3]) - accel_offset_y;
    *accel_z = (int16_t)((buffer[4] << 8) | buffer[5]) - accel_offset_z;
    
    return true;
}

bool gyroscope_read_gyro(int16_t *gyro_x, int16_t *gyro_y, int16_t *gyro_z) {
    uint8_t buffer[6];
    
    if (!mpu6050_read_registers(MPU6050_REG_GYRO_XOUT_H, buffer, 6)) {
        return false;
    }
    
    *gyro_x = (int16_t)((buffer[0] << 8) | buffer[1]);
    *gyro_y = (int16_t)((buffer[2] << 8) | buffer[3]);
    *gyro_z = (int16_t)((buffer[4] << 8) | buffer[5]);
    
    return true;
}

bool gyroscope_get_angles(gyroscope_angles_t *angles) {
    if (angles == NULL) return false;
    
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
    
    // Read sensor data
    if (!gyroscope_read_accel(&accel_x, &accel_y, &accel_z)) {
        return false;
    }
    
    if (!gyroscope_read_gyro(&gyro_x, &gyro_y, &gyro_z)) {
        return false;
    }
    
    // Calculate time delta
    absolute_time_t current_time = get_absolute_time();
    float dt = absolute_time_diff_us(prev_time, current_time) / 1000000.0f;
    prev_time = current_time;
    
    // Calculate angles from accelerometer (in degrees)
    float accel_pitch = atan2f((float)accel_y, sqrtf((float)accel_x * accel_x + (float)accel_z * accel_z)) * 180.0f / M_PI;
    float accel_roll = atan2f(-(float)accel_x, sqrtf((float)accel_y * accel_y + (float)accel_z * accel_z)) * 180.0f / M_PI;
    
    // Gyroscope angular velocity to degrees/sec (sensitivity: 131 LSB/°/s for ±250°/s range)
    float gyro_pitch_rate = gyro_x / 131.0f;
    float gyro_roll_rate = gyro_y / 131.0f;
    float gyro_yaw_rate = gyro_z / 131.0f;
    
    // Complementary filter: combine accelerometer and gyroscope
    angles->pitch = ALPHA * (prev_pitch + gyro_pitch_rate * dt) + (1.0f - ALPHA) * accel_pitch;
    angles->roll = ALPHA * (prev_roll + gyro_roll_rate * dt) + (1.0f - ALPHA) * accel_roll;
    angles->yaw = prev_roll + gyro_yaw_rate * dt;  // Yaw can only be calculated from gyro
    
    // Store for next iteration
    prev_pitch = angles->pitch;
    prev_roll = angles->roll;
    
    return true;
}

bool gyroscope_calibrate(uint16_t samples) {
    if (samples == 0) return false;
    
    int32_t sum_x = 0, sum_y = 0, sum_z = 0;
    int16_t accel_x, accel_y, accel_z;
    
    // Temporarily disable offset
    accel_offset_x = 0;
    accel_offset_y = 0;
    accel_offset_z = 0;
    
    // Collect samples
    for (uint16_t i = 0; i < samples; i++) {
        if (!gyroscope_read_accel(&accel_x, &accel_y, &accel_z)) {
            return false;
        }
        
        sum_x += accel_x;
        sum_y += accel_y;
        sum_z += accel_z;
        
        sleep_ms(10);
    }
    
    // Calculate average offsets
    accel_offset_x = sum_x / samples;
    accel_offset_y = sum_y / samples;
    accel_offset_z = (sum_z / samples) - 16384;  // Remove gravity (1g = 16384 at ±2g scale)
    
    return true;
}
