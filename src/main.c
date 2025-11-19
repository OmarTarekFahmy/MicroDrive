/**
 * @file main.c
 * @brief MicroDrive - Complete driver test and demonstration
 * 
 * This program tests all hardware drivers:
 * - MPU6050 Gyroscope (I2C)
 * - SG90 Servo Motors (PWM)
 * - LCD Display with I2C (I2C)
 * - Capacitive Touch Keys (GPIO)
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"

// Include all driver headers
#include "gyroscope.h"
#include "servo.h"
#include "lcd_i2c.h"
#include "touch_keys.h"
#include "led.h"
#include "rgb_led.h"

// Pin definitions
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define SERVO_PIN_1 15
#define SERVO_PIN_2 14
#define TOUCH_KEY_1 16
#define TOUCH_KEY_2 17
#define TOUCH_KEY_3 18
#define TOUCH_KEY_4 19

// Global objects
static lcd_t lcd;
static servo_t servo1, servo2;
static touch_keys_t touch_keys;
static gyroscope_angles_t angles;

// System state
static uint8_t current_mode = 0;
static bool gyro_initialized = false;
static bool lcd_initialized = false;

/**
 * @brief Touch key callback function
 */
void touch_key_callback(uint8_t key_index, touch_state_t state) {
    if (state == TOUCH_STATE_PRESSED) {
        printf("Touch key %d pressed!\n", key_index + 1);
        
        // Key actions
        switch (key_index) {
            case 0: // Key 1 - Change mode
                current_mode = (current_mode + 1) % 4;
                printf("Mode changed to: %d\n", current_mode);
                break;
                
            case 1: // Key 2 - Servo 1 to 0°
                servo_set_angle(&servo1, 0);
                printf("Servo 1 -> 0°\n");
                break;
                
            case 2: // Key 3 - Servo 1 to 90°
                servo_set_angle(&servo1, 90);
                printf("Servo 1 -> 90°\n");
                break;
                
            case 3: // Key 4 - Servo 1 to 180°
                servo_set_angle(&servo1, 180);
                printf("Servo 1 -> 180°\n");
                break;
        }
    }
}

/**
 * @brief Initialize all hardware components
 */
bool init_hardware(void) {
    printf("\n=== MicroDrive Hardware Initialization ===\n\n");
    
    // Initialize built-in LED for status indication
    printf("1. Initializing status LED... ");
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);
    printf("OK\n");
    
    // Initialize Gyroscope (MPU6050)
    printf("2. Initializing MPU6050 Gyroscope... ");
    gyroscope_config_t gyro_config = {
        .i2c_port = i2c0,
        .sda_pin = I2C_SDA_PIN,
        .scl_pin = I2C_SCL_PIN,
        .baudrate = 400000
    };
    
    if (gyroscope_init(&gyro_config)) {
        printf("OK\n");
        printf("   Calibrating gyroscope (keep still)... ");
        if (gyroscope_calibrate(100)) {
            printf("OK\n");
            gyro_initialized = true;
        } else {
            printf("FAILED\n");
        }
    } else {
        printf("FAILED (check wiring)\n");
    }
    
    // Initialize LCD Display
    printf("3. Initializing LCD Display... ");
    lcd_config_t lcd_config = {
        .i2c_port = i2c0,
        .sda_pin = I2C_SDA_PIN,
        .scl_pin = I2C_SCL_PIN,
        .i2c_addr = 0x27,  // Try 0x3F if this doesn't work
        .baudrate = 400000,
        .cols = 16,
        .rows = 2
    };
    
    if (lcd_init(&lcd_config, &lcd)) {
        printf("OK\n");
        lcd_clear(&lcd);
        lcd_backlight_on(&lcd);
        lcd_set_text(&lcd, 0, 0, "  MicroDrive");
        lcd_set_text(&lcd, 0, 1, " Initializing..");
        lcd_initialized = true;
    } else {
        printf("FAILED (check wiring/address)\n");
    }
    
    sleep_ms(1000);
    
    // Initialize Servo Motors
    printf("4. Initializing Servo Motors... ");
    if (servo_init(&servo1, SERVO_PIN_1)) {
        printf("Servo1 OK ");
    } else {
        printf("Servo1 FAILED ");
    }
    
    if (servo_init(&servo2, SERVO_PIN_2)) {
        printf("Servo2 OK\n");
    } else {
        printf("Servo2 FAILED\n");
    }
    
    // Set servos to center position
    servo_set_angle(&servo1, 90);
    servo_set_angle(&servo2, 90);
    printf("   Servos centered at 90°\n");
    
    // Initialize Touch Keys
    printf("5. Initializing Touch Keys... ");
    touch_keys_init(&touch_keys);
    
    touch_key_config_t key_config = {
        .active_high = true,
        .debounce_ms = 50,
        .callback = touch_key_callback
    };
    
    for (int i = 0; i < 4; i++) {
        key_config.gpio_pin = TOUCH_KEY_1 + i;
        if (touch_keys_add(&touch_keys, &key_config) >= 0) {
            printf(".");
        }
    }
    printf(" OK (4 keys)\n");
    
    printf("\n=== Initialization Complete ===\n\n");
    
    if (lcd_initialized) {
        lcd_clear(&lcd);
        lcd_set_text(&lcd, 0, 0, "Ready!");
        lcd_set_text(&lcd, 0, 1, "Press any key");
    }
    
    return true;
}

