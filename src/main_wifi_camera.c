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
#include "wifi_camera.h"

// ============================================================================
// Configuration
// ============================================================================

// LED pins for verification status
#define LED_GREEN_PIN   20 // GPIO 20 - Verification successful
#define LED_RED_PIN     22 // GPIO 22 - Verification failed

// Camera capture interval
#define CAPTURE_INTERVAL_MS  500  // Capture every 500ms (2 FPS)

// Server connection
#define SERVER_IP    "10.96.19.200"  // Change to your laptop's IP
#define SERVER_PORT  8888

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
    .pin_sioc = 5,
    .pin_siod = 4,
    .pin_resetb = 2,
    .pin_xclk = 3,
    .pin_vsync = 6,
    .pin_y2_pio_base = 10,  // D0-D7 start at GPIO 10
    .pio = pio0,
    .pio_sm = 0,
    .dma_channel = 0,
    .image_buf = g_frame_buffer,
    .image_buf_size = FRAME_SIZE
};

bool camera_init(void) {
    printf("\n[Camera] Initializing OV7670...\n");
    
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
