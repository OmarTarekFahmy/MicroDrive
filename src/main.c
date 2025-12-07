#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "drivers/Gyro/mpu6050.h"
#include "drivers/mpu6050/MPU6050.h"  // Advanced MPU6050 library
#include "drivers/lcd/lcd_i2c.h"
#include "drivers/servo/servo.h"
#include "drivers/touch/touch_sensor.h"
#include "drivers/buzzer/buzzer.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

// LCD I2C on I2C0 (GPIO 0 = SDA, GPIO 1 = SCL)
#define LCD_SDA_PIN 0
#define LCD_SCL_PIN 1
#define LCD_I2C_ADDR 0x27

// Servo GPIO pins - MG996R servos
#define SERVO_X_PIN 14  // 360° servo for X-axis
#define SERVO_Y_PIN 15  // 360° servo for Y-axis
#define SERVO_Z_PIN 16  // 180° servo for Z-axis

// Touch sensor pins (GPIO 9-12)
#define TOUCH_1_PIN 9
#define TOUCH_2_PIN 10
#define TOUCH_3_PIN 11
#define TOUCH_4_PIN 12

// Buzzer pin (GPIO 13)
#define BUZZER_PIN 13

// Electromagnet lock pin (GPIO 17) - connect through transistor/relay!
#define LOCK_PIN 17

// LED pins
#define LED_RED_PIN 18
#define LED_GREEN_PIN 19

// Lock sequence configuration
#define SEQUENCE_LENGTH 4
#define LOCK_OPEN_TIME_MS 5000  // Lock stays open for 5 seconds

// Secret sequence: Touch 1, 3, 2, 4 (like musical notes)
static const int8_t SECRET_SEQUENCE[SEQUENCE_LENGTH] = {0, 2, 1, 3};  // 0-indexed

// Sequence tracking
static int8_t entered_sequence[SEQUENCE_LENGTH];
static uint8_t sequence_index = 0;
static bool system_unlocked = false;  // Track if system has been unlocked

#define UPDATE_INTERVAL_MS 50

// Initialize the electromagnet lock pin
void electromagnet_init(void) {
    gpio_init(LOCK_PIN);
    gpio_set_dir(LOCK_PIN, GPIO_OUT);
    gpio_put(LOCK_PIN, 0);  // Lock closed (LOW)
}

// Open the lock with initial nudge pulse
void electromagnet_open(void) {
    // Nudge: rapid on-off pulses to help electromagnet engage
    for (int i = 0; i < 5; i++) {
        gpio_put(LOCK_PIN, 1);
        sleep_ms(50);
        gpio_put(LOCK_PIN, 0);
        sleep_ms(20);
    }
    
    // Now keep it ON
    gpio_put(LOCK_PIN, 1);
    printf("*** LOCK OPENED ***\n");
}

// Close the lock
void electromagnet_close(void) {
    gpio_put(LOCK_PIN, 0);  // LOW = electromagnet OFF = lock closed
    printf("*** LOCK CLOSED ***\n");
}

// Initialize LEDs
void leds_init(void) {
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    
    // Start with RED on (locked state)
    gpio_put(LED_RED_PIN, 1);
    gpio_put(LED_GREEN_PIN, 0);
}

// Set LEDs for locked state (RED on, GREEN off)
void leds_locked(void) {
    gpio_put(LED_RED_PIN, 1);
    gpio_put(LED_GREEN_PIN, 0);
}

// Set LEDs for unlocked state (RED off, GREEN on)
void leds_unlocked(void) {
    gpio_put(LED_RED_PIN, 0);
    gpio_put(LED_GREEN_PIN, 1);
}

