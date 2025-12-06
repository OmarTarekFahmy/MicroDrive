// #include "pico/stdlib.h"
// #include "hardware/i2c.h"
// #include "drivers/Gyro/mpu6050.h"
// #include "drivers/lcd/lcd_i2c.h"
// #include "drivers/servo/servo.h"
// #include "drivers/touch/touch_sensor.h"
// #include "drivers/buzzer/buzzer.h"
// #include <stdio.h>
// #include <math.h>
// #include <string.h>

// // LCD I2C on I2C0 (GPIO 0 = SDA, GPIO 1 = SCL)
// #define LCD_SDA_PIN 0
// #define LCD_SCL_PIN 1
// #define LCD_I2C_ADDR 0x27

// // Servo GPIO pins - MG996R servos
// #define SERVO_X_PIN 14  // 360° servo for X-axis
// #define SERVO_Y_PIN 15  // 360° servo for Y-axis
// #define SERVO_Z_PIN 16  // 180° servo for Z-axis

// // Touch sensor pins (GPIO 9-12)
// #define TOUCH_1_PIN 9
// #define TOUCH_2_PIN 10
// #define TOUCH_3_PIN 11
// #define TOUCH_4_PIN 12

// // Buzzer pin (GPIO 13)
// #define BUZZER_PIN 13

// // Electromagnet lock pin (GPIO 17) - connect through transistor/relay!
// #define LOCK_PIN 17

// // LED pins
// #define LED_RED_PIN 18
// #define LED_GREEN_PIN 19

// // Lock sequence configuration
// #define SEQUENCE_LENGTH 4
// #define LOCK_OPEN_TIME_MS 5000  // Lock stays open for 5 seconds

// // Secret sequence: Touch 1, 3, 2, 4 (like musical notes)
// static const int8_t SECRET_SEQUENCE[SEQUENCE_LENGTH] = {0, 2, 1, 3};  // 0-indexed

// // Sequence tracking
// static int8_t entered_sequence[SEQUENCE_LENGTH];
// static uint8_t sequence_index = 0;
// static bool system_unlocked = false;  // Track if system has been unlocked

// #define UPDATE_INTERVAL_MS 50

// // Initialize the electromagnet lock pin
// void electromagnet_init(void) {
//     gpio_init(LOCK_PIN);
//     gpio_set_dir(LOCK_PIN, GPIO_OUT);
//     gpio_put(LOCK_PIN, 0);  // Lock closed (LOW)
// }

// // Open the lock with initial nudge pulse
// void electromagnet_open(void) {
//     // Nudge: rapid on-off pulses to help electromagnet engage
//     for (int i = 0; i < 5; i++) {
//         gpio_put(LOCK_PIN, 1);
//         sleep_ms(50);
//         gpio_put(LOCK_PIN, 0);
//         sleep_ms(20);
//     }
    
//     // Now keep it ON
//     gpio_put(LOCK_PIN, 1);
//     printf("*** LOCK OPENED ***\n");
// }

// // Close the lock
// void electromagnet_close(void) {
//     gpio_put(LOCK_PIN, 0);  // LOW = electromagnet OFF = lock closed
//     printf("*** LOCK CLOSED ***\n");
// }

// // Initialize LEDs
// void leds_init(void) {
//     gpio_init(LED_RED_PIN);
//     gpio_set_dir(LED_RED_PIN, GPIO_OUT);
//     gpio_init(LED_GREEN_PIN);
//     gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    
//     // Start with RED on (locked state)
//     gpio_put(LED_RED_PIN, 1);
//     gpio_put(LED_GREEN_PIN, 0);
// }

// // Set LEDs for locked state (RED on, GREEN off)
// void leds_locked(void) {
//     gpio_put(LED_RED_PIN, 1);
//     gpio_put(LED_GREEN_PIN, 0);
// }

// // Set LEDs for unlocked state (RED off, GREEN on)
// void leds_unlocked(void) {
//     gpio_put(LED_RED_PIN, 0);
//     gpio_put(LED_GREEN_PIN, 1);
// }

// // Check sequence and return true if correct
// bool check_sequence(void) {
//     if (sequence_index == SEQUENCE_LENGTH) {
//         // Check if sequence matches
//         bool match = true;
//         for (int i = 0; i < SEQUENCE_LENGTH; i++) {
//             if (entered_sequence[i] != SECRET_SEQUENCE[i]) {
//                 match = false;
//                 break;
//             }
//         }
        
//         // Reset sequence for next attempt
//         sequence_index = 0;
//         memset(entered_sequence, -1, sizeof(entered_sequence));
        
