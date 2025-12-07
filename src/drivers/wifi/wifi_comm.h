/**
 * @file wifi_comm.h
 * @brief WiFi Communication Protocol for Pico-to-Laptop Control
 * 
 * Protocol:
 * Pico 1 (Gimbal) -> Laptop: "INIT" (ready signal)
 * Laptop -> Pico 1: "OK" (detection confirmed)
 * Pico 1 -> Laptop: "RESET" (request reset)
 */

#ifndef WIFI_COMM_H
#define WIFI_COMM_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

// WiFi credentials
#ifndef WIFI_SSID
#define WIFI_SSID       "youssef's Galaxy S21 Ultra 5G"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD   "Ctiger@YM123"
#endif

// Server settings (laptop IP and port)
#define LAPTOP_IP       "10.96.19.200"  // Change to your laptop's IP
#define CONTROL_PORT    9999            // Different port from camera stream

// Protocol commands
#define CMD_INIT        "INIT"      // Pico sends: System ready
#define CMD_OK          "OK"        // Laptop sends: Detection confirmed
#define CMD_RESET       "RESET"     // Pico sends: Request system reset
#define CMD_ACK         "ACK"       // Laptop sends: Acknowledgment
#define CMD_STATUS      "STATUS"    // Pico sends: Request status
#define CMD_PING        "PING"      // Keep-alive heartbeat

#define MAX_CMD_LEN     64
#define HEARTBEAT_INTERVAL_MS   5000    // Send ping every 5 seconds
#define CONNECTION_TIMEOUT_MS   15000   // Reconnect if no response for 15s

// Communication state
typedef struct {
    bool wifi_connected;        // WiFi network connected
    bool server_connected;      // TCP socket connected
    bool detection_active;      // Laptop sent OK (detection mode)
    uint32_t last_response_ms;  // Last time we got a response
    uint32_t last_heartbeat_ms; // Last time we sent a heartbeat
    uint32_t messages_sent;     // Total messages sent
    uint32_t messages_received; // Total messages received
} wifi_comm_state_t;

/**
 * @brief Initialize WiFi and connect to network
 * @return true on success, false on failure
 */
bool wifi_comm_init(void);

/**
 * @brief Connect to the laptop control server
 * @param server_ip IP address of laptop
 * @param port Server port number
 * @return true on success, false on failure
 */
bool wifi_comm_connect(const char* server_ip, uint16_t port);

/**
 * @brief Send INIT command to laptop (system ready)
 * @return true if sent successfully
 */
bool wifi_comm_send_init(void);

/**
 * @brief Send RESET command to laptop (request reset)
 * @return true if sent successfully
 */
bool wifi_comm_send_reset(void);

/**
 * @brief Send STATUS request to laptop
 * @return true if sent successfully
 */
bool wifi_comm_send_status(void);

/**
 * @brief Poll for incoming messages from laptop
 * Should be called regularly in main loop
 * @return true if new message received
 */
bool wifi_comm_poll(void);

/**
 * @brief Check if laptop sent OK (detection confirmed)
 * @return true if detection is active
 */
bool wifi_comm_is_detection_active(void);

/**
 * @brief Get the current communication state
 * @return Pointer to state structure
 */
wifi_comm_state_t* wifi_comm_get_state(void);

/**
 * @brief Disconnect and cleanup
 */
void wifi_comm_disconnect(void);

/**
 * @brief Send a custom command string
 * @param cmd Command string to send
 * @return true if sent successfully
 */
bool wifi_comm_send_command(const char* cmd);

/**
 * @brief Get last received command (after wifi_comm_poll returns true)
 * @return Pointer to received command string (valid until next poll)
 */
const char* wifi_comm_get_last_command(void);

#endif // WIFI_COMM_H
