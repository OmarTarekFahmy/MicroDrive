/**
 * @file wifi_comm.c
 * @brief WiFi Communication Implementation for Pico-Laptop Control
 */

#include "wifi_comm.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>

// Private state
static wifi_comm_state_t g_state = {0};
static struct tcp_pcb* g_tcp_pcb = NULL;
static ip_addr_t g_server_addr;
static uint16_t g_server_port;

// Receive buffer
static char g_recv_buffer[MAX_CMD_LEN];
static volatile int g_recv_len = 0;
static volatile bool g_new_message = false;

// Last received command
static char g_last_command[MAX_CMD_LEN] = {0};

// ============================================================================
// Private Functions - TCP Callbacks
// ============================================================================

static err_t tcp_connected_callback(void* arg, struct tcp_pcb* tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("[WiFi] Connection failed: %d\n", err);
        return err;
    }
    
    printf("[WiFi] Connected to server!\n");
    g_state.server_connected = true;
    g_state.last_response_ms = to_ms_since_boot(get_absolute_time());
    return ERR_OK;
}

static err_t tcp_recv_callback(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err) {
    if (!p) {
        // Connection closed by server
        printf("[WiFi] Connection closed by server\n");
        g_state.server_connected = false;
        tcp_close(tpcb);
        g_tcp_pcb = NULL;
        return ERR_OK;
    }
    
    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }
    
    // Copy received data
    uint16_t len = p->tot_len;
    if (len < MAX_CMD_LEN) {
        pbuf_copy_partial(p, g_recv_buffer, len, 0);
        g_recv_buffer[len] = '\0';  // Null terminate
        g_recv_len = len;
        g_new_message = true;
        
        // Update state
        g_state.last_response_ms = to_ms_since_boot(get_absolute_time());
        g_state.messages_received++;
        
        // Store last command
        strncpy(g_last_command, g_recv_buffer, MAX_CMD_LEN - 1);
        g_last_command[MAX_CMD_LEN - 1] = '\0';
        
        // Check for OK command
        if (strncmp(g_recv_buffer, CMD_OK, strlen(CMD_OK)) == 0) {
            g_state.detection_active = true;
            printf("[WiFi] Detection confirmed by laptop!\n");
        }
    }
    
    // Acknowledge reception
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    
    return ERR_OK;
}

static void tcp_error_callback(void* arg, err_t err) {
    printf("[WiFi] TCP error: %d\n", err);
    g_state.server_connected = false;
    g_tcp_pcb = NULL;  // PCB is already freed by lwIP
}

// ============================================================================
// Public Functions
// ============================================================================

bool wifi_comm_init(void) {
    printf("[WiFi Comm] Initializing WiFi...\n");
    
    // Initialize CYW43 (Pico W WiFi chip)
    if (cyw43_arch_init()) {
        printf("[WiFi Comm] Failed to initialize CYW43\n");
        return false;
    }
    
    // Enable station mode
    cyw43_arch_enable_sta_mode();
    
    printf("[WiFi Comm] Connecting to '%s'...\n", WIFI_SSID);
    
    // Connect to WiFi with timeout
    int result = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID, 
        WIFI_PASSWORD, 
        CYW43_AUTH_WPA2_AES_PSK,
        30000  // 30 second timeout
    );
    
    if (result != 0) {
        printf("[WiFi Comm] Connection failed with error %d\n", result);
        cyw43_arch_deinit();
        return false;
    }
    
    // Get and print IP address
    struct netif* netif = &cyw43_state.netif[0];
    uint8_t* ip = (uint8_t*)&netif->ip_addr.addr;
    printf("[WiFi Comm] Connected! IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    
    // Initialize state
    g_state.wifi_connected = true;
    g_state.server_connected = false;
    g_state.detection_active = false;
    g_state.last_response_ms = to_ms_since_boot(get_absolute_time());
    g_state.last_heartbeat_ms = to_ms_since_boot(get_absolute_time());
    g_state.messages_sent = 0;
    g_state.messages_received = 0;
    
    return true;
}

