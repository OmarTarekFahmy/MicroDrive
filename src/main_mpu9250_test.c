/**
 * @file main_mpu9250_test.c
 * @brief MPU9250 Driver Test Program for Raspberry Pi Pico
 * 
 * This program tests the MPU9250 9-DOF IMU driver by:
 * 1. Initializing I2C and the MPU9250
 * 2. Verifying WHO_AM_I registers
 * 3. Calibrating the gyroscope and accelerometer
 * 4. Reading and displaying YAW, PITCH, ROLL angles
 * 5. Optionally mirroring rotation to servos
 * 
 * Connect MPU9250:
 *   - SDA -> GPIO 2
 *   - SCL -> GPIO 3
 *   - VCC -> 3.3V
 *   - GND -> GND
 *   - AD0 -> GND (address 0x68)
 */

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"
#include "drivers/mpu9250/mpu9250.h"
#include <stdio.h>
#include <math.h>

// Pico W uses CYW43 for LED control
#define LED_ON()    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1)
#define LED_OFF()   cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0)
#define LED_TOGGLE(state) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, (state))

// ============================================================================
// CONFIGURATION
// ============================================================================

// Enable servo output (set to 0 to disable)
#define ENABLE_SERVO_OUTPUT     1

// Servo GPIO pins (180° positional servos)
#define SERVO_ROLL_PIN          14
#define SERVO_PITCH_PIN         15
#define SERVO_YAW_PIN           18

// Update rate
#define UPDATE_INTERVAL_MS      20      // 50Hz update rate
#define PRINT_INTERVAL_MS       200     // Print every 200ms (5Hz)

// ============================================================================
// SERVO FUNCTIONS
// ============================================================================

#if ENABLE_SERVO_OUTPUT

// PWM configuration for servos
#define SERVO_PWM_FREQ          50      // 50Hz for standard servos
#define SERVO_MIN_US            500     // 0.5ms pulse = 0°
#define SERVO_MAX_US            2500    // 2.5ms pulse = 180°

static uint servo_roll_slice, servo_roll_chan;
static uint servo_pitch_slice, servo_pitch_chan;
static uint servo_yaw_slice, servo_yaw_chan;

/**
 * @brief Initialize a servo on specified GPIO pin
 */
static void servo_init_pin(uint gpio, uint *slice, uint *chan)
{
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    *slice = pwm_gpio_to_slice_num(gpio);
    *chan = pwm_gpio_to_channel(gpio);
    
    // Configure for 50Hz (20ms period)
    // System clock = 125MHz, divider = 125, wrap = 20000
    // Frequency = 125MHz / (125 * 20000) = 50Hz
    pwm_set_clkdiv(*slice, 125.0f);
    pwm_set_wrap(*slice, 20000 - 1);
    pwm_set_enabled(*slice, true);
}

/**
 * @brief Set servo position (0-180 degrees)
 */
static void servo_set_angle(uint slice, uint chan, float angle)
{
    // Clamp angle to 0-180
    if (angle < 0.0f) angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;
    
    // Map angle to pulse width in microseconds
    float pulse_us = SERVO_MIN_US + (angle / 180.0f) * (SERVO_MAX_US - SERVO_MIN_US);
    
    // Convert to PWM level (1us = 1 count at our configuration)
    uint16_t level = (uint16_t)pulse_us;
    pwm_set_chan_level(slice, chan, level);
}

/**
 * @brief Initialize all servos
 */
static void servos_init(void)
{
    printf("[SERVO] Initializing servos...\n");
    servo_init_pin(SERVO_ROLL_PIN, &servo_roll_slice, &servo_roll_chan);
    servo_init_pin(SERVO_PITCH_PIN, &servo_pitch_slice, &servo_pitch_chan);
    servo_init_pin(SERVO_YAW_PIN, &servo_yaw_slice, &servo_yaw_chan);
    
    // Center all servos
    servo_set_angle(servo_roll_slice, servo_roll_chan, 90.0f);
    servo_set_angle(servo_pitch_slice, servo_pitch_chan, 90.0f);
    servo_set_angle(servo_yaw_slice, servo_yaw_chan, 90.0f);
    
    printf("[SERVO] Servos initialized and centered\n");
}

/**
 * @brief Update servo positions based on orientation
 * 
 * Maps IMU angles to servo angles:
 *   Roll:  -90° to +90° -> 0° to 180° servo
 *   Pitch: -90° to +90° -> 0° to 180° servo
 *   Yaw:   -180° to +180° -> 0° to 180° servo
 */
