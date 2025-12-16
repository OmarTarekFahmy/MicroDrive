/**
 * @file mpu9250.c
 * @brief MPU9250 9-DOF IMU Driver Implementation
 * 
 * Implements initialization, calibration, and orientation tracking
 * using a complementary filter with magnetometer correction for
 * drift-free yaw estimation.
 */

#include "mpu9250.h"
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
static float s_roll = 0.0f;
static float s_pitch = 0.0f;
static float s_yaw = 0.0f;

// Magnetometer sensitivity adjustment values (from fuse ROM)
static float s_mag_adj_x = 1.0f;
static float s_mag_adj_y = 1.0f;
static float s_mag_adj_z = 1.0f;

// Current scale factors (based on configuration)
static float s_gyro_scale = GYRO_SCALE_250DPS;
static float s_accel_scale = ACCEL_SCALE_2G;

// Calibration data
static mpu9250_calibration_t s_calibration = {
    .gyro_x_offset = 0.0f,
    .gyro_y_offset = 0.0f,
    .gyro_z_offset = 0.0f,
    .accel_x_offset = 0.0f,
    .accel_y_offset = 0.0f,
    .accel_z_offset = 0.0f,
    .mag_x_offset = 0.0f,
    .mag_y_offset = 0.0f,
    .mag_z_offset = 0.0f,
    .mag_x_scale = 1.0f,
    .mag_y_scale = 1.0f,
    .mag_z_scale = 1.0f
};

// Calibration state
static bool s_accel_gyro_calibrated = false;
static bool s_mag_calibrated = false;

// Magnetometer enabled flag
static bool s_use_magnetometer = true;

// Reference yaw (for magnetometer correction)
static float s_mag_declination = 0.0f;  // Magnetic declination angle

// ============================================================================
// PRIVATE HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Write a single byte to an MPU9250 register
 */
int mpu9250_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    int ret = i2c_write_blocking(MPU9250_I2C_PORT, MPU9250_ADDR, buf, 2, false);
    return ret;
}

/**
 * @brief Read bytes from MPU9250 starting at given register
 */
int mpu9250_read_reg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    int ret = i2c_write_blocking(MPU9250_I2C_PORT, MPU9250_ADDR, &reg, 1, true);
    if (ret < 0) return ret;
    
    ret = i2c_read_blocking(MPU9250_I2C_PORT, MPU9250_ADDR, buf, len, false);
    return ret;
}

/**
 * @brief Convert raw 16-bit value from big-endian buffer
 */
static inline int16_t buf_to_int16_be(const uint8_t *buf)
{
    return (int16_t)((buf[0] << 8) | buf[1]);
}

/**
 * @brief Convert raw 16-bit value from little-endian buffer (magnetometer)
 */
static inline int16_t buf_to_int16_le(const uint8_t *buf)
{
    return (int16_t)((buf[1] << 8) | buf[0]);
}

/**
 * @brief Write to AK8963 via MPU9250 I2C master interface
 */
bool mpu9250_mag_write_reg(uint8_t reg, uint8_t value)
{
    // Set slave 0 address for write (bit 7 = 0)
    mpu9250_write_reg(MPU9250_REG_I2C_SLV0_ADDR, AK8963_ADDR);
    // Set register to write
    mpu9250_write_reg(MPU9250_REG_I2C_SLV0_REG, reg);
    // Set data to write
    mpu9250_write_reg(MPU9250_REG_I2C_SLV0_DO, value);
    // Enable slave 0, 1 byte
    mpu9250_write_reg(MPU9250_REG_I2C_SLV0_CTRL, 0x81);
    
    sleep_ms(10);  // Wait for transaction
    return true;
}

/**
 * @brief Read from AK8963 via MPU9250 I2C master interface
 */
bool mpu9250_mag_read_reg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    // Set slave 0 address for read (bit 7 = 1)
    mpu9250_write_reg(MPU9250_REG_I2C_SLV0_ADDR, AK8963_ADDR | 0x80);
    // Set register to read
    mpu9250_write_reg(MPU9250_REG_I2C_SLV0_REG, reg);
    // Enable slave 0, read len bytes
    mpu9250_write_reg(MPU9250_REG_I2C_SLV0_CTRL, 0x80 | len);
    
    sleep_ms(10);  // Wait for transaction
    
    // Read from external sensor data registers
    return mpu9250_read_reg(MPU9250_REG_EXT_SENS_DATA_00, buf, len) >= 0;
}