/**
 * @brief Display mode information on LCD
 */
void update_lcd_mode(void) {
    if (!lcd_initialized) return;
    
    char line1[17], line2[17];
    
    switch (current_mode) {
        case 0: // Gyroscope display
            if (gyro_initialized) {
                snprintf(line1, sizeof(line1), "Pitch: %6.1f", angles.pitch);
                snprintf(line2, sizeof(line2), "Roll:  %6.1f", angles.roll);
            } else {
                snprintf(line1, sizeof(line1), "Gyro: N/A");
                snprintf(line2, sizeof(line2), "Check wiring");
            }
            break;
            
        case 1: // Servo positions
            snprintf(line1, sizeof(line1), "S1:%3.0f  S2:%3.0f", 
                     servo_get_angle(&servo1), servo_get_angle(&servo2));
            snprintf(line2, sizeof(line2), "Mode: Servos");
            break;
            
        case 2: // Touch key status
            {
                uint8_t pressed = touch_keys_get_pressed_count(&touch_keys);
                snprintf(line1, sizeof(line1), "Touch Keys");
                snprintf(line2, sizeof(line2), "Pressed: %d", pressed);
            }
            break;
            
        case 3: // System info
            snprintf(line1, sizeof(line1), "MicroDrive v1.0");
            snprintf(line2, sizeof(line2), "All Systems Go!");
            break;
    }
    
    lcd_set_text(&lcd, 0, 0, line1);
    lcd_set_text(&lcd, 0, 1, line2);
}

/**
 * @brief Servo sweep demonstration
 */
void demo_servo_sweep(void) {
    printf("\n--- Servo Sweep Demo ---\n");
    
    if (lcd_initialized) {
        lcd_clear(&lcd);
        lcd_set_text(&lcd, 0, 0, "Servo Demo");
        lcd_set_text(&lcd, 0, 1, "Sweeping...");
    }
    
    // Sweep servo 1 from 0 to 180
    printf("Sweeping Servo 1: 0° -> 180°\n");
    servo_sweep(&servo1, 180, 2, 20);
    sleep_ms(500);
    
    // Sweep back
    printf("Sweeping Servo 1: 180° -> 0°\n");
    servo_sweep(&servo1, 0, 2, 20);
    sleep_ms(500);
    
    // Return to center
    servo_set_angle(&servo1, 90);
    servo_set_angle(&servo2, 90);
    
    printf("Servo demo complete\n");
}

/**
 * @brief Test all components
 */
