/**
 * @file wifi_camera.h
 * @brief WiFi Camera Streaming Driver for Pico W + OV7670
 * 
 * Streams camera frames over TCP/UDP to a laptop for ArUco processing
 */

#ifndef WIFI_CAMERA_H
#define WIFI_CAMERA_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

// ============================================================================
// Configuration
// ============================================================================

// Ping LED settings
#define PING_LED_PIN    15          // GPIO pin for ping indicator LED
#define PING_LED_DURATION_MS 3000   // LED on duration when pinged

// WiFi credentials (change these!)
#define WIFI_SSID       "OmarGamal"
#define WIFI_PASSWORD   "ufvb8809"

// Server settings (laptop IP and port)
#define SERVER_IP       "192.168.1.100"  // Change to your laptop's IP
#define SERVER_PORT     8888

// Streaming protocol
#define USE_TCP         1   // 1 = TCP (reliable), 0 = UDP (faster)

// Frame settings
#define FRAME_WIDTH_WIFI    320
#define FRAME_HEIGHT_WIFI   240
#define FRAME_CHANNELS      2   // YUV422 = 2 bytes per pixel

// Packet structure
#define PACKET_HEADER_SIZE  12
#define MAX_PACKET_SIZE     1400  // MTU safe size

// ============================================================================
// Data Structures
// ============================================================================

// Frame header sent before each image
typedef struct __attribute__((packed)) {
    uint32_t magic;         // 0xCAFEBABE - identifies start of frame
    uint32_t frame_id;      // Sequential frame number
    uint16_t width;         // Image width
    uint16_t height;        // Image height
    uint16_t format;        // 0=YUV422, 1=RGB565, 2=JPEG
    uint16_t data_size;     // Size of image data following header
    uint32_t checksum;      // Simple checksum for verification
} frame_header_t;

// Response from server
typedef struct __attribute__((packed)) {
    uint32_t magic;         // 0xDEADBEEF - identifies response
    uint8_t  marker_found;  // 1 if ArUco marker detected
    uint8_t  marker_id;     // ID of detected marker
    uint8_t  pose_valid;    // 1 if pose is within tolerance
    uint8_t  unlock_ready;  // 1 if unlock conditions met (2s verified)
    float    pos_x;         // X position of marker
    float    pos_y;         // Y position of marker
    float    pos_z;         // Z position (distance)
    float    rot_x;         // Rotation around X axis (degrees)
    float    rot_y;         // Rotation around Y axis (degrees)
    float    rot_z;         // Rotation around Z axis (degrees)
} pose_response_t;

// WiFi camera state
typedef struct {
    bool connected;
    uint32_t frame_count;
    uint32_t error_count;
    pose_response_t last_pose;
} wifi_camera_state_t;

// ============================================================================
// Function Prototypes
// ============================================================================

/**
 * @brief Initialize WiFi and connect to network
 * @return true on success, false on failure
 */
bool wifi_camera_init(void);

/**
 * @brief Connect to the processing server
 * @param server_ip IP address of the laptop running ArUco processor
 * @param port Server port number
 * @return true on success, false on failure
 */
bool wifi_camera_connect(const char* server_ip, uint16_t port);

/**
 * @brief Send a frame to the server for processing
 * @param frame_data Pointer to image data (YUV422 format)
 * @param width Image width
 * @param height Image height
 * @return true on success, false on failure
 */
bool wifi_camera_send_frame(const uint8_t* frame_data, uint16_t width, uint16_t height);

/**
 * @brief Receive pose response from server
 * @param response Pointer to response structure to fill
 * @param timeout_ms Timeout in milliseconds
 * @return true if response received, false on timeout/error
 */
bool wifi_camera_receive_response(pose_response_t* response, uint32_t timeout_ms);

/**
 * @brief Check if unlock conditions are met
 * @return true if marker verified for 2 consecutive seconds
 */
bool wifi_camera_is_unlock_ready(void);

/**
 * @brief Get current state
 * @return Pointer to current state structure
 */
wifi_camera_state_t* wifi_camera_get_state(void);

/**
 * @brief Disconnect and cleanup
 */
void wifi_camera_disconnect(void);

/**
 * @brief Calculate simple checksum for frame data
 */
uint32_t wifi_camera_checksum(const uint8_t* data, size_t len);

/**
 * @brief Initialize ping LED monitoring
 * Sets up GPIO and ICMP listener to light LED on ping
 */
void wifi_camera_ping_led_init(void);

/**
 * @brief Update ping LED state (call this regularly in main loop)
 * Automatically turns off LED after configured duration
 */
void wifi_camera_ping_led_task(void);

#endif // WIFI_CAMERA_H
