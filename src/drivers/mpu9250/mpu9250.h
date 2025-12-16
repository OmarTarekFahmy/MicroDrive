/**
 * @file mpu9250.h
 * @brief MPU9250 9-DOF IMU Driver for Raspberry Pi Pico
 * 
 * This driver provides initialization, calibration, and orientation (YAW/PITCH/ROLL)
 * output using sensor fusion with a complementary filter and magnetometer correction.
 * 
 * The MPU9250 contains:
 * - 3-axis gyroscope (±250/500/1000/2000 °/s)
 * - 3-axis accelerometer (±2/4/8/16 g)
 * - 3-axis magnetometer AK8963 (14/16-bit resolution)
 * 
 * WHY NOT DMP:
 * The Digital Motion Processor (DMP) would be ideal for sensor fusion, but:
 * 1. DMP requires loading proprietary InvenSense firmware (~3KB blob)
 * 2. DMP programming sequence is not publicly documented
 * 3. Firmware binary is under restrictive licensing
 * 
 * Instead, we use a complementary filter with magnetometer correction for
 * drift-free yaw estimation, which provides excellent results for servo mirroring.
 */

#ifndef MPU9250_H
#define MPU9250_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CONFIGURATION - Adjust these for your setup
// ============================================================================

// I2C Configuration (using I2C1 by default, same as existing MPU6050 driver)
#define MPU9250_I2C_PORT        i2c1
#define MPU9250_SDA_PIN         2
#define MPU9250_SCL_PIN         3
#define MPU9250_I2C_FREQ        400000  // 400kHz Fast Mode

// MPU9250 I2C Address (0x68 if AD0=GND, 0x69 if AD0=VCC)
#define MPU9250_ADDR            0x68

// AK8963 Magnetometer I2C Address (internal to MPU9250)
#define AK8963_ADDR             0x0C

// ============================================================================
// MPU9250 REGISTER MAP
// ============================================================================

// Self-test registers
#define MPU9250_REG_SELF_TEST_X_GYRO    0x00
#define MPU9250_REG_SELF_TEST_Y_GYRO    0x01
#define MPU9250_REG_SELF_TEST_Z_GYRO    0x02
#define MPU9250_REG_SELF_TEST_X_ACCEL   0x0D
#define MPU9250_REG_SELF_TEST_Y_ACCEL   0x0E
#define MPU9250_REG_SELF_TEST_Z_ACCEL   0x0F

// Gyroscope offset registers
#define MPU9250_REG_XG_OFFSET_H         0x13
#define MPU9250_REG_XG_OFFSET_L         0x14
#define MPU9250_REG_YG_OFFSET_H         0x15
#define MPU9250_REG_YG_OFFSET_L         0x16
#define MPU9250_REG_ZG_OFFSET_H         0x17
#define MPU9250_REG_ZG_OFFSET_L         0x18

// Configuration registers
#define MPU9250_REG_SMPLRT_DIV          0x19    // Sample Rate Divider
#define MPU9250_REG_CONFIG              0x1A    // Configuration
#define MPU9250_REG_GYRO_CONFIG         0x1B    // Gyroscope Configuration
#define MPU9250_REG_ACCEL_CONFIG        0x1C    // Accelerometer Configuration
#define MPU9250_REG_ACCEL_CONFIG2       0x1D    // Accelerometer Configuration 2
#define MPU9250_REG_LP_ACCEL_ODR        0x1E    // Low Power Accelerometer ODR
#define MPU9250_REG_WOM_THR             0x1F    // Wake-on Motion Threshold

#define MPU9250_REG_FIFO_EN             0x23    // FIFO Enable
#define MPU9250_REG_I2C_MST_CTRL        0x24    // I2C Master Control
#define MPU9250_REG_I2C_SLV0_ADDR       0x25    // I2C Slave 0 Address
#define MPU9250_REG_I2C_SLV0_REG        0x26    // I2C Slave 0 Register
#define MPU9250_REG_I2C_SLV0_CTRL       0x27    // I2C Slave 0 Control
#define MPU9250_REG_I2C_SLV1_ADDR       0x28
#define MPU9250_REG_I2C_SLV1_REG        0x29
#define MPU9250_REG_I2C_SLV1_CTRL       0x2A
#define MPU9250_REG_I2C_SLV4_ADDR       0x31
#define MPU9250_REG_I2C_SLV4_REG        0x32
#define MPU9250_REG_I2C_SLV4_DO         0x33
#define MPU9250_REG_I2C_SLV4_CTRL       0x34
#define MPU9250_REG_I2C_SLV4_DI         0x35