//         if (match) {
//             printf("Correct sequence!\n");
//             return true;
//         } else {
//             // Wrong sequence
//             printf("Wrong sequence! Try again.\n");
            
//             // Play error tone
//             buzzer_beep(200, 300);
//             return false;
//         }
//     }
//     return false;
// }

// int main() {
//     stdio_init_all();
//     sleep_ms(2000);
    
//     printf("=== Celestial Lock System ===\n\n");
    
//     // Initialize LCD first (keep it blank initially)
//     lcd_i2c_init(i2c0, LCD_SDA_PIN, LCD_SCL_PIN, LCD_I2C_ADDR);
//     lcd_i2c_clear();
//     printf("LCD initialized (blank)\n");
    
//     // Initialize touch sensors
//     touch_init_all(TOUCH_1_PIN, TOUCH_2_PIN, TOUCH_3_PIN, TOUCH_4_PIN);
//     printf("Touch sensors: GPIO %d, %d, %d, %d\n", 
//            TOUCH_1_PIN, TOUCH_2_PIN, TOUCH_3_PIN, TOUCH_4_PIN);
    
//     // Initialize buzzer
//     buzzer_init(BUZZER_PIN);
//     printf("Buzzer: GPIO %d\n", BUZZER_PIN);
    
//     // Initialize LEDs (RED on at start)
//     leds_init();
//     printf("LEDs: RED=GPIO %d, GREEN=GPIO %d\n", LED_RED_PIN, LED_GREEN_PIN);
    
//     // Initialize electromagnet lock
//     electromagnet_init();
//     printf("Lock: GPIO %d\n", LOCK_PIN);
//     printf("Secret sequence: Touch 1-3-2-4\n\n");
    
//     // Initialize sequence tracking
//     memset(entered_sequence, -1, sizeof(entered_sequence));
    
//     char line1[17];
//     char line2[17];
//     static int8_t last_touched = -1;
    
//     printf("=== PHASE 1: Waiting for unlock sequence ===\n");
    
//     // ========== PHASE 1: LOCK PHASE - Wait for correct sequence ==========
//     while (!system_unlocked) {
//         int8_t touched = touch_get_pressed();
        
//         if (touched >= 0) {
//             buzzer_play_touch_tone(touched);
            
//             if (touched != last_touched) {
//                 printf("Touch %d pressed\n", touched + 1);
                
//                 // Add to sequence
//                 entered_sequence[sequence_index] = touched;
//                 sequence_index++;
                
//                 // Check if sequence complete and correct
//                 if (check_sequence()) {
//                     system_unlocked = true;
//                 }
//             }
//             last_touched = touched;
//         } else {
//             buzzer_stop();
//             last_touched = -1;
//         }
        
//         sleep_ms(50);
//     }
    
//     // ========== CORRECT SEQUENCE ENTERED ==========
//     printf("\n=== PHASE 2: System Unlocked ===\n");
    
//     // Switch to green LED (stays green permanently)
//     leds_unlocked();
    
//     // Play success melody
//     buzzer_beep(523, 100);  // C5
//     buzzer_beep(659, 100);  // E5
//     buzzer_beep(784, 200);  // G5
    
//     // Open electromagnet
//     electromagnet_open();
    
//     // Display "Celestial Lock Active" message for 5 seconds
//     lcd_i2c_clear();
//     lcd_i2c_set_cursor(0, 0);
//     lcd_i2c_print("Celestial");
//     lcd_i2c_set_cursor(1, 0);
//     lcd_i2c_print("Lock Active.");
//     sleep_ms(2500);
    
//     lcd_i2c_clear();
//     lcd_i2c_set_cursor(0, 0);
//     lcd_i2c_print("Align Cube.");
//     lcd_i2c_set_cursor(1, 0);
//     lcd_i2c_print("Yaw:90 P:90 R:90");
//     sleep_ms(2500);
    
//     // Now initialize gyro and servos
//     lcd_i2c_clear();
//     lcd_i2c_set_cursor(0, 0);
//     lcd_i2c_print("Initializing");
//     lcd_i2c_set_cursor(1, 0);
//     lcd_i2c_print("Gyroscope...");
    
//     // Initialize MPU6050 gyro
//     mpu6050_setup();
//     printf("MPU6050 initialized\n");
    
//     // Initialize servos: X and Y are 360°, Z is 180°
//     servo_init(SERVO_X, SERVO_X_PIN, SERVO_TYPE_360);
//     servo_init(SERVO_Y, SERVO_Y_PIN, SERVO_TYPE_360);
//     servo_init(SERVO_Z, SERVO_Z_PIN, SERVO_TYPE_180);
//     printf("Servos initialized\n");
    
