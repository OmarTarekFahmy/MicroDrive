/**
 * @file yassinMpu.c
 * @brief MPU6050 Gyroscope/Accelerometer Driver Implementation
 * 
 * Implements initialization, calibration, and orientation tracking
 * using a complementary filter for accurate yaw/pitch/roll output.
 */

#include "yassinMpu.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ============================================================================
// PRIVATE VARIABLES
// ============================================================================

// Current orientation angles (degrees)
static float roll_angle = 0.0f;
static float pitch_angle = 0.0f;
static float yaw_angle = 0.0f;

// Calibration offsets
static MPU6050_Calibration calibration = {0};

// Flag to track if sensor is calibrated
static bool is_calibrated = false;

// ============================================================================
// PRIVATE HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Write a single byte to an MPU6050 register
 */
static int mpu_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    int ret = i2c_write_blocking(MPU_I2C_PORT, MPU6050_ADDR, buf, 2, false);
    return ret;
}

/**
 * @brief Read bytes from MPU6050 starting at given register
 */
static int mpu_read_reg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    int ret = i2c_write_blocking(MPU_I2C_PORT, MPU6050_ADDR, &reg, 1, true);
    if (ret < 0) return ret;
    
    ret = i2c_read_blocking(MPU_I2C_PORT, MPU6050_ADDR, buf, len, false);
    return ret;
}

/**
 * @brief Convert raw 16-bit value from big-endian buffer
 */
static int16_t buf_to_int16(uint8_t *buf)
{
    return (int16_t)((buf[0] << 8) | buf[1]);
}

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