bool wifi_comm_connect(const char* server_ip, uint16_t port) {
    printf("[WiFi Comm] Connecting to server %s:%d...\n", server_ip, port);
    
    if (!g_state.wifi_connected) {
        printf("[WiFi Comm] WiFi not connected\n");
        return false;
    }
    
    // Parse server IP
    if (!ip4addr_aton(server_ip, &g_server_addr)) {
        printf("[WiFi Comm] Invalid server IP address\n");
        return false;
    }
    g_server_port = port;
    
    // Create TCP connection
    g_tcp_pcb = tcp_new();
    if (!g_tcp_pcb) {
        printf("[WiFi Comm] Failed to create TCP PCB\n");
        return false;
    }
    
    // Set callbacks
    tcp_err(g_tcp_pcb, tcp_error_callback);
    tcp_recv(g_tcp_pcb, tcp_recv_callback);
    
    // Initiate connection
    err_t err = tcp_connect(g_tcp_pcb, &g_server_addr, port, tcp_connected_callback);
    if (err != ERR_OK) {
        printf("[WiFi Comm] TCP connect failed: %d\n", err);
        tcp_close(g_tcp_pcb);
        g_tcp_pcb = NULL;
        return false;
    }
    
    // Wait for connection (with timeout)
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (!g_state.server_connected && (to_ms_since_boot(get_absolute_time()) - start) < 10000) {
        cyw43_arch_poll();
        sleep_ms(10);
    }
    
    if (!g_state.server_connected) {
        printf("[WiFi Comm] Connection timeout\n");
        if (g_tcp_pcb) {
            tcp_close(g_tcp_pcb);
            g_tcp_pcb = NULL;
        }
        return false;
    }
    
    printf("[WiFi Comm] Server connected successfully!\n");
    return true;
}

bool wifi_comm_send_command(const char* cmd) {
    if (!g_state.server_connected || !g_tcp_pcb) {
        printf("[WiFi Comm] Not connected to server\n");
        return false;
    }
    
    size_t len = strlen(cmd);
    if (len == 0 || len >= MAX_CMD_LEN) {
        printf("[WiFi Comm] Invalid command length: %d\n", len);
        return false;
    }
    
    // Add newline if not present
    char send_buf[MAX_CMD_LEN];
    if (cmd[len - 1] != '\n') {
        snprintf(send_buf, sizeof(send_buf), "%s\n", cmd);
    } else {
        strncpy(send_buf, cmd, sizeof(send_buf));
    }
    len = strlen(send_buf);
    
    // Send via TCP
    err_t err = tcp_write(g_tcp_pcb, send_buf, len, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        printf("[WiFi Comm] TCP write failed: %d\n", err);
        return false;
    }
    
    // Flush
    err = tcp_output(g_tcp_pcb);
    if (err != ERR_OK) {
        printf("[WiFi Comm] TCP output failed: %d\n", err);
        return false;
    }
    
    g_state.messages_sent++;
    printf("[WiFi Comm] Sent: %s", send_buf);
    
    return true;
}

bool wifi_comm_send_init(void) {
    return wifi_comm_send_command(CMD_INIT);
}

bool wifi_comm_send_reset(void) {
    return wifi_comm_send_command(CMD_RESET);
}

bool wifi_comm_send_status(void) {
    return wifi_comm_send_command(CMD_STATUS);
}

bool wifi_comm_poll(void) {
    if (!g_state.wifi_connected) {
        return false;
    }
    
    // Poll WiFi stack
    cyw43_arch_poll();
    
    // Check for heartbeat timeout
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Send heartbeat if needed
    if (g_state.server_connected && 
        (now - g_state.last_heartbeat_ms) > HEARTBEAT_INTERVAL_MS) {
        wifi_comm_send_command(CMD_PING);
        g_state.last_heartbeat_ms = now;
    }
    
    // Check for connection timeout
    if (g_state.server_connected && 
        (now - g_state.last_response_ms) > CONNECTION_TIMEOUT_MS) {
        printf("[WiFi Comm] Connection timeout, reconnecting...\n");
        g_state.server_connected = false;
        if (g_tcp_pcb) {
            tcp_close(g_tcp_pcb);
            g_tcp_pcb = NULL;
        }
    }
    
    // Check for new message
    if (g_new_message) {
        g_new_message = false;
        return true;
    }
    
    return false;
}

bool wifi_comm_is_detection_active(void) {
    return g_state.detection_active;
}

wifi_comm_state_t* wifi_comm_get_state(void) {
    return &g_state;
}

const char* wifi_comm_get_last_command(void) {
    return g_last_command;
}

void wifi_comm_disconnect(void) {
    printf("[WiFi Comm] Disconnecting...\n");
    
    if (g_tcp_pcb) {
        tcp_close(g_tcp_pcb);
        g_tcp_pcb = NULL;
    }
    
    if (g_state.wifi_connected) {
        cyw43_arch_deinit();
    }
    
    g_state.wifi_connected = false;
    g_state.server_connected = false;
    g_state.detection_active = false;
}