#define MPU9250_REG_I2C_MST_STATUS      0x36    // I2C Master Status
#define MPU9250_REG_INT_PIN_CFG         0x37    // INT Pin / Bypass Enable
#define MPU9250_REG_INT_ENABLE          0x38    // Interrupt Enable
#define MPU9250_REG_INT_STATUS          0x3A    // Interrupt Status

// Sensor output registers
#define MPU9250_REG_ACCEL_XOUT_H        0x3B    // Accelerometer X-axis High Byte
#define MPU9250_REG_ACCEL_XOUT_L        0x3C
#define MPU9250_REG_ACCEL_YOUT_H        0x3D
#define MPU9250_REG_ACCEL_YOUT_L        0x3E
#define MPU9250_REG_ACCEL_ZOUT_H        0x3F
#define MPU9250_REG_ACCEL_ZOUT_L        0x40
#define MPU9250_REG_TEMP_OUT_H          0x41    // Temperature High Byte
#define MPU9250_REG_TEMP_OUT_L          0x42
#define MPU9250_REG_GYRO_XOUT_H         0x43    // Gyroscope X-axis High Byte
#define MPU9250_REG_GYRO_XOUT_L         0x44
#define MPU9250_REG_GYRO_YOUT_H         0x45
#define MPU9250_REG_GYRO_YOUT_L         0x46
#define MPU9250_REG_GYRO_ZOUT_H         0x47
#define MPU9250_REG_GYRO_ZOUT_L         0x48

// External sensor data (used to read magnetometer via I2C master)
#define MPU9250_REG_EXT_SENS_DATA_00    0x49

// I2C Slave data out
#define MPU9250_REG_I2C_SLV0_DO         0x63

// Control registers
#define MPU9250_REG_I2C_MST_DELAY_CTRL  0x67    // I2C Master Delay Control
#define MPU9250_REG_SIGNAL_PATH_RESET   0x68    // Signal Path Reset
#define MPU9250_REG_MOT_DETECT_CTRL     0x69    // Motion Detection Control
#define MPU9250_REG_USER_CTRL           0x6A    // User Control
#define MPU9250_REG_PWR_MGMT_1          0x6B    // Power Management 1
#define MPU9250_REG_PWR_MGMT_2          0x6C    // Power Management 2
#define MPU9250_REG_FIFO_COUNTH         0x72    // FIFO Count High Byte
#define MPU9250_REG_FIFO_COUNTL         0x73
#define MPU9250_REG_FIFO_R_W            0x74    // FIFO Read/Write
#define MPU9250_REG_WHO_AM_I            0x75    // Device ID (should return 0x71)

// Accelerometer offset registers
#define MPU9250_REG_XA_OFFSET_H         0x77
#define MPU9250_REG_XA_OFFSET_L         0x78
#define MPU9250_REG_YA_OFFSET_H         0x7A
#define MPU9250_REG_YA_OFFSET_L         0x7B
#define MPU9250_REG_ZA_OFFSET_H         0x7D
#define MPU9250_REG_ZA_OFFSET_L         0x7E

// ============================================================================
// AK8963 MAGNETOMETER REGISTER MAP
// ============================================================================

#define AK8963_REG_WIA                  0x00    // Device ID (should return 0x48)
#define AK8963_REG_INFO                 0x01    // Information
#define AK8963_REG_ST1                  0x02    // Status 1 (Data Ready)
#define AK8963_REG_HXL                  0x03    // Mag X-axis Low Byte
#define AK8963_REG_HXH                  0x04
#define AK8963_REG_HYL                  0x05
#define AK8963_REG_HYH                  0x06
#define AK8963_REG_HZL                  0x07
#define AK8963_REG_HZH                  0x08
#define AK8963_REG_ST2                  0x09    // Status 2 (Overflow)
#define AK8963_REG_CNTL1                0x0A    // Control 1 (Mode)
#define AK8963_REG_CNTL2                0x0B    // Control 2 (Reset)
#define AK8963_REG_ASTC                 0x0C    // Self-test
#define AK8963_REG_ASAX                 0x10    // X-axis sensitivity adjustment
#define AK8963_REG_ASAY                 0x11    // Y-axis sensitivity adjustment
#define AK8963_REG_ASAZ                 0x12    // Z-axis sensitivity adjustment

