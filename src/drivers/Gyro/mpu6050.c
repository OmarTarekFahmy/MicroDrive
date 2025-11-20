#include "mpu6050.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <math.h>

#define MPU_ADDR 0x68
#define REG_PWR_MGMT_1 0x6B
#define REG_ACCEL_XOUT_H 0x3B
#define REG_GYRO_XOUT_H 0x43

static float angle_x = 0;
static float angle_y = 0;
static float angle_z = 0;

static const float GYRO_SENS = 131.0f;  // LSB/(deg/sec) for ±250°/s
static const float ACCEL_SENS = 16384.0f; // LSB/g for ±2g
static const float dt = 0.01f;          // 10ms loop time
static const float ALPHA = 0.98f;       // Complementary filter coefficient

// Write to register
static void mpu_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(i2c0, MPU_ADDR, buf, 2, false);
}

// Read bytes from a register
static void mpu_read(uint8_t reg, uint8_t *buf, uint8_t len) {
    i2c_write_blocking(i2c0, MPU_ADDR, &reg, 1, true);
    i2c_read_blocking(i2c0, MPU_ADDR, buf, len, false);
}

void mpu6050_setup(void) {
    // --- Initialize I2C0 at 400kHz ---
    i2c_init(i2c0, 400000);

    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);

    sleep_ms(100);

    // --- Wake up MPU6050 ---
    mpu_write(REG_PWR_MGMT_1, 0x00);

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
    uint8_t gyro_buffer[6];
    mpu_read(REG_GYRO_XOUT_H, gyro_buffer, 6);

    int16_t gx = (gyro_buffer[0] << 8) | gyro_buffer[1];
    int16_t gy = (gyro_buffer[2] << 8) | gyro_buffer[3];
    int16_t gz = (gyro_buffer[4] << 8) | gyro_buffer[5];

    // Convert gyroscope to degrees per second
    float gx_dps = gx / GYRO_SENS;
    float gy_dps = gy / GYRO_SENS;
    float gz_dps = gz / GYRO_SENS;

    // Convert accelerometer to g
    float ax_g = ax / ACCEL_SENS;
    float ay_g = ay / ACCEL_SENS;
    float az_g = az / ACCEL_SENS;
    
    // Calculate angles from accelerometer (in degrees)
    float accel_angle_x = atan2(ay_g, sqrt(ax_g * ax_g + az_g * az_g)) * 180.0f / M_PI;
    float accel_angle_y = atan2(-ax_g, sqrt(ay_g * ay_g + az_g * az_g)) * 180.0f / M_PI;
    
    // Complementary filter: combine gyro integration with accelerometer
    // This reduces drift while maintaining responsiveness
    angle_x = ALPHA * (angle_x + gx_dps * dt) + (1.0f - ALPHA) * accel_angle_x;
    angle_y = ALPHA * (angle_y + gy_dps * dt) + (1.0f - ALPHA) * accel_angle_y;
    angle_z += gz_dps * dt; // Z-axis has no accelerometer correction
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
