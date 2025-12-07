#include "mpu6050.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <math.h>
#include <stdio.h>

#define MPU_ADDR 0x68
#define REG_PWR_MGMT_1 0x6B
#define REG_ACCEL_XOUT_H 0x3B
#define REG_GYRO_XOUT_H 0x43
#define REG_CONFIG 0x1A
#define REG_GYRO_CONFIG 0x1B
#define REG_ACCEL_CONFIG 0x1C

// Use I2C1 for MPU6050 (GPIO 2 = SDA, GPIO 3 = SCL)
#define MPU_I2C_PORT i2c1
#define MPU_SDA_PIN 2
#define MPU_SCL_PIN 3

// Number of samples for calibration
#define CALIBRATION_SAMPLES 500

static float angle_x = 0;
static float angle_y = 0;
static float angle_z = 0;

// Gyro and accel bias offsets
static float gyro_bias_x = 0;
static float gyro_bias_y = 0;
static float gyro_bias_z = 0;
static float accel_bias_x = 0;
static float accel_bias_y = 0;
static float accel_bias_z = 0;

// Sensitivity constants
static const float GYRO_SENS = 131.0f;   // LSB/(deg/sec) for ±250°/s
static const float ACCEL_SENS = 16384.0f; // LSB/g for ±2g
static const float dt = 0.05f;           // 50ms update interval

// Complementary filter coefficient (0.98 = 98% gyro, 2% accel)
static const float ALPHA = 0.96f;

// Dead zone threshold - ignore small gyro readings (reduced for more sensitivity)
static const float GYRO_DEADZONE = 0.3f;  // Lower value = more sensitive

// Z-axis specific settings (much higher dead zone to prevent drift)
static const float GYRO_DEADZONE_Z = 3.0f;  // Higher dead zone for Z to prevent random movement
static uint32_t stationary_count = 0;
static const uint32_t STATIONARY_THRESHOLD = 5; // 0.25 second at 50ms updates (faster)
static const float Z_DRIFT_CORRECTION = 0.85f;  // Very aggressive correction when stationary

// Track last Z reading to detect if drift is occurring
static float last_z_rate = 0;

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
    // Initialize I2C1 at 400kHz
    i2c_init(MPU_I2C_PORT, 400000);

    gpio_set_function(MPU_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(MPU_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(MPU_SDA_PIN);
    gpio_pull_up(MPU_SCL_PIN);

    sleep_ms(100);

    // Wake up MPU6050
    mpu_write(REG_PWR_MGMT_1, 0x00);
    sleep_ms(100);
    
    // Configure MPU6050
    // Set DLPF to 94Hz bandwidth (reduces noise)
    mpu_write(REG_CONFIG, 0x02);
    
    // Set gyro range to ±250°/s
    mpu_write(REG_GYRO_CONFIG, 0x00);
    
    // Set accel range to ±2g
    mpu_write(REG_ACCEL_CONFIG, 0x00);
    
    sleep_ms(100);

    // Calibrate sensor (KEEP SENSOR FLAT AND STILL!)
    printf("Calibrating MPU6050... keep sensor flat and still!\n");
    
    float sum_gx = 0, sum_gy = 0, sum_gz = 0;
    float sum_ax = 0, sum_ay = 0, sum_az = 0;
    int16_t gx, gy, gz;
    
    uint8_t accel_buffer[6];
    
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        // Read gyro
        mpu_read_gyro_raw(&gx, &gy, &gz);
        sum_gx += gx / GYRO_SENS;
        sum_gy += gy / GYRO_SENS;
        sum_gz += gz / GYRO_SENS;
        
        // Read accel
        mpu_read(REG_ACCEL_XOUT_H, accel_buffer, 6);
        int16_t ax = (accel_buffer[0] << 8) | accel_buffer[1];
        int16_t ay = (accel_buffer[2] << 8) | accel_buffer[3];
        int16_t az = (accel_buffer[4] << 8) | accel_buffer[5];
        
        sum_ax += ax / ACCEL_SENS;
        sum_ay += ay / ACCEL_SENS;
        sum_az += az / ACCEL_SENS;
        
        sleep_ms(2);
    }
    
    // Calculate average bias
    gyro_bias_x = sum_gx / CALIBRATION_SAMPLES;
    gyro_bias_y = sum_gy / CALIBRATION_SAMPLES;
    gyro_bias_z = sum_gz / CALIBRATION_SAMPLES;
    
    accel_bias_x = sum_ax / CALIBRATION_SAMPLES;
    accel_bias_y = sum_ay / CALIBRATION_SAMPLES;
    accel_bias_z = (sum_az / CALIBRATION_SAMPLES) - 1.0f; // Remove gravity (1g)
    
    printf("Calibration done!\n");
    printf("Gyro bias: X=%.3f Y=%.3f Z=%.3f deg/s\n", 
           gyro_bias_x, gyro_bias_y, gyro_bias_z);
    printf("Accel bias: X=%.3f Y=%.3f Z=%.3f g\n", 
           accel_bias_x, accel_bias_y, accel_bias_z);

    // Initialize angles to current position
    angle_x = angle_y = angle_z = 0;
    last_z_rate = 0;
    stationary_count = 0;
}

