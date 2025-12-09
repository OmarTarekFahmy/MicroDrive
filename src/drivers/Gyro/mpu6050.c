#include "mpu6050.h"
#include <stdio.h>

// Use I2C1 for MPU6050 (GPIO 2 = SDA, 3 = SCL)
#define MPU_I2C i2c1
#define MPU_SDA_PIN 2
#define MPU_SCL_PIN 3

// Maximum allowed dt (seconds) - reject outliers
#define MAX_DT 0.1f

// Raw sensor data
static int16_t accel_raw[3];
static int16_t gyro_raw[3];
static int16_t temp_raw;

// Gyro bias offsets (deg/s), determined by calibration while sensor is still
static float gx_offset = 0.0f;
static float gy_offset = 0.0f;
static float gz_offset = 0.0f;

// Orientation state (absolute, in degrees)
// Roll  = rotation about X axis (tilting left/right)
// Pitch = rotation about Y axis (tilting forward/backward)
// Yaw   = rotation about Z axis (turning left/right)
static float roll = 0.0f;
static float pitch = 0.0f;
static float yaw = 0.0f;

// Reference orientation (zero point)
static float roll0 = 0.0f;
static float pitch0 = 0.0f;
static float yaw0 = 0.0f;

// Timing for integration
static absolute_time_t last_time;
static bool first_update = true;

// Initialise angle state
static void angles_init(void)
{
    last_time = get_absolute_time();
    roll = pitch = yaw = 0.0f;
    roll0 = pitch0 = yaw0 = 0.0f;
    first_update = true;
}

// Low-level: read all 14 data bytes from sensor
static bool mpu6050_read_all(void)
{
    uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
    uint8_t buf[14];

    // Write start register
    if (i2c_write_blocking(MPU_I2C, MPU6050_ADDR, &reg, 1, true) != 1)
        return false;

    // Read 14 bytes: accel (6) + temp (2) + gyro (6)
    if (i2c_read_blocking(MPU_I2C, MPU6050_ADDR, buf, 14, false) != 14)
        return false;

    accel_raw[0] = (int16_t)((buf[0] << 8) | buf[1]);
    accel_raw[1] = (int16_t)((buf[2] << 8) | buf[3]);
    accel_raw[2] = (int16_t)((buf[4] << 8) | buf[5]);

    temp_raw = (int16_t)((buf[6] << 8) | buf[7]);

    gyro_raw[0] = (int16_t)((buf[8] << 8) | buf[9]);
    gyro_raw[1] = (int16_t)((buf[10] << 8) | buf[11]);
    gyro_raw[2] = (int16_t)((buf[12] << 8) | buf[13]);

    return true;
}

void mpu6050_calibrate_gyro(void)
{
    const int samples = 500;
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_z = 0.0f;

    // Assume sensor is completely still during calibration
    for (int i = 0; i < samples; i++)
    {
        if (!mpu6050_read_all())
        {
            // If a read fails, skip this sample and try again
            continue;
        }

        sum_x += (float)gyro_raw[0];
        sum_y += (float)gyro_raw[1];
        sum_z += (float)gyro_raw[2];

        sleep_ms(2);
    }

    gx_offset = (sum_x / samples) / MPU6050_GYRO_SCALE;
    gy_offset = (sum_y / samples) / MPU6050_GYRO_SCALE;
    gz_offset = (sum_z / samples) / MPU6050_GYRO_SCALE;

    printf("MPU6050: Gyro calibrated. Offsets = %.4f, %.4f, %.4f deg/s\n",
           gx_offset, gy_offset, gz_offset);
}