static void servos_update(float roll, float pitch, float yaw)
{
    // Map roll (-90 to +90) to servo (0 to 180)
    float servo_roll = 90.0f + roll;
    
    // Map pitch (-90 to +90) to servo (0 to 180)
    float servo_pitch = 90.0f + pitch;
    
    // Map yaw (-180 to +180) to servo (0 to 180)
    float servo_yaw = 90.0f + (yaw * 0.5f);
    
    servo_set_angle(servo_roll_slice, servo_roll_chan, servo_roll);
    servo_set_angle(servo_pitch_slice, servo_pitch_chan, servo_pitch);
    servo_set_angle(servo_yaw_slice, servo_yaw_chan, servo_yaw);
}

#endif // ENABLE_SERVO_OUTPUT

// ============================================================================
// MAIN PROGRAM
// ============================================================================

int main(void)
{
    // Initialize stdio for USB/UART output
    stdio_init_all();
    
    // Wait for USB serial connection (timeout after 3 seconds)
    for (int i = 0; i < 30 && !stdio_usb_connected(); i++) {
        sleep_ms(100);
    }
    sleep_ms(500);  // Extra delay for terminal to settle
    
    printf("\n");
    printf("========================================\n");
    printf("   MPU9250 9-DOF IMU Test Program\n");
    printf("========================================\n");
    printf("\n");
    
    // Initialize MPU9250
    printf("[INIT] Initializing MPU9250...\n");
    if (!mpu9250_init()) {
        printf("[ERROR] MPU9250 initialization failed!\n");
        printf("[ERROR] Check wiring:\n");
        printf("        SDA -> GPIO 2\n");
        printf("        SCL -> GPIO 3\n");
        printf("        VCC -> 3.3V\n");
        printf("        GND -> GND\n");
        printf("        AD0 -> GND\n");
        
        // Blink LED to indicate error
        while (true) {
            LED_ON();
            sleep_ms(100);
            LED_OFF();
            sleep_ms(100);
        }
    }
    
    printf("[INIT] MPU9250 initialized successfully!\n\n");
    
    // Verify MPU9250 communication
    printf("[TEST] Verifying MPU9250 WHO_AM_I...\n");
    if (mpu9250_is_connected()) {
        printf("[TEST] MPU9250 connection verified!\n");
    } else {
        printf("[WARN] WHO_AM_I mismatch (may be MPU9255 variant)\n");
    }
    
    // Verify magnetometer
    printf("[TEST] Verifying AK8963 magnetometer...\n");
    if (mpu9250_mag_is_connected()) {
        printf("[TEST] AK8963 magnetometer verified!\n");
    } else {
        printf("[WARN] Magnetometer verification failed\n");
    }
    printf("\n");
    
#if ENABLE_SERVO_OUTPUT
    // Initialize servos
    servos_init();
    printf("\n");
#endif
    
    // Calibration
    printf("[CAL] Starting gyroscope/accelerometer calibration...\n");
    printf("[CAL] Keep the sensor STILL and LEVEL!\n");
    sleep_ms(2000);
    
    mpu9250_calibrate_accel_gyro(1000);  // 1000 samples calibration
    printf("[CAL] Calibration complete!\n\n");
    
    // Optional: Magnetometer calibration (requires rotating the sensor)
    printf("[INFO] Magnetometer calibration skipped (use mpu9250_calibrate_magnetometer() if needed)\n");
    printf("[INFO] For best yaw accuracy, perform magnetometer calibration\n\n");
    
    // Main loop
    printf("[RUN] Starting orientation tracking...\n\n");
    
    // Initialize CYW43 for LED control (required for Pico W)
    if (cyw43_arch_init()) {
        printf("[WARN] CYW43 init failed, LED won't work\n");
    }
    
    uint32_t last_print_time = 0;
    uint32_t loop_count = 0;
    
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Update orientation at high rate
        mpu9250_update_orientation(UPDATE_INTERVAL_MS / 1000.0f);
        
        // Get current orientation
        mpu9250_orientation_t orientation;
        mpu9250_get_orientation(&orientation);
        
#if ENABLE_SERVO_OUTPUT
        // Update servos
        servos_update(orientation.roll, orientation.pitch, orientation.yaw);
#endif
        
        // Print at lower rate
        if (now - last_print_time >= PRINT_INTERVAL_MS) {
            last_print_time = now;
            
            // Print orientation on separate line for easy parsing
            printf("YAW: %.2f PITCH: %.2f ROLL: %.2f\n",
                   orientation.yaw, orientation.pitch, orientation.roll);
            
            // Toggle LED to show activity
            LED_TOGGLE(loop_count++ & 1);
        }
        
        sleep_ms(UPDATE_INTERVAL_MS);
    }
    
    return 0;
}
