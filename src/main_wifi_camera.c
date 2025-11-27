/**
 * @file main_wifi_camera.c
 * @brief Main application for WiFi Camera with ArUco Detection
 * 
 * Streams OV7670 camera frames over WiFi to laptop for ArUco pose verification.
 * Signals unlock when correct marker pose is held for 2 seconds.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/dma.h"
#include "hardware/pio.h"

// Drivers
#include "drivers/camera/ov7670.h"
#include "drivers/wifi/wifi_camera.h"

// ============================================================================
// Configuration
// ============================================================================

// Frame dimensions (match OV7670 settings)
#define FRAME_WIDTH     320
#define FRAME_HEIGHT    240

// WiFi server settings (change these to match your laptop)
#define LAPTOP_IP       "192.168.1.100"
#define LAPTOP_PORT     8888

// LED pin for status indication
#define LED_PIN         CYW43_WL_GPIO_LED_PIN  // Onboard LED on Pico W

// Unlock output pin (connect to relay, solenoid, etc.)
#define UNLOCK_PIN      22

// ============================================================================
// Global Variables
// ============================================================================

// Frame buffer for camera (YUV422 format)
static uint8_t frame_buffer[FRAME_WIDTH * FRAME_HEIGHT * 2];

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

// Status tracking
static bool g_unlock_triggered = false;
static uint32_t g_last_frame_time = 0;

// ============================================================================
// Function Prototypes
// ============================================================================

void init_gpio(void);
void status_led_task(void);
void unlock_task(pose_response_t* response);
void print_pose_info(pose_response_t* response);

// ============================================================================
// Main Application
// ============================================================================

int main(void) {
    // Initialize standard I/O
    stdio_init_all();
    sleep_ms(2000);  // Wait for USB serial
    
    printf("\n");
    printf("==============================================\n");
    printf("  WiFi Camera ArUco Pose Verification System\n");
    printf("==============================================\n\n");
    
    // Initialize GPIO pins
    init_gpio();
    
    // Initialize camera
    printf("[Camera] Initializing OV7670...\n");
    ov2640_init(&camera_config);
    
    // Verify camera communication
    uint8_t pid = ov2640_reg_read(&camera_config, 0x0A);
    uint8_t ver = ov2640_reg_read(&camera_config, 0x0B);
    printf("[Camera] ID: PID=0x%02X, VER=0x%02X\n", pid, ver);
    
    if (pid != 0x76) {
        printf("[Camera] WARNING: Unexpected camera ID!\n");
    }
    
    printf("[Camera] Resolution: %dx%d\n", FRAME_WIDTH, FRAME_HEIGHT);
    
    // Initialize WiFi
    printf("\n[System] Initializing WiFi...\n");
    if (!wifi_camera_init()) {
        printf("[System] WiFi initialization failed! Halting.\n");
        while (1) {
            cyw43_arch_gpio_put(LED_PIN, 1);
            sleep_ms(100);
            cyw43_arch_gpio_put(LED_PIN, 0);
            sleep_ms(100);
        }
    }
    
    // Initialize ping LED monitoring
    printf("\n[System] Initializing ping LED monitoring...\n");
    wifi_camera_ping_led_init();
    
    // Connect to server
    printf("\n[System] Connecting to ArUco processor at %s:%d...\n", LAPTOP_IP, LAPTOP_PORT);
    if (!wifi_camera_connect(LAPTOP_IP, LAPTOP_PORT)) {
        printf("[System] Server connection failed! Check laptop is running.\n");
        printf("[System] Retrying in 5 seconds...\n");
        sleep_ms(5000);
        
        // Keep trying to connect
        while (!wifi_camera_connect(LAPTOP_IP, LAPTOP_PORT)) {
            printf("[System] Retry failed. Trying again in 5 seconds...\n");
            sleep_ms(5000);
        }
    }
    
    printf("\n[System] ===== SYSTEM READY =====\n");
    printf("[System] Streaming frames to server...\n");
    printf("[System] Hold ArUco marker in view for 2 seconds to unlock\n\n");
    
    // Main loop
    pose_response_t response;
    uint32_t frame_count = 0;
    
    while (1) {
        uint32_t frame_start = to_ms_since_boot(get_absolute_time());
        
        // Capture frame from camera
        ov2640_capture_frame(&camera_config);
        
        // Send frame to server
        if (wifi_camera_send_frame(frame_buffer, FRAME_WIDTH, FRAME_HEIGHT)) {
            
            // Wait for response (max 500ms)
            if (wifi_camera_receive_response(&response, 500)) {
                
                // Print pose info every 10 frames
                if (frame_count % 10 == 0) {
                    print_pose_info(&response);
                }
                
                // Handle unlock logic
                unlock_task(&response);
                
            } else {
                printf("[System] No response from server (timeout)\n");
            }
            
        } else {
            printf("[System] Failed to send frame\n");
            
            // Try to reconnect
            if (!wifi_camera_get_state()->connected) {
                printf("[System] Connection lost, reconnecting...\n");
                wifi_camera_connect(LAPTOP_IP, LAPTOP_PORT);
            }
        }
        
        // Calculate and print FPS
        uint32_t frame_time = to_ms_since_boot(get_absolute_time()) - frame_start;
        if (frame_count % 30 == 0) {
            float fps = (frame_time > 0) ? (1000.0f / frame_time) : 0;
            printf("[System] Frame time: %dms (%.1f FPS)\n", frame_time, fps);
        }
        
        // Status LED
        status_led_task();
        
        // Update ping LED state
        wifi_camera_ping_led_task();
        
        frame_count++;
        
        // Limit frame rate to ~10 FPS if processing is faster
        if (frame_time < 100) {
            sleep_ms(100 - frame_time);
        }
    }
    
    return 0;
}

// ============================================================================
// Utility Functions
// ============================================================================

void init_gpio(void) {
    // Initialize unlock output pin
    gpio_init(UNLOCK_PIN);
    gpio_set_dir(UNLOCK_PIN, GPIO_OUT);
    gpio_put(UNLOCK_PIN, 0);  // Start locked
    
    printf("[GPIO] Unlock pin initialized (GPIO%d)\n", UNLOCK_PIN);
}

void status_led_task(void) {
    static uint32_t last_toggle = 0;
    static bool led_state = false;
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    wifi_camera_state_t* state = wifi_camera_get_state();
    
    // Different blink rates for different states
    uint32_t interval;
    if (g_unlock_triggered) {
        interval = 100;   // Fast blink when unlocked
    } else if (state->last_pose.pose_valid) {
        interval = 250;   // Medium blink when pose valid
    } else if (state->connected) {
        interval = 1000;  // Slow blink when connected
    } else {
        interval = 2000;  // Very slow blink when disconnected
    }
    
    if ((now - last_toggle) >= interval) {
        led_state = !led_state;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
        last_toggle = now;
    }
}

void unlock_task(pose_response_t* response) {
    static uint32_t unlock_start_time = 0;
    
    // Check if unlock conditions are met
    if (wifi_camera_is_unlock_ready() || response->unlock_ready) {
        if (!g_unlock_triggered) {
            printf("\n");
            printf("*********************************************\n");
            printf("*           UNLOCK TRIGGERED!               *\n");
            printf("*  ArUco marker verified for 2 seconds      *\n");
            printf("*********************************************\n");
            printf("\n");
            
            // Activate unlock pin
            gpio_put(UNLOCK_PIN, 1);
            g_unlock_triggered = true;
            unlock_start_time = to_ms_since_boot(get_absolute_time());
        }
    }
    
    // Auto-relock after 5 seconds
    if (g_unlock_triggered) {
        uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - unlock_start_time;
        if (elapsed > 5000) {
            printf("[System] Auto-relocking...\n");
            gpio_put(UNLOCK_PIN, 0);
            g_unlock_triggered = false;
        }
    }
}

void print_pose_info(pose_response_t* response) {
    if (response->marker_found) {
        printf("[ArUco] Marker ID: %d | Pose: [%.1f, %.1f, %.1f] Rot: [%.1f°, %.1f°, %.1f°] | Valid: %s\n",
               response->marker_id,
               response->pos_x, response->pos_y, response->pos_z,
               response->rot_x, response->rot_y, response->rot_z,
               response->pose_valid ? "YES" : "NO");
    } else {
        printf("[ArUco] No marker detected\n");
    }
}