void mpu6050_init(void)
{
    // Init I2C1
    i2c_init(MPU_I2C, 400000); // 400 kHz
    gpio_set_function(MPU_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(MPU_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(MPU_SDA_PIN);
    gpio_pull_up(MPU_SCL_PIN);
    sleep_ms(100);

    // Wake device
    uint8_t buf[2];

    buf[0] = MPU6050_REG_PWR_MGMT_1;
    buf[1] = 0x00; // clear sleep bit
    i2c_write_blocking(MPU_I2C, MPU6050_ADDR, buf, 2, false);
    sleep_ms(100);

    // Gyro config: ±250°/s (0x00)
    buf[0] = MPU6050_REG_GYRO_CONFIG;
    buf[1] = 0x00;
    i2c_write_blocking(MPU_I2C, MPU6050_ADDR, buf, 2, false);

    // Accel config: ±2g (0x00)
    buf[0] = MPU6050_REG_ACCEL_CONFIG;
    buf[1] = 0x00;
    i2c_write_blocking(MPU_I2C, MPU6050_ADDR, buf, 2, false);

    // Initialise angle integration state
    angles_init();

    printf("MPU6050: Initialised on I2C1 (SDA=2,SCL=3)\n");
}

void mpu6050_update(void)
{
    if (!mpu6050_read_all())
    {
        // On read failure, reset timing to avoid huge dt on next successful read
        last_time = get_absolute_time();
        return;
    }

    // Convert raw values to physical units
    float ax = (float)accel_raw[0] / MPU6050_ACCEL_SCALE; // g
    float ay = (float)accel_raw[1] / MPU6050_ACCEL_SCALE;
    float az = (float)accel_raw[2] / MPU6050_ACCEL_SCALE;

    float gx = (float)gyro_raw[0] / MPU6050_GYRO_SCALE - gx_offset; // deg/s
    float gy = (float)gyro_raw[1] / MPU6050_GYRO_SCALE - gy_offset;
    float gz = (float)gyro_raw[2] / MPU6050_GYRO_SCALE - gz_offset;

    // CALCULATE dt with protection against large values
    absolute_time_t now = get_absolute_time();
    float dt = absolute_time_diff_us(last_time, now) / 1e6f;
    last_time = now;

    // ACCELEROMETER → TILT ANGLES
    // Roll: rotation around X axis (tilting left/right)
    float acc_roll = atan2f(ay, az) * 57.29578f;
    // Pitch: rotation around Y axis (tilting forward/backward) - negated so up is positive
    float acc_pitch = -atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.29578f;

    // On first update or after long delay, initialize from accelerometer
    if (first_update || dt > MAX_DT)
    {
        roll = acc_roll;
        pitch = acc_pitch;
        // Don't reset yaw, keep integrating from current value
        first_update = false;
        dt = 0.0f; // Skip gyro integration this cycle
    }

    // GYRO INTEGRATION (always runs, even after first_update init)
    // MPU6050 gyro axes: gx = rotation rate around X, gy = around Y, gz = around Z
    // Match gyro integration to the same axis definitions as accelerometer
    float roll_gyro = roll + gx * dt;   // gx rotates around X axis = roll
    float pitch_gyro = pitch - gy * dt; // gy rotates around Y axis = pitch (negated)
    yaw += gz * dt;                     // gz rotates around Z axis = yaw

    // COMPLEMENTARY FILTER
    // Trust gyro for fast changes, accelerometer for long-term stability
    const float alpha = MPU6050_ALPHA;

    roll = alpha * roll_gyro + (1.0f - alpha) * acc_roll;
    pitch = alpha * pitch_gyro + (1.0f - alpha) * acc_pitch;
    // Yaw has no accelerometer correction (would need magnetometer) - already integrated above
}

void mpu6050_set_reference(void)
{
    pitch0 = pitch;
    roll0 = roll;
    yaw0 = yaw;
}

void mpu6050_get_deltas(float *dRoll, float *dPitch, float *dYaw)
{
    float d_pitch = pitch - pitch0;
    float d_roll = roll - roll0;
    float d_yaw = yaw - yaw0;

    if (dRoll)
        *dRoll = d_roll;
    if (dPitch)
        *dPitch = d_pitch;
    if (dYaw)
        *dYaw = d_yaw;
}

float mpu6050_get_temperature_c(void)
{
    return (float)temp_raw / 340.0f + 36.53f;
}

void mpu6050_get_raw_accel(int16_t *ax, int16_t *ay, int16_t *az)
{
    if (ax)
        *ax = accel_raw[0];
    if (ay)
        *ay = accel_raw[1];
    if (az)
        *az = accel_raw[2];
}

void mpu6050_get_raw_gyro(int16_t *gx, int16_t *gy, int16_t *gz)
{
    if (gx)
        *gx = gyro_raw[0];
    if (gy)
        *gy = gyro_raw[1];
    if (gz)
        *gz = gyro_raw[2];
}