// AK8963 operating modes
#define AK8963_MODE_POWERDOWN           0x00
#define AK8963_MODE_SINGLE              0x01
#define AK8963_MODE_CONTINUOUS_8HZ      0x02
#define AK8963_MODE_CONTINUOUS_100HZ    0x06
#define AK8963_MODE_SELFTEST            0x08
#define AK8963_MODE_FUSEROM             0x0F
#define AK8963_BIT_16BIT                0x10    // 16-bit output (OR with mode)

// ============================================================================
// CONFIGURATION VALUES
// ============================================================================

// WHO_AM_I expected values
#define MPU9250_WHO_AM_I_VALUE          0x71    // MPU9250
#define MPU9255_WHO_AM_I_VALUE          0x73    // MPU9255 variant
#define AK8963_WHO_AM_I_VALUE           0x48    // AK8963 magnetometer

// Gyroscope full scale range
typedef enum {
    MPU9250_GYRO_FS_250DPS  = 0x00,   // ±250 °/s (most sensitive)
    MPU9250_GYRO_FS_500DPS  = 0x08,   // ±500 °/s
    MPU9250_GYRO_FS_1000DPS = 0x10,   // ±1000 °/s
    MPU9250_GYRO_FS_2000DPS = 0x18    // ±2000 °/s (least sensitive)
} mpu9250_gyro_fs_t;

// Accelerometer full scale range
typedef enum {
    MPU9250_ACCEL_FS_2G  = 0x00,      // ±2g (most sensitive)
    MPU9250_ACCEL_FS_4G  = 0x08,      // ±4g
    MPU9250_ACCEL_FS_8G  = 0x10,      // ±8g
    MPU9250_ACCEL_FS_16G = 0x18       // ±16g (least sensitive)
} mpu9250_accel_fs_t;

// Sensitivity scale factors
#define GYRO_SCALE_250DPS       131.0f      // LSB/(°/s)
#define GYRO_SCALE_500DPS       65.5f
#define GYRO_SCALE_1000DPS      32.8f
#define GYRO_SCALE_2000DPS      16.4f

#define ACCEL_SCALE_2G          16384.0f    // LSB/g
#define ACCEL_SCALE_4G          8192.0f
#define ACCEL_SCALE_8G          4096.0f
#define ACCEL_SCALE_16G         2048.0f

#define MAG_SCALE_16BIT         0.15f       // µT/LSB (4912µT / 32760)

// Complementary filter coefficients
#define COMPLEMENTARY_ALPHA     0.98f       // Gyro trust (0.98 = 98% gyro, 2% accel)
#define MAG_ALPHA               0.05f       // Magnetometer blend for yaw correction

// Calibration samples
#define CALIBRATION_SAMPLES     2000

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * @brief Raw sensor data from MPU9250 (accelerometer + gyroscope + temperature)
 */
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temp;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} mpu9250_raw_data_t;

/**
 * @brief Raw magnetometer data from AK8963
 */
typedef struct {
    int16_t mag_x;
    int16_t mag_y;
    int16_t mag_z;
    bool overflow;      // True if magnetic sensor overflow detected
} mpu9250_mag_raw_t;

/**
 * @brief Calibration offsets for sensors
 */
typedef struct {
    // Gyroscope offsets (°/s)
    float gyro_x_offset;
    float gyro_y_offset;
    float gyro_z_offset;
    
    // Accelerometer offsets (g)
    float accel_x_offset;
    float accel_y_offset;
    float accel_z_offset;
    
    // Magnetometer hard-iron offsets (µT)
    float mag_x_offset;
    float mag_y_offset;
    float mag_z_offset;
    
    // Magnetometer soft-iron scale factors
    float mag_x_scale;
    float mag_y_scale;
    float mag_z_scale;
} mpu9250_calibration_t;

/**
 * @brief Orientation angles in degrees
 * 
 * Roll:  Rotation around X-axis (-180 to +180°)
 * Pitch: Rotation around Y-axis (-90 to +90°)
 * Yaw:   Rotation around Z-axis (0 to 360° or -180 to +180°)
 * 
 * For servo mirroring:
 * - Map roll  to servo controlling rotation around X-axis
 * - Map pitch to servo controlling rotation around Y-axis
 * - Map yaw   to servo controlling rotation around Z-axis
 */