// Check sequence and return true if correct
bool check_sequence(void) {
    if (sequence_index == SEQUENCE_LENGTH) {
        // Check if sequence matches
        bool match = true;
        for (int i = 0; i < SEQUENCE_LENGTH; i++) {
            if (entered_sequence[i] != SECRET_SEQUENCE[i]) {
                match = false;
                break;
            }
        }
        
        // Reset sequence for next attempt
        sequence_index = 0;
        memset(entered_sequence, -1, sizeof(entered_sequence));
        
        if (match) {
            printf("Correct sequence!\n");
            return true;
        } else {
            // Wrong sequence
            printf("Wrong sequence! Try again.\n");
            
            // Play error tone
            buzzer_beep(200, 300);
            return false;
        }
    }
    return false;
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    
    printf("=== Celestial Lock System ===\n\n");
    
    // Initialize LCD first (keep it blank initially)
    lcd_i2c_init(i2c0, LCD_SDA_PIN, LCD_SCL_PIN, LCD_I2C_ADDR);
    lcd_i2c_clear();
    printf("LCD initialized (blank)\n");
    
    // Initialize touch sensors
    touch_init_all(TOUCH_1_PIN, TOUCH_2_PIN, TOUCH_3_PIN, TOUCH_4_PIN);
    printf("Touch sensors: GPIO %d, %d, %d, %d\n", 
           TOUCH_1_PIN, TOUCH_2_PIN, TOUCH_3_PIN, TOUCH_4_PIN);
    
    // Initialize buzzer
    buzzer_init(BUZZER_PIN);
    printf("Buzzer: GPIO %d\n", BUZZER_PIN);
    
    // Initialize LEDs (RED on at start)
    leds_init();
    printf("LEDs: RED=GPIO %d, GREEN=GPIO %d\n", LED_RED_PIN, LED_GREEN_PIN);
    
    // Initialize electromagnet lock
    electromagnet_init();
    printf("Lock: GPIO %d\n", LOCK_PIN);
    printf("Secret sequence: Touch 1-3-2-4\n\n");
    
    // Initialize sequence tracking
    memset(entered_sequence, -1, sizeof(entered_sequence));
    
    char line1[17];
    char line2[17];
    static int8_t last_touched = -1;
    
    printf("=== PHASE 1: Waiting for unlock sequence ===\n");
    
//    ========== PHASE 1: LOCK PHASE - Wait for correct sequence ==========
    while (!system_unlocked) {
        int8_t touched = touch_get_pressed();
        
        if (touched >= 0) {
            buzzer_play_touch_tone(touched);
            
            if (touched != last_touched) {
                printf("Touch %d pressed\n", touched + 1);
                
                // Add to sequence
                entered_sequence[sequence_index] = touched;
                sequence_index++;
                
                // Check if sequence complete and correct
                if (check_sequence()) {
                    system_unlocked = true;
                }
            }
            last_touched = touched;
        } else {
            buzzer_stop();
            last_touched = -1;
        }
        
        sleep_ms(50);
    }
    
    // ========== CORRECT SEQUENCE ENTERED ==========
    printf("\n=== PHASE 2: System Unlocked ===\n");
    
    // Switch to green LED (stays green permanently)
    leds_unlocked();
    
    // Play success melody
    buzzer_beep(523, 100);  // C5
    buzzer_beep(659, 100);  // E5
    buzzer_beep(784, 200);  // G5
    
    // Open electromagnet
    electromagnet_open();
    
    // Display "Celestial Lock Active" message for 5 seconds
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Celestial");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Lock Active.");
    sleep_ms(2500);
    
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("MPU6050 Test");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Initializing...");
    sleep_ms(1000);
    
    // ========== PHASE 3: MPU6050 GYROSCOPE TEST ==========
    printf("\n=== PHASE 3: MPU6050 Gyroscope Test ===\n");
    
    // Initialize MPU6050 using I2C1 (GPIO 2 = SDA, GPIO 3 = SCL)
    printf("Initializing I2C1 on GPIO 2 (SDA) and GPIO 3 (SCL)...\n");
    i2c_init(i2c1, 400000);  // 400kHz I2C
    gpio_set_function(2, GPIO_FUNC_I2C);  // SDA
    gpio_set_function(3, GPIO_FUNC_I2C);  // SCL
    gpio_pull_up(2);
    gpio_pull_up(3);
    sleep_ms(100);  // Give I2C time to stabilize
    
    // Scan I2C bus to find devices
    printf("Scanning I2C bus...\n");
    bool found_device = false;
    uint8_t found_addr = 0;
    
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t data;
        int ret = i2c_read_blocking(i2c1, addr, &data, 1, false);
        if (ret >= 0) {
            printf("Found I2C device at address 0x%02X\n", addr);
            if (addr == 0x68 || addr == 0x69) {
                found_device = true;
                found_addr = addr;
            }
        }
    }
    
    if (!found_device) {
        printf("ERROR: No MPU6050 found on I2C bus!\n");
        printf("Check wiring:\n");
        printf("  MPU6050 VCC -> Pico 3.3V\n");
        printf("  MPU6050 GND -> Pico GND\n");
        printf("  MPU6050 SDA -> Pico GPIO 2\n");
        printf("  MPU6050 SCL -> Pico GPIO 3\n");
        lcd_i2c_clear();
        lcd_i2c_set_cursor(0, 0);
        lcd_i2c_print("I2C Scan FAIL");
        lcd_i2c_set_cursor(1, 0);
        lcd_i2c_print("Check wiring");
        while(1) sleep_ms(1000);
    }
    
    printf("Using MPU6050 at address 0x%02X\n", found_addr);
    
    // Initialize MPU6050 struct with detected address
    mpu6050_t mpu = mpu6050_init(i2c1, found_addr);
    sleep_ms(50);
    
    // Try to initialize
    uint8_t who_am_i = mpu6050_who_am_i(&mpu);
    printf("WHO_AM_I register = 0x%02X\n", who_am_i);
    
    // Identify the sensor type
    const char* sensor_type = "Unknown";
    if (who_am_i == 0x68) sensor_type = "MPU6050";
    else if (who_am_i == 0x70) sensor_type = "MPU6500";
    else if (who_am_i == 0x71) sensor_type = "MPU9250";
    else if (who_am_i == 0x72) sensor_type = "ICM-20608";
    printf("Detected sensor: %s\n", sensor_type);
    
    if (!mpu6050_begin(&mpu)) {
        printf("MPU6050 initialization failed!\n");
        printf("WHO_AM_I = 0x%02X (expected 0x68/0x70/0x71/0x72)\n", who_am_i);
        lcd_i2c_clear();
        lcd_i2c_set_cursor(0, 0);
        lcd_i2c_print("IMU INIT ERR");
        lcd_i2c_set_cursor(1, 0);
        snprintf(line2, sizeof(line2), "ID=0x%02X bad", who_am_i);
        lcd_i2c_print(line2);
        while(1) sleep_ms(1000);
    }
    
    printf("%s connected successfully!\n", sensor_type);
    
    // Configure MPU6050 for optimal performance
    mpu6050_set_scale(&mpu, MPU6050_SCALE_500DPS);      // ±500°/s gyro range
    mpu6050_set_range(&mpu, MPU6050_RANGE_4G);          // ±4g accel range
    mpu6050_set_dlpf_mode(&mpu, MPU6050_DLPF_3);        // Low-pass filter ~40Hz
    
    // Enable measurements
    mpu6050_set_gyroscope_measuring(&mpu, 1);
    mpu6050_set_accelerometer_measuring(&mpu, 1);
    mpu6050_set_temperature_measuring(&mpu, 1);
    
    printf("Performing initial calibration (keep sensor still)...\n");
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Calibrating...");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Keep still!");
    sleep_ms(1000);
    
    // Initial calibration (device must be stationary)
    mpu6050_calibrate_gyro(&mpu, 100);
    mpu6050_set_threshold(&mpu, 3);
    
    printf("Calibration complete!\n");
    printf("Touch 1 = Recalibrate | Touch 2-4 = Reserved\n\n");
    
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Ready! T1=Cal");
    sleep_ms(1500);
    lcd_i2c_clear();
    
    printf("Starting MPU6050 test loop...\n\n");
    
    last_touched = -1;
    bool needs_recalibration = false;
    
    // ========== MAIN TEST LOOP ==========
    while (1) {
        // Check for touch input
        int8_t touched = touch_get_pressed();
        
        if (touched >= 0 && touched != last_touched) {
            if (touched == 0) {  // Touch 1 pressed - recalibrate
                needs_recalibration = true;
                printf("\n*** Recalibration requested - keep sensor still! ***\n");
                buzzer_beep(800, 100);
            }
            last_touched = touched;
        } else if (touched < 0) {
            last_touched = -1;
        }
        
        // Perform recalibration if requested
        if (needs_recalibration) {
            lcd_i2c_clear();
            lcd_i2c_set_cursor(0, 0);
            lcd_i2c_print("Recalibrating..");
            lcd_i2c_set_cursor(1, 0);
            lcd_i2c_print("Keep still!");
            
            mpu6050_calibrate_gyro(&mpu, 100);
            mpu6050_set_threshold(&mpu, 3);
            
            printf("Recalibration complete!\n\n");
            buzzer_beep(1000, 150);
            
            lcd_i2c_clear();
            needs_recalibration = false;
            sleep_ms(500);
        }
        
        // Read all sensor data
        mpu6050_event(&mpu);
        
        // Get gyroscope data (degrees/second)
        mpu6050_vectorf_t *gyro = mpu6050_get_gyroscope(&mpu);
        
        // Get accelerometer data (m/s²)
        mpu6050_vectorf_t *accel = mpu6050_get_accelerometer(&mpu);
        
        // Get temperature
        float temp = mpu6050_get_temperature_c(&mpu);
        
        if (gyro && accel) {
            // Display on LCD (rotate between different views)
            static uint8_t display_mode = 0;
            static uint32_t mode_counter = 0;
            
            mode_counter++;
            if (mode_counter >= 40) {  // Switch every 2 seconds (40 * 50ms)
                display_mode = (display_mode + 1) % 3;
                mode_counter = 0;
            }
            
            if (display_mode == 0) {
                // Show gyroscope (rotation rates)
                snprintf(line1, sizeof(line1), "GX:%+5.1f GY:%+5.1f", gyro->x, gyro->y);
                snprintf(line2, sizeof(line2), "GZ:%+5.1f d/s", gyro->z);
            } else if (display_mode == 1) {
                // Show accelerometer
                snprintf(line1, sizeof(line1), "AX:%+5.1f AY:%+5.1f", accel->x, accel->y);
                snprintf(line2, sizeof(line2), "AZ:%+5.1f m/s2", accel->z);
            } else {
                // Show temperature
                snprintf(line1, sizeof(line1), "Temperature:");
                snprintf(line2, sizeof(line2), "%.1f C", temp);
            }
            
            lcd_i2c_set_cursor(0, 0);
            lcd_i2c_print(line1);
            lcd_i2c_set_cursor(1, 0);
            lcd_i2c_print(line2);
            
            // Detailed debug output
            printf("Gyro: X=%+6.2f Y=%+6.2f Z=%+6.2f deg/s | ", 
                   gyro->x, gyro->y, gyro->z);
            printf("Accel: X=%+6.2f Y=%+6.2f Z=%+6.2f m/s² | ", 
                   accel->x, accel->y, accel->z);
            printf("Temp: %.1f°C\n", temp);
        }
        
        sleep_ms(UPDATE_INTERVAL_MS);
    }
    
    return 0;
}