//     // Center all servos
//     servo_center_all();
    
//     lcd_i2c_set_cursor(1, 0);
//     lcd_i2c_print("Ready!          ");
//     sleep_ms(1000);
    
//     lcd_i2c_clear();
    
//     printf("Starting gyro-servo control...\n\n");
    
//     // ========== PHASE 3: ACTIVE GYRO/SERVO LOOP ==========
//     while (1) {
//         // Update gyro readings
//         mpu6050_update();
        
//         // Get angles from gyro (-90 to +90 range)
//         float gyro_x = mpu6050_get_angle_x();
//         float gyro_y = mpu6050_get_angle_y();
//         float gyro_z = mpu6050_get_angle_z();
        
//         // Set servo positions directly from gyro angles
//         servo_set_angle(SERVO_X, gyro_x);
//         servo_set_angle(SERVO_Y, gyro_y);
//         servo_set_angle(SERVO_Z, gyro_z);
        
//         // Format for LCD - show as Yaw/Pitch/Roll
//         snprintf(line1, sizeof(line1), "Y:%+4.0f P:%+4.0f", gyro_z, gyro_x);
//         snprintf(line2, sizeof(line2), "R:%+4.0f deg", gyro_y);
        
//         // Update LCD
//         lcd_i2c_set_cursor(0, 0);
//         lcd_i2c_print(line1);
//         lcd_i2c_set_cursor(1, 0);
//         lcd_i2c_print(line2);
        
//         // Debug output
//         printf("Yaw:%+6.1f Pitch:%+6.1f Roll:%+6.1f\n", gyro_z, gyro_x, gyro_y);
        
//         sleep_ms(UPDATE_INTERVAL_MS);
//     }
    
//     return 0;
// }



/* 
 */