typedef struct {
    float roll;         // X-axis rotation in degrees
    float pitch;        // Y-axis rotation in degrees
    float yaw;          // Z-axis rotation in degrees (magnetometer corrected)
} mpu9250_orientation_t;

/**
 * @brief Scaled sensor data in physical units
 */
typedef struct {
    float accel_x;      // g
    float accel_y;      // g
    float accel_z;      // g
    float gyro_x;       // °/s
    float gyro_y;       // °/s
    float gyro_z;       // °/s
    float mag_x;        // µT
    float mag_y;        // µT
    float mag_z;        // µT
    float temp;         // °C
} mpu9250_scaled_data_t;

/**
 * @brief MPU9250 configuration structure
 */
typedef struct {
    mpu9250_gyro_fs_t gyro_fs;      // Gyroscope full scale
    mpu9250_accel_fs_t accel_fs;    // Accelerometer full scale
    uint8_t sample_rate_div;         // Sample rate divider (0-255)
    uint8_t dlpf_cfg;               // Digital low-pass filter config
    bool use_magnetometer;          // Enable magnetometer for yaw correction
} mpu9250_config_t;

// ============================================================================
// PUBLIC API - INITIALIZATION
// ============================================================================

/**
 * @brief Initialize MPU9250 with default configuration
 * 
 * Default settings:
 * - Gyroscope: ±250°/s
 * - Accelerometer: ±2g
 * - Sample rate: 100Hz
 * - DLPF: 41Hz bandwidth
 * - Magnetometer: enabled at 100Hz
 * 
 * @return true if initialization successful, false otherwise
 */
bool mpu9250_init(void);

/**
 * @brief Initialize MPU9250 with custom configuration
 * @param config Pointer to configuration structure
 * @return true if initialization successful, false otherwise
 */
bool mpu9250_init_config(const mpu9250_config_t *config);

/**
 * @brief Check if MPU9250 is connected and responding
 * @return true if WHO_AM_I register returns expected value
 */
bool mpu9250_is_connected(void);

/**
 * @brief Check if AK8963 magnetometer is connected
 * @return true if magnetometer WHO_AM_I returns expected value
 */
bool mpu9250_mag_is_connected(void);

/**
 * @brief Reset MPU9250 to default state
 */
void mpu9250_reset(void);

// ============================================================================
// PUBLIC API - CALIBRATION
// ============================================================================

/**
 * @brief Calibrate gyroscope and accelerometer
 * 
 * IMPORTANT: Keep sensor FLAT and COMPLETELY STILL during calibration!
 * This measures sensor bias when stationary.
 * 
 * @param num_samples Number of samples to average (0 = use default)
 */
void mpu9250_calibrate_accel_gyro(uint16_t num_samples);

/**
 * @brief Calibrate magnetometer (hard-iron and soft-iron)
 * 
 * IMPORTANT: Rotate sensor slowly in a figure-8 pattern during calibration
 * to sample all orientations. Continue for about 30-60 seconds.
 * 
 * @param duration_ms Duration of calibration in milliseconds
 */
void mpu9250_calibrate_magnetometer(uint32_t duration_ms);

/**
 * @brief Set calibration offsets manually
 * @param cal Pointer to calibration structure
 */
void mpu9250_set_calibration(const mpu9250_calibration_t *cal);

/**
 * @brief Get current calibration offsets
 * @param cal Pointer to store calibration values
 */
void mpu9250_get_calibration(mpu9250_calibration_t *cal);

/**
 * @brief Check if sensor has been calibrated
 * @return true if calibration has been performed
 */
bool mpu9250_is_calibrated(void);

// ============================================================================
// PUBLIC API - SENSOR READING
// ============================================================================

/**
 * @brief Read raw accelerometer and gyroscope data
 * @param data Pointer to store raw data
 */
void mpu9250_read_raw(mpu9250_raw_data_t *data);

/**
 * @brief Read raw magnetometer data
 * @param data Pointer to store magnetometer data
 * @return true if new data available, false if data not ready
 */
bool mpu9250_read_mag_raw(mpu9250_mag_raw_t *data);

/**
 * @brief Read all sensors and return scaled values in physical units
 * @param data Pointer to store scaled data
 */
void mpu9250_read_scaled(mpu9250_scaled_data_t *data);

