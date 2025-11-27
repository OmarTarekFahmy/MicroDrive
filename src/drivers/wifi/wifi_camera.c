/**
 * @file wifi_camera.c
 * @brief WiFi Camera Streaming Implementation for Pico W + OV7670
 */

#include "wifi_camera.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/dns.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Private Variables
// ============================================================================

static wifi_camera_state_t g_state = {0};
static struct tcp_pcb* g_tcp_pcb = NULL;
static struct udp_pcb* g_udp_pcb = NULL;
static ip_addr_t g_server_addr;
static uint16_t g_server_port;

// Buffer for receiving responses
static uint8_t g_recv_buffer[256];
static volatile bool g_response_received = false;
static volatile int g_recv_len = 0;

// Unlock verification tracking
static uint32_t g_pose_valid_start_ms = 0;
static bool g_pose_was_valid = false;

// ============================================================================
// Private Function Prototypes
// ============================================================================

static err_t tcp_connected_callback(void* arg, struct tcp_pcb* tpcb, err_t err);
static err_t tcp_recv_callback(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err);
static void tcp_error_callback(void* arg, err_t err);
static void udp_recv_callback(void* arg, struct udp_pcb* pcb, struct pbuf* p,
                               const ip_addr_t* addr, u16_t port);

// ============================================================================
// Public Functions
// ============================================================================

bool wifi_camera_init(void) {
    printf("[WiFi] Initializing...\n");
    
    // Initialize CYW43 (Pico W WiFi chip)
    if (cyw43_arch_init()) {
        printf("[WiFi] Failed to initialize CYW43\n");
        return false;
    }
    
    // Enable station mode
    cyw43_arch_enable_sta_mode();
    
    printf("[WiFi] Connecting to '%s'...\n", WIFI_SSID);
    
    // Connect to WiFi with timeout
    int result = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID, 
        WIFI_PASSWORD, 
        CYW43_AUTH_WPA2_AES_PSK,
        30000  // 30 second timeout
    );
    
    if (result != 0) {
        printf("[WiFi] Connection failed with error %d\n", result);
        return false;
    }
    
    // Get and print IP address
    uint8_t* ip = (uint8_t*)&cyw43_state.netif[0].ip_addr.addr;
    printf("[WiFi] Connected! IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    
    g_state.connected = false;  // Socket not yet connected
    g_state.frame_count = 0;
    g_state.error_count = 0;
    
    return true;
}

bool wifi_camera_connect(const char* server_ip, uint16_t port) {
    printf("[WiFi] Connecting to server %s:%d...\n", server_ip, port);
    
    // Parse server IP
    if (!ip4addr_aton(server_ip, &g_server_addr)) {
        printf("[WiFi] Invalid server IP address\n");
        return false;
    }
    g_server_port = port;
    
#if USE_TCP
    // Create TCP connection
    g_tcp_pcb = tcp_new();
    if (!g_tcp_pcb) {
        printf("[WiFi] Failed to create TCP PCB\n");
        return false;
    }
    
    // Set callbacks
    tcp_err(g_tcp_pcb, tcp_error_callback);
    tcp_recv(g_tcp_pcb, tcp_recv_callback);
    
    // Initiate connection
    err_t err = tcp_connect(g_tcp_pcb, &g_server_addr, port, tcp_connected_callback);
    if (err != ERR_OK) {
        printf("[WiFi] TCP connect failed: %d\n", err);
        tcp_close(g_tcp_pcb);
        g_tcp_pcb = NULL;
        return false;
    }
    
    // Wait for connection (with timeout)
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (!g_state.connected && (to_ms_since_boot(get_absolute_time()) - start) < 10000) {
        cyw43_arch_poll();
        sleep_ms(10);
    }
    
    if (!g_state.connected) {
        printf("[WiFi] Connection timeout\n");
        return false;
    }
    
#else
    // Create UDP connection
    g_udp_pcb = udp_new();
    if (!g_udp_pcb) {
        printf("[WiFi] Failed to create UDP PCB\n");
        return false;
    }
    
    // Bind to any local port
    udp_bind(g_udp_pcb, IP_ADDR_ANY, 0);
    
    // Set receive callback
    udp_recv(g_udp_pcb, udp_recv_callback, NULL);
    
    // Connect (for UDP, this just sets the default destination)
    err_t err = udp_connect(g_udp_pcb, &g_server_addr, port);
    if (err != ERR_OK) {
        printf("[WiFi] UDP connect failed: %d\n", err);
        return false;
    }
    
    g_state.connected = true;
#endif
    
    printf("[WiFi] Connected to server!\n");
    return true;
}

