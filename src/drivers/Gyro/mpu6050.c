#include "mpu6050.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <math.h>
#include <stdio.h>

#define MPU_ADDR 0x68
#define REG_PWR_MGMT_1 0x6B
#define REG_ACCEL_XOUT_H 0x3B
#define REG_GYRO_XOUT_H 0x43

// Use I2C1 for MPU6050 (GPIO 2 = SDA, GPIO 3 = SCL)
#define MPU_I2C_PORT i2c1
#define MPU_SDA_PIN 2
#define MPU_SCL_PIN 3

// Number of samples for calibration
#define CALIBRATION_SAMPLES 200

static float angle_x = 0;
static float angle_y = 0;
static float angle_z = 0;

// Gyro bias offsets (calculated during calibration)
static float gyro_bias_x = 0;
static float gyro_bias_y = 0;
static float gyro_bias_z = 0;

static const float GYRO_SENS = 131.0f;  // LSB/(deg/sec) for ±250°/s
static const float ACCEL_SENS = 16384.0f; // LSB/g for ±2g
static const float dt = 0.05f;          // 50ms loop time (match your update interval)

// Dead zone threshold - ignore small gyro readings (noise)
static const float GYRO_DEADZONE = 2.0f;  // degrees per second

// Write to register
static void mpu_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(MPU_I2C_PORT, MPU_ADDR, buf, 2, false);
}

// Read bytes from a register
static void mpu_read(uint8_t reg, uint8_t *buf, uint8_t len) {
    i2c_write_blocking(MPU_I2C_PORT, MPU_ADDR, &reg, 1, true);
    i2c_read_blocking(MPU_I2C_PORT, MPU_ADDR, buf, len, false);
}

// Read raw gyro values
static void mpu_read_gyro_raw(int16_t *gx, int16_t *gy, int16_t *gz) {
    uint8_t gyro_buffer[6];
    mpu_read(REG_GYRO_XOUT_H, gyro_buffer, 6);
    *gx = (gyro_buffer[0] << 8) | gyro_buffer[1];
    *gy = (gyro_buffer[2] << 8) | gyro_buffer[3];
    *gz = (gyro_buffer[4] << 8) | gyro_buffer[5];
}

void mpu6050_setup(void) {
    // --- Initialize I2C1 at 400kHz ---
    i2c_init(MPU_I2C_PORT, 400000);

    gpio_set_function(MPU_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(MPU_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(MPU_SDA_PIN);
    gpio_pull_up(MPU_SCL_PIN);

    sleep_ms(100);

    // --- Wake up MPU6050 ---
    mpu_write(REG_PWR_MGMT_1, 0x00);
    sleep_ms(100);

    // --- Calibrate gyro (KEEP SENSOR STATIONARY!) ---
    printf("Calibrating gyro... keep sensor still!\n");
    
    float sum_x = 0, sum_y = 0, sum_z = 0;
    int16_t gx, gy, gz;
    
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        mpu_read_gyro_raw(&gx, &gy, &gz);
        sum_x += gx / GYRO_SENS;
        sum_y += gy / GYRO_SENS;
        sum_z += gz / GYRO_SENS;
        sleep_ms(5);
    }
    
    // Calculate average bias
    gyro_bias_x = sum_x / CALIBRATION_SAMPLES;
    gyro_bias_y = sum_y / CALIBRATION_SAMPLES;
    gyro_bias_z = sum_z / CALIBRATION_SAMPLES;
    
    printf("Gyro calibration done. Bias: X=%.3f Y=%.3f Z=%.3f deg/s\n", 
           gyro_bias_x, gyro_bias_y, gyro_bias_z);

    angle_x = angle_y = angle_z = 0;
}

void mpu6050_update(void) {
    // Read accelerometer data
    uint8_t accel_buffer[6];
    mpu_read(REG_ACCEL_XOUT_H, accel_buffer, 6);
    
    int16_t ax = (accel_buffer[0] << 8) | accel_buffer[1];
    int16_t ay = (accel_buffer[2] << 8) | accel_buffer[3];
    int16_t az = (accel_buffer[4] << 8) | accel_buffer[5];
    
    // Read gyroscope data
    int16_t gx, gy, gz;
    mpu_read_gyro_raw(&gx, &gy, &gz);

    // Convert gyroscope to degrees per second and remove bias
    float gx_dps = (gx / GYRO_SENS) - gyro_bias_x;
    float gy_dps = (gy / GYRO_SENS) - gyro_bias_y;
    float gz_dps = (gz / GYRO_SENS) - gyro_bias_z;
    
    // Apply dead zone to reduce drift when stationary
    if (fabsf(gx_dps) < GYRO_DEADZONE) gx_dps = 0;
    if (fabsf(gy_dps) < GYRO_DEADZONE) gy_dps = 0;
    if (fabsf(gz_dps) < GYRO_DEADZONE) gz_dps = 0;

    // Convert accelerometer to g
    float ax_g = ax / ACCEL_SENS;
    float ay_g = ay / ACCEL_SENS;
    float az_g = az / ACCEL_SENS;
    
    // Calculate angles from accelerometer (in degrees)
    float accel_angle_x = atan2f(ay_g, sqrtf(ax_g * ax_g + az_g * az_g)) * 180.0f / M_PI;
    float accel_angle_y = atan2f(-ax_g, sqrtf(ay_g * ay_g + az_g * az_g)) * 180.0f / M_PI;
    
    // For X and Y: Use accelerometer directly (no drift)
    // Only use gyro for fast movements
    angle_x = accel_angle_x;
    angle_y = accel_angle_y;
    
    // Z-axis: only integrate if there's significant rotation
    angle_z += gz_dps * dt;
}

float mpu6050_get_angle_x(void) {
    return angle_x;
}

float mpu6050_get_angle_y(void) {
    return angle_y;
}

float mpu6050_get_angle_z(void) {
    return angle_z;
}

void mpu6050_reset_angles(void) {
    angle_x = 0;
    angle_y = 0;
    angle_z = 0;
}
