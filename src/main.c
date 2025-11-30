#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "drivers/Gyro/mpu6050.h"
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
    
    // ========== PHASE 1: LOCK PHASE - Wait for correct sequence ==========
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
    lcd_i2c_print("Align Cube.");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Yaw:90 P:90 R:90");
    sleep_ms(2500);
    
    // Now initialize gyro and servos
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Initializing");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Gyroscope...");
    
    // Initialize MPU6050 gyro
    mpu6050_setup();
    printf("MPU6050 initialized\n");
    
    // Initialize servos: X and Y are 360°, Z is 180°
    servo_init(SERVO_X, SERVO_X_PIN, SERVO_TYPE_360);
    servo_init(SERVO_Y, SERVO_Y_PIN, SERVO_TYPE_360);
    servo_init(SERVO_Z, SERVO_Z_PIN, SERVO_TYPE_180);
    printf("Servos initialized\n");
    
    // Center all servos
    servo_center_all();
    
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Ready!          ");
    sleep_ms(1000);
    
    lcd_i2c_clear();
    
    printf("Starting gyro-servo control...\n\n");
    
    // ========== PHASE 3: ACTIVE GYRO/SERVO LOOP ==========
    while (1) {
        // Update gyro readings
        mpu6050_update();
        
        // Get angles from gyro (-90 to +90 range)
        float gyro_x = mpu6050_get_angle_x();
        float gyro_y = mpu6050_get_angle_y();
        float gyro_z = mpu6050_get_angle_z();
        
        // Set servo positions directly from gyro angles
        servo_set_angle(SERVO_X, gyro_x);
        servo_set_angle(SERVO_Y, gyro_y);
        servo_set_angle(SERVO_Z, gyro_z);
        
        // Format for LCD - show as Yaw/Pitch/Roll
        snprintf(line1, sizeof(line1), "Y:%+4.0f P:%+4.0f", gyro_z, gyro_x);
        snprintf(line2, sizeof(line2), "R:%+4.0f deg", gyro_y);
        
        // Update LCD
        lcd_i2c_set_cursor(0, 0);
        lcd_i2c_print(line1);
        lcd_i2c_set_cursor(1, 0);
        lcd_i2c_print(line2);
        
        // Debug output
        printf("Yaw:%+6.1f Pitch:%+6.1f Roll:%+6.1f\n", gyro_z, gyro_x, gyro_y);
        
        sleep_ms(UPDATE_INTERVAL_MS);
    }
    
    return 0;
}