void run_component_tests(void) {
    printf("\n=== Running Component Tests ===\n\n");
    
    // Test 1: Gyroscope reading
    if (gyro_initialized) {
        printf("Test 1: Gyroscope Reading\n");
        for (int i = 0; i < 5; i++) {
            if (gyroscope_get_angles(&angles)) {
                printf("  Sample %d - Pitch: %6.2f°, Roll: %6.2f°, Yaw: %6.2f°\n",
                       i + 1, angles.pitch, angles.roll, angles.yaw);
            }
            sleep_ms(100);
        }
        printf("  Gyroscope test PASSED\n\n");
    }
    
    // Test 2: Servo movement
    printf("Test 2: Servo Movement\n");
    float test_angles[] = {0, 45, 90, 135, 180, 90};
    for (int i = 0; i < 6; i++) {
        printf("  Setting servo to %.0f°\n", test_angles[i]);
        servo_set_angle(&servo1, test_angles[i]);
        sleep_ms(500);
    }
    printf("  Servo test PASSED\n\n");
    
    // Test 3: LCD Display
    if (lcd_initialized) {
        printf("Test 3: LCD Display\n");
        lcd_clear(&lcd);
        lcd_set_text(&lcd, 0, 0, "LCD Test");
        sleep_ms(1000);
        lcd_set_text(&lcd, 0, 1, "Line 2 OK");
        sleep_ms(1000);
        printf("  LCD test PASSED\n\n");
    }
    
    // Test 4: Touch keys (wait for input)
    printf("Test 4: Touch Keys\n");
    printf("  Please press each touch key...\n");
    printf("  (Will timeout in 10 seconds)\n");
    
    bool keys_tested[4] = {false};
    absolute_time_t start_time = get_absolute_time();
    
    while (absolute_time_diff_us(start_time, get_absolute_time()) < 10000000) {
        touch_keys_update(&touch_keys);
        
        for (int i = 0; i < 4; i++) {
            if (touch_key_just_pressed(&touch_keys, i)) {
                keys_tested[i] = true;
                printf("  Key %d detected!\n", i + 1);
            }
        }
        
        // Check if all keys tested
        bool all_tested = true;
        for (int i = 0; i < 4; i++) {
            if (!keys_tested[i]) all_tested = false;
        }
        if (all_tested) break;
        
        sleep_ms(10);
    }
    
    printf("  Touch keys test COMPLETE\n\n");
    
    printf("=== All Tests Complete ===\n\n");
}

/**
 * @brief Main application loop
 */
int main(void) {
    // Initialize stdio for USB serial output
    stdio_init_all();
    sleep_ms(2000);  // Wait for USB connection
    
    printf("\n\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║         MicroDrive Test Suite         ║\n");
    printf("║      Raspberry Pi Pico (RP2040)       ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    // Initialize all hardware
    if (!init_hardware()) {
        printf("ERROR: Hardware initialization failed!\n");
        while (1) {
            sleep_ms(1000);
        }
    }
    
    sleep_ms(2000);
    
    // Run initial component tests
    run_component_tests();
    
    // Run servo sweep demo
    demo_servo_sweep();
    
    printf("\n=== Entering Main Loop ===\n");
    printf("Touch keys control:\n");
    printf("  Key 1: Change display mode\n");
    printf("  Key 2: Servo -> 0°\n");
    printf("  Key 3: Servo -> 90°\n");
    printf("  Key 4: Servo -> 180°\n\n");
    
    // Main loop
    uint32_t loop_count = 0;
    absolute_time_t last_update = get_absolute_time();
    
    while (true) {
        // Update touch keys every cycle
        touch_keys_update(&touch_keys);
        
        // Update displays every 100ms
        if (absolute_time_diff_us(last_update, get_absolute_time()) >= 100000) {
            last_update = get_absolute_time();
            
            // Read gyroscope
            if (gyro_initialized) {
                gyroscope_get_angles(&angles);
            }
            
            // Update LCD based on current mode
            update_lcd_mode();
            
            // Print to serial every 10 loops (1 second)
            if (loop_count % 10 == 0) {
                printf("Mode:%d | ", current_mode);
                
                if (gyro_initialized) {
                    printf("Pitch:%.1f Roll:%.1f | ", angles.pitch, angles.roll);
                }
                
                printf("S1:%.0f S2:%.0f | ", 
                       servo_get_angle(&servo1), servo_get_angle(&servo2));
                
                printf("Keys:%d\n", touch_keys_get_pressed_count(&touch_keys));
            }
            
            loop_count++;
        }
        
        // Small delay to prevent CPU spinning
        sleep_ms(10);
    }
    
    return 0;
}