/**
 * @brief Initialize the AK8963 magnetometer
 */
static bool ak8963_init(void)
{
    uint8_t buf[3];
    
    printf("[MPU9250] Initializing AK8963 magnetometer...\n");
    
    // Reset magnetometer
    mpu9250_mag_write_reg(AK8963_REG_CNTL2, 0x01);
    sleep_ms(100);
    
    // Power down before changing mode
    mpu9250_mag_write_reg(AK8963_REG_CNTL1, AK8963_MODE_POWERDOWN);
    sleep_ms(10);
    
    // Enter Fuse ROM access mode to read sensitivity adjustment
    mpu9250_mag_write_reg(AK8963_REG_CNTL1, AK8963_MODE_FUSEROM);
    sleep_ms(10);
    
    // Read sensitivity adjustment values
    if (!mpu9250_mag_read_reg(AK8963_REG_ASAX, buf, 3)) {
        printf("[MPU9250] ERROR: Failed to read magnetometer sensitivity\n");
        return false;
    }
    
    // Calculate adjustment factors: Hadj = H * ((ASA - 128) * 0.5 / 128 + 1)
    s_mag_adj_x = ((float)(buf[0] - 128) / 256.0f) + 1.0f;
    s_mag_adj_y = ((float)(buf[1] - 128) / 256.0f) + 1.0f;
    s_mag_adj_z = ((float)(buf[2] - 128) / 256.0f) + 1.0f;
    
    printf("[MPU9250] Mag sensitivity: X=%.3f, Y=%.3f, Z=%.3f\n",
           s_mag_adj_x, s_mag_adj_y, s_mag_adj_z);
    
    // Power down again
    mpu9250_mag_write_reg(AK8963_REG_CNTL1, AK8963_MODE_POWERDOWN);
    sleep_ms(10);
    
    // Set to continuous measurement mode 2 (100Hz) with 16-bit output
    mpu9250_mag_write_reg(AK8963_REG_CNTL1, AK8963_MODE_CONTINUOUS_100HZ | AK8963_BIT_16BIT);
    sleep_ms(10);
    
    printf("[MPU9250] AK8963 initialized: 100Hz, 16-bit mode\n");
    return true;
}

/**
 * @brief Configure MPU9250 I2C master for magnetometer access
 */
static void configure_i2c_master(void)
{
    // Enable I2C master mode
    uint8_t user_ctrl;
    mpu9250_read_reg(MPU9250_REG_USER_CTRL, &user_ctrl, 1);
    mpu9250_write_reg(MPU9250_REG_USER_CTRL, user_ctrl | 0x20);  // I2C_MST_EN
    
    // Configure I2C master clock: 400kHz
    mpu9250_write_reg(MPU9250_REG_I2C_MST_CTRL, 0x0D);  // 400kHz
    
    sleep_ms(10);
}

/**
 * @brief Setup automatic magnetometer reading via I2C slave
 */
static void setup_mag_continuous_read(void)
{
    // Configure slave 0 to continuously read magnetometer data
    // Read starting from ST1 (status) through HZH and ST2 (8 bytes total)
    mpu9250_write_reg(MPU9250_REG_I2C_SLV0_ADDR, AK8963_ADDR | 0x80);  // Read
    mpu9250_write_reg(MPU9250_REG_I2C_SLV0_REG, AK8963_REG_ST1);       // Start at ST1
    mpu9250_write_reg(MPU9250_REG_I2C_SLV0_CTRL, 0x88);                // Enable, 8 bytes
    
    // Set I2C master delay for slave 0
    mpu9250_write_reg(MPU9250_REG_I2C_MST_DELAY_CTRL, 0x01);
}

// ============================================================================
// PUBLIC FUNCTIONS - INITIALIZATION
// ============================================================================

bool mpu9250_init(void)
{
    mpu9250_config_t default_config = {
        .gyro_fs = MPU9250_GYRO_FS_250DPS,
        .accel_fs = MPU9250_ACCEL_FS_2G,
        .sample_rate_div = 9,       // 100Hz (1kHz / (1 + 9))
        .dlpf_cfg = 3,              // 41Hz bandwidth
        .use_magnetometer = true
    };
    
    return mpu9250_init_config(&default_config);
}