/**
 * @file main_wifi_camera.c
 * @brief WiFi Camera with ArUco Marker Verification
 * 
 * Captures images with OV7670, sends to laptop for ArUco processing,
 * and controls GREEN/RED LEDs based on verification result.
 * 
 * LED Indicators:
 *   GREEN LED - Verification successful (2 consecutive valid frames)
 *   RED LED   - Verification failed or marker not detected
 * 
 * @author MicroDrive Team
 * @date December 4, 2025
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "ov7670.h"
#include "drivers/wifi/wifi_camera.h"

// ============================================================================
// Configuration
// ============================================================================

// LED pins for verification status
#define LED_GREEN_PIN   16  // GPIO 16 - Verification successful
#define LED_RED_PIN     17  // GPIO 17 - Verification failed

// Camera capture interval
#define CAPTURE_INTERVAL_MS  500  // Capture every 500ms (2 FPS)

// Frame buffer size
#define FRAME_WIDTH   320
#define FRAME_HEIGHT  240
#define FRAME_SIZE    (FRAME_WIDTH * FRAME_HEIGHT * 2)  // YUV422

// ============================================================================
// Global Variables
// ============================================================================

static uint8_t g_frame_buffer[FRAME_SIZE];
static bool g_camera_ready = false;
static uint32_t g_frame_counter = 0;

// LED state
static bool g_green_led_on = false;
static bool g_red_led_on = false;

// Statistics
static uint32_t g_total_captures = 0;
static uint32_t g_successful_verifications = 0;
static uint32_t g_failed_verifications = 0;
static uint32_t g_markers_detected = 0;

// ============================================================================
// LED Control
// ============================================================================

void leds_init(void) {
    // Initialize LED pins
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, 0);
    
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, 0);
    
    printf("[LED] Initialized: GREEN=GPIO%d, RED=GPIO%d\n", LED_GREEN_PIN, LED_RED_PIN);
    
    // Test LEDs at startup
    printf("[LED] Testing LEDs...\n");
    gpio_put(LED_GREEN_PIN, 1);
    sleep_ms(300);
    gpio_put(LED_GREEN_PIN, 0);
    gpio_put(LED_RED_PIN, 1);
    sleep_ms(300);
    gpio_put(LED_RED_PIN, 0);
    printf("[LED] Test complete\n");
}

void led_set_verification_status(bool verified) {
    if (verified) {
        // Verification successful - GREEN on, RED off
        gpio_put(LED_GREEN_PIN, 1);
        gpio_put(LED_RED_PIN, 0);
        g_green_led_on = true;
        g_red_led_on = false;
        printf("[LED] ✓ GREEN ON (Verified)\n");
    } else {
        // Verification failed - RED on, GREEN off
        gpio_put(LED_GREEN_PIN, 0);
        gpio_put(LED_RED_PIN, 1);
        g_green_led_on = false;
        g_red_led_on = true;
        printf("[LED] ✗ RED ON (Failed)\n");
    }
}

void led_set_idle(void) {
    // Both LEDs off
    gpio_put(LED_GREEN_PIN, 0);
    gpio_put(LED_RED_PIN, 0);
    g_green_led_on = false;
    g_red_led_on = false;
}

void led_blink_error(int count) {
    // Blink RED LED to indicate error
    for (int i = 0; i < count; i++) {
        gpio_put(LED_RED_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_RED_PIN, 0);
        sleep_ms(100);
    }
}

// ============================================================================
// Camera Functions
// ============================================================================

// Camera configuration (OV7670/OV2640 driver)
static struct ov2640_config camera_config = {
    .sccb = i2c0,
    .pin_sioc = 21,  // SCL - Fixed to match CAMERA_WIRING.md
    .pin_siod = 4,   // SDA
    .pin_resetb = 17,  // RESET - Fixed
    .pin_xclk = 3,   // XCLK
    .pin_vsync = 16,  // VSYNC - Fixed
    .pin_y2_pio_base = 6,  // D0-D7 start at GPIO 6 - Fixed
    .pio = pio0,
    .pio_sm = 0,
    .dma_channel = 0,
    .image_buf = g_frame_buffer,
    .image_buf_size = FRAME_SIZE
};

bool camera_init(void) {
    printf("\n[Camera] Initializing OV7670...\n");
    
    gpio_init(camera_config.pin_vsync);
    gpio_set_dir(camera_config.pin_vsync, GPIO_IN);
    gpio_pull_down(camera_config.pin_vsync);   // or pull_up if your board idles high

    // Data pins D0..D7 (if your PIO program expects them as inputs)
    for (int pin = camera_config.pin_y2_pio_base;
         pin < camera_config.pin_y2_pio_base + 8;
         ++pin) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        // no pull needed usually, they’re driven by camera
    }

    ov2640_init(&camera_config);
    
    // Read camera ID to verify I2C communication
    uint8_t pid = ov2640_reg_read(&camera_config, 0x0A);
    uint8_t ver = ov2640_reg_read(&camera_config, 0x0B);
    printf("[Camera] Camera ID: PID=0x%02X, VER=0x%02X\n", pid, ver);
    
    if (pid != 0x76) {
        printf("[Camera] Warning: Expected PID=0x76, got 0x%02X\n", pid);
    }
    
    printf("[Camera] OV7670 initialized: %dx%d YUV422\n", FRAME_WIDTH, FRAME_HEIGHT);
    g_camera_ready = true;
    return true;
}

bool camera_capture_frame(uint8_t* buffer, size_t buffer_size) {
    if (!g_camera_ready) {
        return false;
    }
    
    if (buffer_size < FRAME_SIZE) {
        printf("[Camera] Buffer too small: %zu < %d\n", buffer_size, FRAME_SIZE);
        return false;
    }
    
    printf("[Camera] Capturing frame %lu...\n", g_frame_counter);
    
    // Capture frame using ov2640 driver
    ov2640_capture_frame(&camera_config);
    
    // Frame data is already in g_frame_buffer (camera_config.image_buf)
    // No need to copy if buffer points to g_frame_buffer
    
    g_frame_counter++;
    g_total_captures++;
    printf("[Camera] Frame %lu captured (%d bytes)\n", g_frame_counter, FRAME_SIZE);
    
    return true;
}

// ============================================================================
// Main Application
// ============================================================================

void print_banner(void) {
    printf("\n");
    printf("========================================\n");
    printf("  WiFi Camera ArUco Verification\n");
    printf("========================================\n");
    printf("  Pico W + OV7670 Camera\n");
    printf("  Resolution: %dx%d\n", FRAME_WIDTH, FRAME_HEIGHT);
    printf("  Server: %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("========================================\n\n");
}

void print_statistics(void) {
    printf("\n--- Statistics ---\n");
    printf("  Total Captures:       %lu\n", g_total_captures);
    printf("  Markers Detected:     %lu\n", g_markers_detected);
    printf("  Verifications (OK):   %lu\n", g_successful_verifications);
    printf("  Verifications (FAIL): %lu\n", g_failed_verifications);
    printf("  LED Status: GREEN=%s, RED=%s\n",
           g_green_led_on ? "ON" : "OFF",
           g_red_led_on ? "ON" : "OFF");
    printf("------------------\n\n");
}

int main() {
    // Initialize stdio
    stdio_init_all();
    sleep_ms(2000);  // Wait for USB serial
    
    print_banner();
    
    // Initialize LEDs
    leds_init();
    
    // Initialize WiFi
    printf("[WiFi] Initializing WiFi...\n");
    if (!wifi_camera_init()) {
        printf("[ERROR] WiFi initialization failed\n");
        led_blink_error(5);
        return -1;
    }
    
    // Connect to server
    printf("[WiFi] Connecting to server %s:%d...\n", SERVER_IP, SERVER_PORT);
    if (!wifi_camera_connect(SERVER_IP, SERVER_PORT)) {
        printf("[ERROR] Failed to connect to server\n");
        led_blink_error(5);
        return -1;
    }
    
    printf("[WiFi] Connected to server!\n");
    
    // Initialize camera
    if (!camera_init()) {
        printf("[ERROR] Camera initialization failed\n");
        led_blink_error(5);
        return -1;
    }
    
    // Initialize ping LED monitoring
    wifi_camera_ping_led_init();
    
    printf("\n[Main] Starting capture loop...\n");
    printf("[Main] Press Ctrl+C to stop\n\n");
    
    uint32_t last_capture_time = 0;
    uint32_t loop_count = 0;
    
    // Main loop
    while (true) {
        // Update ping LED state
        wifi_camera_ping_led_task();
        
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        
        // Capture and send frame at regular intervals
        if ((current_time - last_capture_time) >= CAPTURE_INTERVAL_MS) {
            last_capture_time = current_time;
            loop_count++;
            
            printf("\n=== Capture #%lu (Frame #%lu) ===\n", loop_count, g_frame_counter);
            
            // Capture frame
            if (!camera_capture_frame(g_frame_buffer, FRAME_SIZE)) {
                printf("[ERROR] Frame capture failed\n");
                led_set_verification_status(false);
                g_failed_verifications++;
                continue;
            }
            
            // Send frame to server
            printf("[WiFi] Sending frame to server...\n");
            if (!wifi_camera_send_frame(g_frame_buffer, FRAME_WIDTH, FRAME_HEIGHT)) {
                printf("[ERROR] Failed to send frame\n");
                led_set_verification_status(false);
                g_failed_verifications++;
                continue;
            }
            
            printf("[WiFi] Frame sent, waiting for response...\n");
            
            // Receive verification response
            pose_response_t response;
            if (!wifi_camera_receive_response(&response, 5000)) {
                printf("[ERROR] No response from server (timeout)\n");
                led_set_verification_status(false);
                g_failed_verifications++;
                continue;
            }
            
            // Process response
            printf("[Response] Received:\n");
            printf("  Marker Found:  %s\n", response.marker_found ? "YES" : "NO");
            
            if (response.marker_found) {
                g_markers_detected++;
                printf("  Marker ID:     %d\n", response.marker_id);
                printf("  Pose Valid:    %s\n", response.pose_valid ? "YES" : "NO");
                printf("  Unlock Ready:  %s\n", response.unlock_ready ? "YES" : "NO");
                printf("  Position:      (%.3f, %.3f, %.3f) m\n",
                       response.pos_x, response.pos_y, response.pos_z);
                printf("  Rotation:      (%.1f, %.1f, %.1f) deg\n",
                       response.rot_x, response.rot_y, response.rot_z);
                
                // Update LED status based on verification
                if (response.unlock_ready) {
                    printf("[RESULT] ✓✓✓ VERIFICATION COMPLETE ✓✓✓\n");
                    led_set_verification_status(true);
                    g_successful_verifications++;
                } else if (response.pose_valid) {
                    printf("[RESULT] ✓ Pose valid, waiting for consecutive frames...\n");
                    led_set_verification_status(false);
                    g_failed_verifications++;
                } else {
                    printf("[RESULT] ✗ Pose out of tolerance\n");
                    led_set_verification_status(false);
                    g_failed_verifications++;
                }
            } else {
                printf("[RESULT] ✗ No marker detected\n");
                led_set_verification_status(false);
                g_failed_verifications++;
            }
            
            // Print statistics every 10 captures
            if (loop_count % 10 == 0) {
                print_statistics();
            }
        }
        
        // Small delay to prevent busy waiting
        sleep_ms(10);
    }
    
    // Cleanup (never reached in this implementation)
    wifi_camera_disconnect();
    led_set_idle();
    
    return 0;
}