bool mpu6050_init(void)
{
    printf("[MPU6050] Initializing...\n");
    
    // Initialize I2C peripheral
    i2c_init(MPU_I2C_PORT, MPU_I2C_FREQ);
    
    // Configure GPIO pins for I2C
    gpio_set_function(MPU_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(MPU_SCL_PIN, GPIO_FUNC_I2C);
    
    // Enable internal pull-ups
    gpio_pull_up(MPU_SDA_PIN);
    gpio_pull_up(MPU_SCL_PIN);
    
    // Wait for sensor to stabilize
    sleep_ms(100);
    
    // Check if sensor is connected
    if (!mpu6050_is_connected()) {
        printf("[MPU6050] ERROR: Sensor not found at address 0x%02X\n", MPU6050_ADDR);
        printf("[MPU6050] Check wiring: SDA=GPIO%d, SCL=GPIO%d\n", MPU_SDA_PIN, MPU_SCL_PIN);
        printf("[MPU6050] If AD0 pin is HIGH, change MPU6050_ADDR to 0x69\n");
        return false;
    }
    
    printf("[MPU6050] Sensor found (WHO_AM_I OK)\n");
    
    // Wake up the sensor (clear sleep bit in PWR_MGMT_1)
    // Also set clock source to PLL with X-axis gyro reference for better accuracy
    mpu_write_reg(MPU6050_REG_PWR_MGMT_1, 0x01);
    sleep_ms(100);
    
    // Configure sample rate divider (Sample Rate = 1kHz / (1 + SMPLRT_DIV))
    // Setting to 9 gives 100Hz sample rate
    mpu_write_reg(MPU6050_REG_SMPLRT_DIV, 9);
    
    // Configure Digital Low Pass Filter (DLPF)
    // DLPF_CFG = 3: Accel 44Hz, Gyro 42Hz bandwidth (good noise reduction)
    mpu_write_reg(MPU6050_REG_CONFIG, 0x03);
    
    // Configure Gyroscope: ±250°/s (most sensitive, best for small movements)
    // FS_SEL = 0
    mpu_write_reg(MPU6050_REG_GYRO_CONFIG, 0x00);
    
    // Configure Accelerometer: ±2g (most sensitive)
    // AFS_SEL = 0
    mpu_write_reg(MPU6050_REG_ACCEL_CONFIG, 0x00);
    
    // Reset orientation
    roll_angle = 0.0f;
    pitch_angle = 0.0f;
    yaw_angle = 0.0f;
    
    printf("[MPU6050] Configuration complete\n");
    printf("[MPU6050] Gyro: ±250°/s, Accel: ±2g, DLPF: 42Hz\n");
    
    return true;
}

bool mpu6050_is_connected(void)
{
    uint8_t who_am_i = 0;
    int ret = mpu_read_reg(MPU6050_REG_WHO_AM_I, &who_am_i, 1);
    
    if (ret < 0) {
        return false;
    }
    
    // WHO_AM_I should return 0x68 for MPU6050
    // (Some clones may return 0x72 or other values)
    return (who_am_i == 0x68 || who_am_i == 0x72 || who_am_i == 0x70);
}

void mpu6050_calibrate(uint16_t num_samples)
{
    if (num_samples == 0) {
        num_samples = CALIBRATION_SAMPLES;
    }
    
    printf("[MPU6050] Calibrating with %d samples...\n", num_samples);
    printf("[MPU6050] KEEP SENSOR FLAT AND STILL!\n");
    
    // Accumulators for averaging
    double gyro_x_sum = 0.0;
    double gyro_y_sum = 0.0;
    double gyro_z_sum = 0.0;
    double accel_x_sum = 0.0;
    double accel_y_sum = 0.0;
    double accel_z_sum = 0.0;
    
    MPU6050_RawData raw;
    
    // Collect samples
    for (uint16_t i = 0; i < num_samples; i++) {
        mpu6050_read_raw(&raw);
        
        // Accumulate raw values
        gyro_x_sum += raw.gyro_x;
        gyro_y_sum += raw.gyro_y;
        gyro_z_sum += raw.gyro_z;
        accel_x_sum += raw.accel_x;
        accel_y_sum += raw.accel_y;
        accel_z_sum += raw.accel_z;
        
        // Progress indicator every 100 samples
        if ((i + 1) % 100 == 0) {
            printf("[MPU6050] Calibration progress: %d/%d\n", i + 1, num_samples);
        }
        
        sleep_ms(2);  // ~500Hz sampling during calibration
    }
    
    // Calculate average offsets
    // Gyro offsets: should be near zero when stationary
    calibration.gyro_x_offset = (float)(gyro_x_sum / num_samples) / GYRO_SCALE_250DPS;
    calibration.gyro_y_offset = (float)(gyro_y_sum / num_samples) / GYRO_SCALE_250DPS;
    calibration.gyro_z_offset = (float)(gyro_z_sum / num_samples) / GYRO_SCALE_250DPS;
    
    // Accel offsets: X and Y should be ~0, Z should be ~1g when flat
    calibration.accel_x_offset = (float)(accel_x_sum / num_samples) / ACCEL_SCALE_2G;
    calibration.accel_y_offset = (float)(accel_y_sum / num_samples) / ACCEL_SCALE_2G;
    // For Z, we expect +1g (gravity), so offset = measured - 1.0
    calibration.accel_z_offset = ((float)(accel_z_sum / num_samples) / ACCEL_SCALE_2G) - 1.0f;
    
    is_calibrated = true;
    
    printf("[MPU6050] Calibration complete!\n");
    printf("[MPU6050] Gyro offsets (°/s): X=%.4f, Y=%.4f, Z=%.4f\n",
           calibration.gyro_x_offset, calibration.gyro_y_offset, calibration.gyro_z_offset);
    printf("[MPU6050] Accel offsets (g):  X=%.4f, Y=%.4f, Z=%.4f\n",
           calibration.accel_x_offset, calibration.accel_y_offset, calibration.accel_z_offset);
}

void mpu6050_read_raw(MPU6050_RawData *data)
{
    uint8_t buf[14];
    
    // Read all 14 bytes starting from ACCEL_XOUT_H
    // Order: AccelX, AccelY, AccelZ, Temp, GyroX, GyroY, GyroZ (2 bytes each)
    mpu_read_reg(MPU6050_REG_ACCEL_XOUT_H, buf, 14);
    
    data->accel_x = buf_to_int16(&buf[0]);
    data->accel_y = buf_to_int16(&buf[2]);
    data->accel_z = buf_to_int16(&buf[4]);
    data->temp    = buf_to_int16(&buf[6]);
    data->gyro_x  = buf_to_int16(&buf[8]);
    data->gyro_y  = buf_to_int16(&buf[10]);
    data->gyro_z  = buf_to_int16(&buf[12]);
}

void mpu6050_update(float dt)
{
    MPU6050_RawData raw;
    mpu6050_read_raw(&raw);
    
    // Convert raw values to physical units and apply calibration
    float accel_x = (raw.accel_x / ACCEL_SCALE_2G) - calibration.accel_x_offset;
    float accel_y = (raw.accel_y / ACCEL_SCALE_2G) - calibration.accel_y_offset;
    float accel_z = (raw.accel_z / ACCEL_SCALE_2G) - calibration.accel_z_offset;
    
    float gyro_x = (raw.gyro_x / GYRO_SCALE_250DPS) - calibration.gyro_x_offset;  // °/s
    float gyro_y = (raw.gyro_y / GYRO_SCALE_250DPS) - calibration.gyro_y_offset;
    float gyro_z = (raw.gyro_z / GYRO_SCALE_250DPS) - calibration.gyro_z_offset;
    
    // Calculate angles from accelerometer (only valid for roll and pitch)
    // Roll: rotation around X-axis
    float accel_roll = atan2f(accel_y, sqrtf(accel_x * accel_x + accel_z * accel_z)) * (180.0f / M_PI);
    
    // Pitch: rotation around Y-axis
    float accel_pitch = atan2f(-accel_x, sqrtf(accel_y * accel_y + accel_z * accel_z)) * (180.0f / M_PI);
    
    // Integrate gyroscope data
    // Note: Gyro gives angular velocity, so angle += rate * time
    roll_angle += gyro_x * dt;
    pitch_angle += gyro_y * dt;
    yaw_angle += gyro_z * dt;    // Yaw drifts since no magnetometer to correct it
    
    // Apply complementary filter for roll and pitch
    // Trust gyro for short-term (98%), trust accel for long-term drift correction (2%)
    roll_angle = COMPLEMENTARY_ALPHA * roll_angle + (1.0f - COMPLEMENTARY_ALPHA) * accel_roll;
    pitch_angle = COMPLEMENTARY_ALPHA * pitch_angle + (1.0f - COMPLEMENTARY_ALPHA) * accel_pitch;
    
    // Normalize yaw to -180 to +180 range
    while (yaw_angle > 180.0f) yaw_angle -= 360.0f;
    while (yaw_angle < -180.0f) yaw_angle += 360.0f;
}

void mpu6050_get_orientation(MPU6050_Orientation *orientation)
{
    orientation->roll = roll_angle;
    orientation->pitch = pitch_angle;
    orientation->yaw = yaw_angle;
}

float mpu6050_get_roll(void)
{
    return roll_angle;
}

float mpu6050_get_pitch(void)
{
    return pitch_angle;
}

float mpu6050_get_yaw(void)
{
    return yaw_angle;
}

void mpu6050_reset_orientation(void)
{
    roll_angle = 0.0f;
    pitch_angle = 0.0f;
    yaw_angle = 0.0f;
    printf("[MPU6050] Orientation reset to zero\n");
}

void mpu6050_reset_yaw(void)
{
    yaw_angle = 0.0f;
    printf("[MPU6050] Yaw reset to zero\n");
}

float mpu6050_get_temperature(void)
{
    uint8_t buf[2];
    mpu_read_reg(MPU6050_REG_TEMP_OUT_H, buf, 2);
    int16_t raw_temp = buf_to_int16(buf);
    
    // Temperature formula from datasheet:
    // Temperature in °C = (TEMP_OUT / 340.0) + 36.53
    return (raw_temp / 340.0f) + 36.53f;
}

void mpu6050_get_calibration(MPU6050_Calibration *cal)
{
    memcpy(cal, &calibration, sizeof(MPU6050_Calibration));
}