/**
 * @brief Get temperature in degrees Celsius
 * @return Temperature reading
 */
float mpu9250_get_temperature(void);

// ============================================================================
// PUBLIC API - ORIENTATION (YAW/PITCH/ROLL)
// ============================================================================

/**
 * @brief Update orientation using complementary filter
 * 
 * Call this regularly (every 10-20ms recommended) for accurate tracking.
 * The function fuses accelerometer, gyroscope, and magnetometer data
 * to compute roll, pitch, and yaw angles.
 * 
 * @param dt Time since last update in seconds (e.g., 0.01 for 100Hz)
 */
void mpu9250_update_orientation(float dt);

/**
 * @brief Get current orientation angles
 * @param orientation Pointer to store roll, pitch, yaw in degrees
 */
void mpu9250_get_orientation(mpu9250_orientation_t *orientation);

/**
 * @brief Get roll angle (rotation around X-axis)
 * @return Roll angle in degrees (-180 to +180)
 */
float mpu9250_get_roll(void);

/**
 * @brief Get pitch angle (rotation around Y-axis)
 * @return Pitch angle in degrees (-90 to +90)
 */
float mpu9250_get_pitch(void);

/**
 * @brief Get yaw angle (rotation around Z-axis)
 * 
 * Yaw is corrected using magnetometer to prevent gyro drift.
 * 
 * @return Yaw angle in degrees (0 to 360 or -180 to +180 depending on config)
 */
float mpu9250_get_yaw(void);

/**
 * @brief Reset all orientation angles to zero
 */
void mpu9250_reset_orientation(void);

/**
 * @brief Reset only yaw angle to zero
 * 
 * Useful to set current heading as reference direction.
 */
void mpu9250_reset_yaw(void);

// ============================================================================
// PUBLIC API - SERVO MAPPING UTILITIES
// ============================================================================

/**
 * @brief Map orientation angle to servo pulse width
 * 
 * Maps an angle to a servo pulse width for 180° positional servos.
 * 
 * @param angle Input angle in degrees
 * @param angle_min Minimum input angle (e.g., -90)
 * @param angle_max Maximum input angle (e.g., +90)
 * @param pulse_min Minimum servo pulse in µs (typically 500-1000)
 * @param pulse_max Maximum servo pulse in µs (typically 2000-2500)
 * @return Servo pulse width in microseconds
 */
uint16_t mpu9250_angle_to_servo_pulse(float angle, 
                                       float angle_min, float angle_max,
                                       uint16_t pulse_min, uint16_t pulse_max);

/**
 * @brief Get all servo pulse widths for mirroring orientation
 * 
 * Convenience function that maps roll, pitch, yaw to three servo pulses.
 * Assumes standard 180° servos with 500-2500µs pulse range.
 * 
 * @param roll_pulse Output pulse for roll servo (µs)
 * @param pitch_pulse Output pulse for pitch servo (µs)
 * @param yaw_pulse Output pulse for yaw servo (µs)
 */
void mpu9250_get_servo_pulses(uint16_t *roll_pulse, 
                               uint16_t *pitch_pulse, 
                               uint16_t *yaw_pulse);

// ============================================================================
// LOW-LEVEL REGISTER ACCESS (for advanced users)
// ============================================================================

/**
 * @brief Write a byte to MPU9250 register
 * @param reg Register address
 * @param value Value to write
 * @return Number of bytes written, or PICO_ERROR_GENERIC on failure
 */
int mpu9250_write_reg(uint8_t reg, uint8_t value);

/**
 * @brief Read bytes from MPU9250
 * @param reg Starting register address
 * @param buf Buffer to store data
 * @param len Number of bytes to read
 * @return Number of bytes read, or PICO_ERROR_GENERIC on failure
 */
int mpu9250_read_reg(uint8_t reg, uint8_t *buf, uint8_t len);

/**
 * @brief Write to AK8963 magnetometer register (via I2C master)
 * @param reg Register address
 * @param value Value to write
 * @return true on success
 */
bool mpu9250_mag_write_reg(uint8_t reg, uint8_t value);

/**
 * @brief Read from AK8963 magnetometer register (via I2C master)
 * @param reg Starting register address
 * @param buf Buffer to store data
 * @param len Number of bytes to read
 * @return true on success
 */
bool mpu9250_mag_read_reg(uint8_t reg, uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif // MPU9250_H