bool mpu9250_init_config(const mpu9250_config_t *config)
{
    printf("[MPU9250] Initializing...\n");
    
    // Initialize I2C peripheral
    i2c_init(MPU9250_I2C_PORT, MPU9250_I2C_FREQ);
    
    // Configure GPIO pins for I2C
    gpio_set_function(MPU9250_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(MPU9250_SCL_PIN, GPIO_FUNC_I2C);
    
    // Enable internal pull-ups
    gpio_pull_up(MPU9250_SDA_PIN);
    gpio_pull_up(MPU9250_SCL_PIN);
    
    // Wait for sensor to stabilize
    sleep_ms(100);
    
    // Check if sensor is connected
    if (!mpu9250_is_connected()) {
        printf("[MPU9250] ERROR: Sensor not found at address 0x%02X\n", MPU9250_ADDR);
        printf("[MPU9250] Check wiring: SDA=GPIO%d, SCL=GPIO%d\n", 
               MPU9250_SDA_PIN, MPU9250_SCL_PIN);
        return false;
    }
    
    printf("[MPU9250] Sensor found (WHO_AM_I OK)\n");
    
    // Reset device
    mpu9250_reset();
    
    // Wake up: clear sleep bit, use auto-select clock source
    mpu9250_write_reg(MPU9250_REG_PWR_MGMT_1, 0x01);  // Auto clock source
    sleep_ms(100);
    
    // Configure sample rate divider
    mpu9250_write_reg(MPU9250_REG_SMPLRT_DIV, config->sample_rate_div);
    
    // Configure DLPF
    mpu9250_write_reg(MPU9250_REG_CONFIG, config->dlpf_cfg);
    
    // Configure Gyroscope
    mpu9250_write_reg(MPU9250_REG_GYRO_CONFIG, config->gyro_fs);
    switch (config->gyro_fs) {
        case MPU9250_GYRO_FS_250DPS:  s_gyro_scale = GYRO_SCALE_250DPS; break;
        case MPU9250_GYRO_FS_500DPS:  s_gyro_scale = GYRO_SCALE_500DPS; break;
        case MPU9250_GYRO_FS_1000DPS: s_gyro_scale = GYRO_SCALE_1000DPS; break;
        case MPU9250_GYRO_FS_2000DPS: s_gyro_scale = GYRO_SCALE_2000DPS; break;
    }
    
    // Configure Accelerometer
    mpu9250_write_reg(MPU9250_REG_ACCEL_CONFIG, config->accel_fs);
    mpu9250_write_reg(MPU9250_REG_ACCEL_CONFIG2, config->dlpf_cfg);  // DLPF for accel
    switch (config->accel_fs) {
        case MPU9250_ACCEL_FS_2G:  s_accel_scale = ACCEL_SCALE_2G; break;
        case MPU9250_ACCEL_FS_4G:  s_accel_scale = ACCEL_SCALE_4G; break;
        case MPU9250_ACCEL_FS_8G:  s_accel_scale = ACCEL_SCALE_8G; break;
        case MPU9250_ACCEL_FS_16G: s_accel_scale = ACCEL_SCALE_16G; break;
    }
    
    // Enable I2C bypass for direct magnetometer access initially
    mpu9250_write_reg(MPU9250_REG_INT_PIN_CFG, 0x02);  // BYPASS_EN
    sleep_ms(10);
    
    // Initialize magnetometer if requested
    s_use_magnetometer = config->use_magnetometer;
    if (s_use_magnetometer) {
        if (!mpu9250_mag_is_connected()) {
            printf("[MPU9250] WARNING: Magnetometer not detected, disabling\n");
            s_use_magnetometer = false;
        } else {
            // Initialize AK8963 via bypass
            if (!ak8963_init()) {
                printf("[MPU9250] WARNING: Magnetometer init failed, disabling\n");
                s_use_magnetometer = false;
            }
        }
    }
    
    // Disable bypass and enable I2C master mode
    mpu9250_write_reg(MPU9250_REG_INT_PIN_CFG, 0x00);  // Disable bypass
    configure_i2c_master();
    
    // Setup continuous magnetometer reading
    if (s_use_magnetometer) {
        setup_mag_continuous_read();
    }
    
    // Reset orientation
    s_roll = 0.0f;
    s_pitch = 0.0f;
    s_yaw = 0.0f;
    
    printf("[MPU9250] Configuration complete\n");
    printf("[MPU9250] Gyro: ±%d°/s, Accel: ±%dg, Mag: %s\n",
           (config->gyro_fs == MPU9250_GYRO_FS_250DPS) ? 250 :
           (config->gyro_fs == MPU9250_GYRO_FS_500DPS) ? 500 :
           (config->gyro_fs == MPU9250_GYRO_FS_1000DPS) ? 1000 : 2000,
           (config->accel_fs == MPU9250_ACCEL_FS_2G) ? 2 :
           (config->accel_fs == MPU9250_ACCEL_FS_4G) ? 4 :
           (config->accel_fs == MPU9250_ACCEL_FS_8G) ? 8 : 16,
           s_use_magnetometer ? "enabled" : "disabled");
    
    return true;
}

bool mpu9250_is_connected(void)
{
    uint8_t who_am_i = 0;
    int ret = mpu9250_read_reg(MPU9250_REG_WHO_AM_I, &who_am_i, 1);
    
    if (ret < 0) {
        return false;
    }
    
    // WHO_AM_I should return 0x71 for MPU9250 or 0x73 for MPU9255
    return (who_am_i == MPU9250_WHO_AM_I_VALUE || 
            who_am_i == MPU9255_WHO_AM_I_VALUE);
}

bool mpu9250_mag_is_connected(void)
{
    // Need to enable bypass to directly check magnetometer
    uint8_t int_pin_cfg;
    mpu9250_read_reg(MPU9250_REG_INT_PIN_CFG, &int_pin_cfg, 1);
    mpu9250_write_reg(MPU9250_REG_INT_PIN_CFG, int_pin_cfg | 0x02);
    sleep_ms(10);
    
    // Direct I2C read from AK8963
    uint8_t who_am_i = 0;
    uint8_t reg = AK8963_REG_WIA;
    i2c_write_blocking(MPU9250_I2C_PORT, AK8963_ADDR, &reg, 1, true);
    i2c_read_blocking(MPU9250_I2C_PORT, AK8963_ADDR, &who_am_i, 1, false);
    
    // Restore original setting
    mpu9250_write_reg(MPU9250_REG_INT_PIN_CFG, int_pin_cfg);
    
    return (who_am_i == AK8963_WHO_AM_I_VALUE);
}

void mpu9250_reset(void)
{
    // Reset device
    mpu9250_write_reg(MPU9250_REG_PWR_MGMT_1, 0x80);
    sleep_ms(100);
    
    // Reset calibration
    s_accel_gyro_calibrated = false;
    s_mag_calibrated = false;
    
    // Reset orientation
    s_roll = 0.0f;
    s_pitch = 0.0f;
    s_yaw = 0.0f;
}

// ============================================================================
// PUBLIC FUNCTIONS - CALIBRATION
// ============================================================================

void mpu9250_calibrate_accel_gyro(uint16_t num_samples)
{
    if (num_samples == 0) {
        num_samples = CALIBRATION_SAMPLES;
    }
    
    printf("[MPU9250] Calibrating gyro/accel with %d samples...\n", num_samples);
    printf("[MPU9250] KEEP SENSOR FLAT AND STILL!\n");
    
    // Accumulators
    double gyro_x_sum = 0.0, gyro_y_sum = 0.0, gyro_z_sum = 0.0;
    double accel_x_sum = 0.0, accel_y_sum = 0.0, accel_z_sum = 0.0;
    
    mpu9250_raw_data_t raw;
    
    for (uint16_t i = 0; i < num_samples; i++) {
        mpu9250_read_raw(&raw);
        
        gyro_x_sum += raw.gyro_x;
        gyro_y_sum += raw.gyro_y;
        gyro_z_sum += raw.gyro_z;
        accel_x_sum += raw.accel_x;
        accel_y_sum += raw.accel_y;
        accel_z_sum += raw.accel_z;
        
        if ((i + 1) % 200 == 0) {
            printf("[MPU9250] Progress: %d/%d\n", i + 1, num_samples);
        }
        
        sleep_ms(2);
    }
    
    // Calculate offsets
    s_calibration.gyro_x_offset = (float)(gyro_x_sum / num_samples) / s_gyro_scale;
    s_calibration.gyro_y_offset = (float)(gyro_y_sum / num_samples) / s_gyro_scale;
    s_calibration.gyro_z_offset = (float)(gyro_z_sum / num_samples) / s_gyro_scale;
    
    s_calibration.accel_x_offset = (float)(accel_x_sum / num_samples) / s_accel_scale;
    s_calibration.accel_y_offset = (float)(accel_y_sum / num_samples) / s_accel_scale;
    // Z-axis: subtract 1g (gravity)
    s_calibration.accel_z_offset = ((float)(accel_z_sum / num_samples) / s_accel_scale) - 1.0f;
    
    s_accel_gyro_calibrated = true;
    
    printf("[MPU9250] Gyro/Accel calibration complete!\n");
    printf("[MPU9250] Gyro offsets (°/s): X=%.4f, Y=%.4f, Z=%.4f\n",
           s_calibration.gyro_x_offset, 
           s_calibration.gyro_y_offset, 
           s_calibration.gyro_z_offset);
    printf("[MPU9250] Accel offsets (g): X=%.4f, Y=%.4f, Z=%.4f\n",
           s_calibration.accel_x_offset,
           s_calibration.accel_y_offset,
           s_calibration.accel_z_offset);
}

void mpu9250_calibrate_magnetometer(uint32_t duration_ms)
{
    if (!s_use_magnetometer) {
        printf("[MPU9250] Magnetometer not enabled, skipping calibration\n");
        return;
    }
    
    printf("[MPU9250] Calibrating magnetometer for %lu ms...\n", duration_ms);
    printf("[MPU9250] SLOWLY ROTATE SENSOR IN FIGURE-8 PATTERN!\n");
    
    float mag_x_min = 99999.0f, mag_x_max = -99999.0f;
    float mag_y_min = 99999.0f, mag_y_max = -99999.0f;
    float mag_z_min = 99999.0f, mag_z_max = -99999.0f;
    
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    uint32_t samples = 0;
    
    while ((to_ms_since_boot(get_absolute_time()) - start_time) < duration_ms) {
        mpu9250_mag_raw_t mag;
        if (mpu9250_read_mag_raw(&mag)) {
            // Convert to µT with sensitivity adjustment
            float mx = (float)mag.mag_x * MAG_SCALE_16BIT * s_mag_adj_x;
            float my = (float)mag.mag_y * MAG_SCALE_16BIT * s_mag_adj_y;
            float mz = (float)mag.mag_z * MAG_SCALE_16BIT * s_mag_adj_z;
            
            // Track min/max
            if (mx < mag_x_min) mag_x_min = mx;
            if (mx > mag_x_max) mag_x_max = mx;
            if (my < mag_y_min) mag_y_min = my;
            if (my > mag_y_max) mag_y_max = my;
            if (mz < mag_z_min) mag_z_min = mz;
            if (mz > mag_z_max) mag_z_max = mz;
            
            samples++;
        }
        
        // Progress every 5 seconds
        uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start_time;
        if (elapsed % 5000 < 50) {
            printf("[MPU9250] Mag cal progress: %lu/%lu ms, %lu samples\n", 
                   elapsed, duration_ms, samples);
        }
        
        sleep_ms(20);
    }
    
    // Calculate hard-iron offsets (center of min/max)
    s_calibration.mag_x_offset = (mag_x_max + mag_x_min) / 2.0f;
    s_calibration.mag_y_offset = (mag_y_max + mag_y_min) / 2.0f;
    s_calibration.mag_z_offset = (mag_z_max + mag_z_min) / 2.0f;
    
    // Calculate soft-iron scale factors (normalize to average radius)
    float avg_delta = ((mag_x_max - mag_x_min) + 
                       (mag_y_max - mag_y_min) + 
                       (mag_z_max - mag_z_min)) / 3.0f;
    
    s_calibration.mag_x_scale = avg_delta / (mag_x_max - mag_x_min);
    s_calibration.mag_y_scale = avg_delta / (mag_y_max - mag_y_min);
    s_calibration.mag_z_scale = avg_delta / (mag_z_max - mag_z_min);
    
    s_mag_calibrated = true;
    
    printf("[MPU9250] Magnetometer calibration complete! (%lu samples)\n", samples);
    printf("[MPU9250] Hard-iron (µT): X=%.2f, Y=%.2f, Z=%.2f\n",
           s_calibration.mag_x_offset,
           s_calibration.mag_y_offset,
           s_calibration.mag_z_offset);
    printf("[MPU9250] Soft-iron scale: X=%.3f, Y=%.3f, Z=%.3f\n",
           s_calibration.mag_x_scale,
           s_calibration.mag_y_scale,
           s_calibration.mag_z_scale);
}

void mpu9250_set_calibration(const mpu9250_calibration_t *cal)
{
    memcpy(&s_calibration, cal, sizeof(mpu9250_calibration_t));
    s_accel_gyro_calibrated = true;
    s_mag_calibrated = true;
}

void mpu9250_get_calibration(mpu9250_calibration_t *cal)
{
    memcpy(cal, &s_calibration, sizeof(mpu9250_calibration_t));
}

bool mpu9250_is_calibrated(void)
{
    return s_accel_gyro_calibrated;
}

// ============================================================================
// PUBLIC FUNCTIONS - SENSOR READING
// ============================================================================

void mpu9250_read_raw(mpu9250_raw_data_t *data)
{
    uint8_t buf[14];
    
    // Read 14 bytes starting from ACCEL_XOUT_H
    mpu9250_read_reg(MPU9250_REG_ACCEL_XOUT_H, buf, 14);
    
    // Parse accelerometer (big-endian)
    data->accel_x = buf_to_int16_be(&buf[0]);
    data->accel_y = buf_to_int16_be(&buf[2]);
    data->accel_z = buf_to_int16_be(&buf[4]);
    
    // Parse temperature
    data->temp = buf_to_int16_be(&buf[6]);
    
    // Parse gyroscope (big-endian)
    data->gyro_x = buf_to_int16_be(&buf[8]);
    data->gyro_y = buf_to_int16_be(&buf[10]);
    data->gyro_z = buf_to_int16_be(&buf[12]);
}

bool mpu9250_read_mag_raw(mpu9250_mag_raw_t *data)
{
    if (!s_use_magnetometer) {
        data->mag_x = 0;
        data->mag_y = 0;
        data->mag_z = 0;
        data->overflow = false;
        return false;
    }
    
    // Read from external sensor data (populated by I2C master)
    // Format: ST1, HXL, HXH, HYL, HYH, HZL, HZH, ST2
    uint8_t buf[8];
    mpu9250_read_reg(MPU9250_REG_EXT_SENS_DATA_00, buf, 8);
    
    // Check if data is ready (DRDY bit in ST1)
    if (!(buf[0] & 0x01)) {
        return false;  // Data not ready
    }
    
    // Parse magnetometer data (little-endian)
    data->mag_x = buf_to_int16_le(&buf[1]);
    data->mag_y = buf_to_int16_le(&buf[3]);
    data->mag_z = buf_to_int16_le(&buf[5]);
    
    // Check for overflow (HOFL bit in ST2)
    data->overflow = (buf[7] & 0x08) != 0;
    
    return true;
}

void mpu9250_read_scaled(mpu9250_scaled_data_t *data)
{
    mpu9250_raw_data_t raw;
    mpu9250_mag_raw_t mag;
    
    mpu9250_read_raw(&raw);
    mpu9250_read_mag_raw(&mag);
    
    // Convert accelerometer to g
    data->accel_x = ((float)raw.accel_x / s_accel_scale) - s_calibration.accel_x_offset;
    data->accel_y = ((float)raw.accel_y / s_accel_scale) - s_calibration.accel_y_offset;
    data->accel_z = ((float)raw.accel_z / s_accel_scale) - s_calibration.accel_z_offset;
    
    // Convert gyroscope to °/s
    data->gyro_x = ((float)raw.gyro_x / s_gyro_scale) - s_calibration.gyro_x_offset;
    data->gyro_y = ((float)raw.gyro_y / s_gyro_scale) - s_calibration.gyro_y_offset;
    data->gyro_z = ((float)raw.gyro_z / s_gyro_scale) - s_calibration.gyro_z_offset;
    
    // Convert magnetometer to µT with calibration
    if (s_use_magnetometer) {
        float mx = (float)mag.mag_x * MAG_SCALE_16BIT * s_mag_adj_x;
        float my = (float)mag.mag_y * MAG_SCALE_16BIT * s_mag_adj_y;
        float mz = (float)mag.mag_z * MAG_SCALE_16BIT * s_mag_adj_z;
        
        // Apply hard-iron and soft-iron correction
        data->mag_x = (mx - s_calibration.mag_x_offset) * s_calibration.mag_x_scale;
        data->mag_y = (my - s_calibration.mag_y_offset) * s_calibration.mag_y_scale;
        data->mag_z = (mz - s_calibration.mag_z_offset) * s_calibration.mag_z_scale;
    } else {
        data->mag_x = 0.0f;
        data->mag_y = 0.0f;
        data->mag_z = 0.0f;
    }
    
    // Convert temperature: Temp_degC = ((TEMP_OUT - RoomTemp_Offset)/Temp_Sensitivity) + 21
    // Room temp offset = 0, sensitivity = 333.87
    data->temp = ((float)raw.temp / 333.87f) + 21.0f;
}

float mpu9250_get_temperature(void)
{
    uint8_t buf[2];
    mpu9250_read_reg(MPU9250_REG_TEMP_OUT_H, buf, 2);
    int16_t temp_raw = buf_to_int16_be(buf);
    return ((float)temp_raw / 333.87f) + 21.0f;
}

// ============================================================================
// PUBLIC FUNCTIONS - ORIENTATION
// ============================================================================

void mpu9250_update_orientation(float dt)
{
    mpu9250_scaled_data_t data;
    mpu9250_read_scaled(&data);
    
    // ========================================================================
    // STEP 1: Calculate roll and pitch from accelerometer
    // ========================================================================
    float accel_roll = atan2f(data.accel_y, 
                              sqrtf(data.accel_x * data.accel_x + 
                                    data.accel_z * data.accel_z)) * 180.0f / M_PI;
    
    float accel_pitch = atan2f(-data.accel_x,
                               sqrtf(data.accel_y * data.accel_y + 
                                     data.accel_z * data.accel_z)) * 180.0f / M_PI;
    
    // ========================================================================
    // STEP 2: Integrate gyroscope for all axes
    // ========================================================================
    float gyro_roll_rate = data.gyro_x;
    float gyro_pitch_rate = data.gyro_y;
    float gyro_yaw_rate = data.gyro_z;
    
    // Apply complementary filter for roll and pitch
    // Gyro is trusted more (98%) than accelerometer (2%) for smooth output
    s_roll = COMPLEMENTARY_ALPHA * (s_roll + gyro_roll_rate * dt) + 
             (1.0f - COMPLEMENTARY_ALPHA) * accel_roll;
    
    s_pitch = COMPLEMENTARY_ALPHA * (s_pitch + gyro_pitch_rate * dt) + 
              (1.0f - COMPLEMENTARY_ALPHA) * accel_pitch;
    
    // ========================================================================
    // STEP 3: Calculate yaw from magnetometer (with tilt compensation)
    // ========================================================================
    if (s_use_magnetometer && s_mag_calibrated) {
        // Tilt-compensate magnetometer readings
        float cos_roll = cosf(s_roll * M_PI / 180.0f);
        float sin_roll = sinf(s_roll * M_PI / 180.0f);
        float cos_pitch = cosf(s_pitch * M_PI / 180.0f);
        float sin_pitch = sinf(s_pitch * M_PI / 180.0f);
        
        // Tilt compensation (rotate magnetometer readings to horizontal plane)
        float mag_x_comp = data.mag_x * cos_pitch + 
                           data.mag_y * sin_roll * sin_pitch + 
                           data.mag_z * cos_roll * sin_pitch;
        
        float mag_y_comp = data.mag_y * cos_roll - 
                           data.mag_z * sin_roll;
        
        // Calculate magnetic heading
        float mag_yaw = atan2f(-mag_y_comp, mag_x_comp) * 180.0f / M_PI;
        
        // Apply magnetic declination if set
        mag_yaw += s_mag_declination;
        
        // Normalize to 0-360 range
        if (mag_yaw < 0) mag_yaw += 360.0f;
        if (mag_yaw >= 360.0f) mag_yaw -= 360.0f;
        
        // Integrate gyro for yaw
        float gyro_yaw = s_yaw + gyro_yaw_rate * dt;
        
        // Normalize gyro yaw to 0-360
        while (gyro_yaw < 0) gyro_yaw += 360.0f;
        while (gyro_yaw >= 360.0f) gyro_yaw -= 360.0f;
        
        // Blend gyro yaw with magnetometer yaw
        // Handle wraparound at 0/360 boundary
        float yaw_diff = mag_yaw - gyro_yaw;
        if (yaw_diff > 180.0f) yaw_diff -= 360.0f;
        if (yaw_diff < -180.0f) yaw_diff += 360.0f;
        
        s_yaw = gyro_yaw + MAG_ALPHA * yaw_diff;
        
        // Normalize result
        while (s_yaw < 0) s_yaw += 360.0f;
        while (s_yaw >= 360.0f) s_yaw -= 360.0f;
    } else {
        // No magnetometer: just integrate gyro (will drift over time!)
        s_yaw += gyro_yaw_rate * dt;
        
        // Normalize to 0-360
        while (s_yaw < 0) s_yaw += 360.0f;
        while (s_yaw >= 360.0f) s_yaw -= 360.0f;
    }
}

void mpu9250_get_orientation(mpu9250_orientation_t *orientation)
{
    orientation->roll = s_roll;
    orientation->pitch = s_pitch;
    orientation->yaw = s_yaw;
}

float mpu9250_get_roll(void)
{
    return s_roll;
}

float mpu9250_get_pitch(void)
{
    return s_pitch;
}

float mpu9250_get_yaw(void)
{
    return s_yaw;
}

void mpu9250_reset_orientation(void)
{
    s_roll = 0.0f;
    s_pitch = 0.0f;
    s_yaw = 0.0f;
}

void mpu9250_reset_yaw(void)
{
    s_yaw = 0.0f;
}

// ============================================================================
// PUBLIC FUNCTIONS - SERVO MAPPING UTILITIES
// ============================================================================

uint16_t mpu9250_angle_to_servo_pulse(float angle, 
                                       float angle_min, float angle_max,
                                       uint16_t pulse_min, uint16_t pulse_max)
{
    // Clamp angle to range
    if (angle < angle_min) angle = angle_min;
    if (angle > angle_max) angle = angle_max;
    
    // Linear interpolation
    float normalized = (angle - angle_min) / (angle_max - angle_min);
    uint16_t pulse = pulse_min + (uint16_t)(normalized * (pulse_max - pulse_min));
    
    return pulse;
}

void mpu9250_get_servo_pulses(uint16_t *roll_pulse, 
                               uint16_t *pitch_pulse, 
                               uint16_t *yaw_pulse)
{
    // Standard 180° servo pulse range
    const uint16_t SERVO_MIN = 500;   // 0.5ms
    const uint16_t SERVO_MAX = 2500;  // 2.5ms
    const uint16_t SERVO_MID = 1500;  // 1.5ms (90° position)
    
    // Map roll (-90 to +90) to servo pulse
    // Roll range limited to ±90° for 180° servos
    *roll_pulse = mpu9250_angle_to_servo_pulse(s_roll, -90.0f, 90.0f, SERVO_MIN, SERVO_MAX);
    
    // Map pitch (-90 to +90) to servo pulse
    *pitch_pulse = mpu9250_angle_to_servo_pulse(s_pitch, -90.0f, 90.0f, SERVO_MIN, SERVO_MAX);
    
    // Map yaw (0 to 180) to servo pulse
    // Note: Full 360° yaw can't be represented by 180° servo
    // Map yaw 0-180 to servo range, yaw 180-360 mirrors back
    float yaw_limited = s_yaw;
    if (yaw_limited > 180.0f) {
        yaw_limited = 360.0f - yaw_limited;  // Mirror back
    }
    *yaw_pulse = mpu9250_angle_to_servo_pulse(yaw_limited, 0.0f, 180.0f, SERVO_MIN, SERVO_MAX);
}
