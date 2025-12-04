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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "ov7670.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

// Frame buffer for camera
static uint8_t frame_buffer[FRAME_WIDTH * FRAME_HEIGHT * 2];  // YUY2 format (2 bytes per pixel)

// Camera configuration
static struct ov2640_config camera_config = {
    .sccb = i2c0,
    .pin_sioc = 21,           // I2C0 SCL
    .pin_siod = 4,            // I2C0 SDA
    .pin_resetb = 17,         // Camera reset
    .pin_xclk = 3,            // Master clock for camera
    .pin_vsync = 16,          // Vertical sync
    .pin_y2_pio_base = 6,     // D0-D7, PCLK, HREF base pin
    .pio = pio0,
    .pio_sm = 0,
    .dma_channel = 0,
    .image_buf = frame_buffer,
    .image_buf_size = sizeof(frame_buffer)
};

/* Blink pattern:
 * - 250 ms : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void video_task(void);

/*------------- MAIN -------------*/
int main(void)
{
    board_init();
    
    // Initialize TinyUSB
    tusb_init();

    stdio_init_all();
    sleep_ms(2000);  // Give time for USB serial to initialize

    printf("\n\nOV7670 USB Camera\n");
    printf("Initializing camera...\n");

    // Initialize the camera
    ov2640_init(&camera_config);
    
    // Read camera ID to verify I2C communication
    uint8_t pid = ov2640_reg_read(&camera_config, 0x0A);  // Product ID
    uint8_t ver = ov2640_reg_read(&camera_config, 0x0B);  // Version
    printf("Camera ID: PID=0x%02X, VER=0x%02X (should be 0x76, 0x73)\n", pid, ver);
    
    printf("Camera initialized successfully!\n");
    printf("Resolution: %dx%d\n", FRAME_WIDTH, FRAME_HEIGHT);
    printf("Frame buffer size: %d bytes\n", sizeof(frame_buffer));
    
    // Fill buffer with test pattern to verify USB streaming works
    for(int i = 0; i < sizeof(frame_buffer); i += 4) {
        frame_buffer[i] = 0x80;      // Y
        frame_buffer[i+1] = 0x80;    // U
        frame_buffer[i+2] = 0x80;    // Y
        frame_buffer[i+3] = 0x80;    // V (gray color)
    }
    printf("Test pattern loaded\n");

    while (1)
    {
        tud_task(); // TinyUSB device task
        led_blinking_task();
        video_task();
    }

    return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
    printf("Device mounted\n");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    blink_interval_ms = BLINK_NOT_MOUNTED;
    printf("Device unmounted\n");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    blink_interval_ms = BLINK_SUSPENDED;
    printf("Device suspended\n");
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
    printf("Device resumed\n");
}

//--------------------------------------------------------------------+
// USB Video
//--------------------------------------------------------------------+

static unsigned frame_num = 0;
static unsigned tx_busy = 0;
static unsigned interval_ms = 1000 / FRAME_RATE;

void video_task(void)
{
    static unsigned start_ms = 0;
    static unsigned already_sent = 0;

    if (!tud_video_n_streaming(0, 0)) {
        already_sent  = 0;
        frame_num     = 0;
        tx_busy       = 0;
        return;
    }

    if (!already_sent) {
        already_sent = 1;
        tx_busy = 1;
        start_ms = board_millis();
        
        // Capture a frame from the camera
        printf("Capturing frame %u...\n", frame_num);
        ov2640_capture_frame(&camera_config);
        printf("Frame captured (%d bytes)\n", sizeof(frame_buffer));
        
        tud_video_n_frame_xfer(0, 0, (void*)frame_buffer, sizeof(frame_buffer));
        return;
    }

    unsigned cur = board_millis();
    if (cur - start_ms < interval_ms) return; // not enough time
    if (tx_busy) return;
    
    tx_busy = 1;
    start_ms += interval_ms;

    // Capture next frame
    printf("Capturing frame %u...\n", ++frame_num);
    ov2640_capture_frame(&camera_config);
    printf("Frame captured\n");
    
    tud_video_n_frame_xfer(0, 0, (void*)frame_buffer, sizeof(frame_buffer));
}

void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx)
{
    (void)ctl_idx; (void)stm_idx;
    tx_busy = 0;
    /* flip buffer */
    //already_sent = 0;
}

int tud_video_commit_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx,
                        video_probe_and_commit_control_t const *parameters)
{
    (void)ctl_idx; (void)stm_idx;
    /* convert unit to ms from 100 ns */
    interval_ms = parameters->dwFrameInterval / 10000;
    printf("Video commit: interval = %u ms\n", interval_ms);
    return VIDEO_ERROR_NONE;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
    static uint32_t start_ms = 0;
    static bool led_state = false;

    // Blink every interval ms
    if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
}