void mpu6050_update(void) {
    // Read accelerometer data with error checking
    uint8_t accel_buffer[6];
    int result = i2c_read_timeout_us(MPU_I2C_PORT, MPU_ADDR, accel_buffer, 6, false, 10000);
    if (result == PICO_ERROR_GENERIC || result == PICO_ERROR_TIMEOUT) {
        printf("I2C read error, reinitializing...\n");
        // Reinitialize I2C on error
        i2c_deinit(MPU_I2C_PORT);
        sleep_ms(10);
        i2c_init(MPU_I2C_PORT, 400000);
        gpio_set_function(MPU_SDA_PIN, GPIO_FUNC_I2C);
        gpio_set_function(MPU_SCL_PIN, GPIO_FUNC_I2C);
        gpio_pull_up(MPU_SDA_PIN);
        gpio_pull_up(MPU_SCL_PIN);
        sleep_ms(50);
        return;
    }
    
    int16_t ax_raw = (accel_buffer[0] << 8) | accel_buffer[1];
    int16_t ay_raw = (accel_buffer[2] << 8) | accel_buffer[3];
    int16_t az_raw = (accel_buffer[4] << 8) | accel_buffer[5];
    
    // Read gyroscope data
    int16_t gx_raw, gy_raw, gz_raw;
    mpu_read_gyro_raw(&gx_raw, &gy_raw, &gz_raw);

    // Convert and remove bias
    float ax_g = (ax_raw / ACCEL_SENS) - accel_bias_x;
    float ay_g = (ay_raw / ACCEL_SENS) - accel_bias_y;
    float az_g = (az_raw / ACCEL_SENS) - accel_bias_z;
    
    float gx_dps = (gx_raw / GYRO_SENS) - gyro_bias_x;
    float gy_dps = (gy_raw / GYRO_SENS) - gyro_bias_y;
    float gz_dps = (gz_raw / GYRO_SENS) - gyro_bias_z;
    
    // Apply dead zone to gyro
    if (fabsf(gx_dps) < GYRO_DEADZONE) gx_dps = 0;
    if (fabsf(gy_dps) < GYRO_DEADZONE) gy_dps = 0;
    if (fabsf(gz_dps) < GYRO_DEADZONE) gz_dps = 0;

    // Calculate tilt angles from accelerometer (in degrees)
    // Use atan2 for full range (-90 to +90 degrees)
    // Multiply by 2 to correct for sensor orientation/scaling
    // Negate X to fix rotation direction
    float accel_angle_x = -atan2f(ay_g, az_g) * 180.0f / M_PI * 2.0f;
    float accel_angle_y = atan2f(-ax_g, az_g) * 180.0f / M_PI * 2.0f;
    
    // Complementary filter for X and Y:
    // Combines gyro (smooth but drifts) with accel (accurate but noisy)
    // Formula: angle = ALPHA * (angle + gyro * dt) + (1 - ALPHA) * accel_angle
    
    angle_x = ALPHA * (angle_x + gx_dps * dt) + (1.0f - ALPHA) * accel_angle_x;
    angle_y = ALPHA * (angle_y + gy_dps * dt) + (1.0f - ALPHA) * accel_angle_y;
    
    // For Z-axis (yaw): Only gyro available (no accel reference)
    // Apply additional filtering to reduce noise accumulation
    
    // Apply Z-specific dead zone BEFORE filtering
    if (fabsf(gz_dps) < GYRO_DEADZONE_Z) {
        gz_dps = 0;
    }
    
    // Low-pass filter on gyro rate to smooth out noise
    float filtered_gz = 0.7f * gz_dps + 0.3f * last_z_rate;
    last_z_rate = filtered_gz;
    
    // Check if sensor is stationary (all gyro rates near zero)
    bool is_stationary = (fabsf(gx_dps) < GYRO_DEADZONE && 
                         fabsf(gy_dps) < GYRO_DEADZONE && 
                         fabsf(filtered_gz) < GYRO_DEADZONE_Z);
    
    if (is_stationary) {
        stationary_count++;
        // After being stationary for a while, aggressively correct Z drift toward 0
        if (stationary_count > STATIONARY_THRESHOLD) {
            angle_z *= Z_DRIFT_CORRECTION;
            // Snap to zero if close
            if (fabsf(angle_z) < 2.0f) {
                angle_z = 0.0f;
            }
        }
    } else {
        stationary_count = 0;
        // Only integrate if there's significant rotation (above dead zone)
        if (fabsf(filtered_gz) > GYRO_DEADZONE_Z) {
            angle_z += filtered_gz * dt;
        }
    }
    
    // Keep Z bounded between -180 and +180
    while (angle_z > 180.0f) angle_z -= 360.0f;
    while (angle_z < -180.0f) angle_z += 360.0f;
    
    // Clamp X and Y to reasonable ranges
    if (angle_x > 90.0f) angle_x = 90.0f;
    if (angle_x < -90.0f) angle_x = -90.0f;
    if (angle_y > 90.0f) angle_y = 90.0f;
    if (angle_y < -90.0f) angle_y = -90.0f;
}

float mpu6050_get_angle_x(void) {
    return angle_x*1.5;
}

float mpu6050_get_angle_y(void) {
    return angle_y *1.5;
}

float mpu6050_get_angle_z(void) {
    return angle_z;
}

void mpu6050_reset_angles(void) {
    angle_x = 0;
    angle_y = 0;
    angle_z = 0;
}