bool wifi_camera_send_frame(const uint8_t* frame_data, uint16_t width, uint16_t height) {
    if (!g_state.connected) {
        return false;
    }
    
    size_t data_size = width * height * FRAME_CHANNELS;
    
    // Build frame header
    frame_header_t header = {
        .magic = 0xCAFEBABE,
        .frame_id = g_state.frame_count,
        .width = width,
        .height = height,
        .format = 0,  // YUV422
        .data_size = (uint16_t)data_size,
        .checksum = wifi_camera_checksum(frame_data, data_size)
    };
    
#if USE_TCP
    // Send header
    err_t err = tcp_write(g_tcp_pcb, &header, sizeof(header), TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        printf("[WiFi] Failed to send header: %d\n", err);
        g_state.error_count++;
        return false;
    }
    
    // Send frame data in chunks
    size_t offset = 0;
    while (offset < data_size) {
        size_t chunk = data_size - offset;
        if (chunk > MAX_PACKET_SIZE) {
            chunk = MAX_PACKET_SIZE;
        }
        
        // Wait for send buffer space
        while (tcp_sndbuf(g_tcp_pcb) < chunk) {
            cyw43_arch_poll();
            sleep_ms(1);
        }
        
        err = tcp_write(g_tcp_pcb, frame_data + offset, chunk, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            printf("[WiFi] Failed to send data chunk: %d\n", err);
            g_state.error_count++;
            return false;
        }
        
        offset += chunk;
    }
    
    // Flush the data
    tcp_output(g_tcp_pcb);
    
#else
    // UDP: Send header + data (may need fragmentation for large frames)
    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, sizeof(header), PBUF_RAM);
    if (!p) {
        g_state.error_count++;
        return false;
    }
    
    memcpy(p->payload, &header, sizeof(header));
    udp_send(g_udp_pcb, p);
    pbuf_free(p);
    
    // Send data in chunks
    size_t offset = 0;
    while (offset < data_size) {
        size_t chunk = data_size - offset;
        if (chunk > MAX_PACKET_SIZE) {
            chunk = MAX_PACKET_SIZE;
        }
        
        p = pbuf_alloc(PBUF_TRANSPORT, chunk, PBUF_RAM);
        if (!p) {
            g_state.error_count++;
            return false;
        }
        
        memcpy(p->payload, frame_data + offset, chunk);
        udp_send(g_udp_pcb, p);
        pbuf_free(p);
        
        offset += chunk;
    }
#endif
    
    g_state.frame_count++;
    return true;
}

bool wifi_camera_receive_response(pose_response_t* response, uint32_t timeout_ms) {
    g_response_received = false;
    g_recv_len = 0;
    
    uint32_t start = to_ms_since_boot(get_absolute_time());
    
    while (!g_response_received) {
        cyw43_arch_poll();
        
        if ((to_ms_since_boot(get_absolute_time()) - start) > timeout_ms) {
            return false;  // Timeout
        }
        
        sleep_ms(1);
    }
    
    if (g_recv_len >= sizeof(pose_response_t)) {
        memcpy(response, g_recv_buffer, sizeof(pose_response_t));
        
        // Verify magic number
        if (response->magic != 0xDEADBEEF) {
            return false;
        }
        
        // Update last pose
        memcpy(&g_state.last_pose, response, sizeof(pose_response_t));
        
        // Track pose validity for unlock
        if (response->pose_valid) {
            if (!g_pose_was_valid) {
                g_pose_valid_start_ms = to_ms_since_boot(get_absolute_time());
                g_pose_was_valid = true;
            }
        } else {
            g_pose_was_valid = false;
            g_pose_valid_start_ms = 0;
        }
        
        return true;
    }
    
    return false;
}

bool wifi_camera_is_unlock_ready(void) {
    if (!g_pose_was_valid) {
        return false;
    }
    
    uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - g_pose_valid_start_ms;
    return (elapsed >= 2000);  // 2 seconds of valid pose
}

wifi_camera_state_t* wifi_camera_get_state(void) {
    return &g_state;
}

void wifi_camera_disconnect(void) {
#if USE_TCP
    if (g_tcp_pcb) {
        tcp_close(g_tcp_pcb);
        g_tcp_pcb = NULL;
    }
#else
    if (g_udp_pcb) {
        udp_remove(g_udp_pcb);
        g_udp_pcb = NULL;
    }
#endif
    
    g_state.connected = false;
    cyw43_arch_deinit();
    
    printf("[WiFi] Disconnected\n");
}

uint32_t wifi_camera_checksum(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

// ============================================================================
// Private Callback Functions
// ============================================================================

static err_t tcp_connected_callback(void* arg, struct tcp_pcb* tpcb, err_t err) {
    (void)arg;
    (void)tpcb;
    
    if (err == ERR_OK) {
        g_state.connected = true;
        printf("[WiFi] TCP connected\n");
    } else {
        printf("[WiFi] TCP connection error: %d\n", err);
    }
    
    return ERR_OK;
}

static err_t tcp_recv_callback(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err) {
    (void)arg;
    
    if (p == NULL) {
        // Connection closed by server
        g_state.connected = false;
        tcp_close(tpcb);
        g_tcp_pcb = NULL;
        return ERR_OK;
    }
    
    if (err == ERR_OK && p->tot_len > 0) {
        size_t copy_len = p->tot_len;
        if (copy_len > sizeof(g_recv_buffer)) {
            copy_len = sizeof(g_recv_buffer);
        }
        
        pbuf_copy_partial(p, g_recv_buffer, copy_len, 0);
        g_recv_len = copy_len;
        g_response_received = true;
    }
    
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    
    return ERR_OK;
}

static void tcp_error_callback(void* arg, err_t err) {
    (void)arg;
    
    printf("[WiFi] TCP error: %d\n", err);
    g_state.connected = false;
    g_tcp_pcb = NULL;
    g_state.error_count++;
}

static void udp_recv_callback(void* arg, struct udp_pcb* pcb, struct pbuf* p,
                               const ip_addr_t* addr, u16_t port) {
    (void)arg;
    (void)pcb;
    (void)addr;
    (void)port;
    
    if (p != NULL) {
        size_t copy_len = p->tot_len;
        if (copy_len > sizeof(g_recv_buffer)) {
            copy_len = sizeof(g_recv_buffer);
        }
        
        pbuf_copy_partial(p, g_recv_buffer, copy_len, 0);
        g_recv_len = copy_len;
        g_response_received = true;
        
        pbuf_free(p);
    }
}
