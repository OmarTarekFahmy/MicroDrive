/* 
 * OV7670 WiFi Camera - Simple approach using working USB camera code
 * Same camera capture as USB version, just send over WiFi instead
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "ov7670.h"

// Frame size (320x240 QVGA, YUV422 = 2 bytes per pixel)
#define FRAME_WIDTH 320
#define FRAME_HEIGHT 240

// WiFi credentials
#define WIFI_SSID "277353"
#define WIFI_PASSWORD "2004ahmed"
#define SERVER_IP "172.20.10.3"
#define SERVER_PORT 8888

// Frame buffer - same as USB camera
static uint8_t frame_buffer[FRAME_WIDTH * FRAME_HEIGHT * 2];  // YUY2 format

// Camera configuration - EXACT SAME AS WORKING USB CAMERA
static struct ov2640_config camera_config = {
    .sccb = i2c0,
    .pin_sioc = 21,
    .pin_siod = 4,
    .pin_resetb = 17,
    .pin_xclk = 3,
    .pin_vsync = 16,
    .pin_y2_pio_base = 6,
    .pio = pio0,              // Same as USB camera
    .pio_sm = 0,              // Same as USB camera
    .dma_channel = 0,         // Same as USB camera
    .image_buf = frame_buffer,
    .image_buf_size = sizeof(frame_buffer)
};

// WiFi/TCP state
static struct tcp_pcb *client_pcb = NULL;
static bool connected = false;
static uint32_t frame_count = 0;

// Frame header matching server expectations
typedef struct __attribute__((packed)) {
    uint32_t magic;        // 0xCAFEBABE
    uint32_t frame_id;     // Sequential frame number
    uint16_t width;        // Image width
    uint16_t height;       // Image height
    uint16_t format;       // 0 = YUV422
    uint16_t reserved;     // padding/reserved (set to 0)
    uint32_t data_size;    // Size of image data (full 32-bit)
    uint32_t checksum;     // Simple checksum
} frame_header_t;

_Static_assert(sizeof(frame_header_t) == 24, "frame_header_t must be 24 bytes");



// Simple checksum
uint32_t calculate_checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

// Simple TCP send with proper flow control
bool send_frame_tcp(const uint8_t *data, size_t len) {
    if (!connected || !client_pcb) {
        return false;
    }
    
    // Build proper header
     frame_header_t header = {
        .magic     = 0xCAFEBABE,
        .frame_id  = frame_count++,
        .width     = FRAME_WIDTH,
        .height    = FRAME_HEIGHT,
        .format    = 0,                 // YUV422
        .reserved  = 0,
        .data_size = (uint32_t)len,     // <<< IMPORTANT: no cast to uint16_t
        .checksum  = calculate_checksum(data, len),
    };

    
    // Wait for send buffer space for header
    while (tcp_sndbuf(client_pcb) < sizeof(header)) {
        cyw43_arch_poll();
        sleep_ms(1);
    }
    
    err_t err = tcp_write(client_pcb, &header, sizeof(header), TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        printf("[TCP] Header write failed: %d\n", err);
        return false;
    }
    tcp_output(client_pcb);
    
    // Send frame data in chunks with flow control
    size_t sent = 0;
    while (sent < len) {
        // Wait for send buffer space
        u16_t available = tcp_sndbuf(client_pcb);
        if (available == 0) {
            cyw43_arch_poll();
            sleep_ms(1);
            continue;
        }
        
        // Send what we can
        size_t to_send = len - sent;
        if (to_send > available) {
            to_send = available;
        }
        if (to_send > 1460) {
            to_send = 1460;  // MSS limit
        }
        
        err = tcp_write(client_pcb, data + sent, to_send, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            printf("[TCP] Data write failed at %zu: %d\n", sent, err);
            return false;
        }
        
        sent += to_send;
        tcp_output(client_pcb);
        cyw43_arch_poll();
    }
    
    // Final flush
    tcp_output(client_pcb);
    
    // Wait for all data to be sent
    for (int i = 0; i < 100; i++) {
        if (tcp_sndqueuelen(client_pcb) == 0) {
            break;
        }
        cyw43_arch_poll();
        sleep_ms(10);
    }
    
    return true;
}

// TCP callbacks
static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("[TCP] Connection failed: %d\n", err);
        connected = false;
        return err;
    }
    printf("[TCP] Connected to server!\n");
    connected = true;
    return ERR_OK;
}

// Connect to server
bool connect_to_server(const char *ip, uint16_t port) {
    client_pcb = tcp_new();
    if (!client_pcb) {
        printf("[TCP] Failed to create PCB\n");
        return false;
    }
    
    ip_addr_t server_addr;
    if (!ip4addr_aton(ip, &server_addr)) {
        printf("[TCP] Invalid IP address\n");
        return false;
    }
    
    tcp_arg(client_pcb, NULL);
    err_t err = tcp_connect(client_pcb, &server_addr, port, tcp_client_connected);
    if (err != ERR_OK) {
        printf("[TCP] Connect failed: %d\n", err);
        return false;
    }
    
    // Wait for connection
    int timeout = 100;
    while (!connected && timeout-- > 0) {
        cyw43_arch_poll();
        sleep_ms(100);
    }
    
    return connected;
}

int main(void) {
    stdio_init_all();
    sleep_ms(3000);
    
    printf("\n\n=================================\n");
    printf("OV7670 WiFi Camera (Simple)\n");
    printf("=================================\n\n");
    
    // Initialize WiFi
    printf("[WiFi] Initializing...\n");
    if (cyw43_arch_init()) {
        printf("[ERROR] WiFi init failed\n");
        return -1;
    }
    
    cyw43_arch_enable_sta_mode();
    
    printf("[WiFi] Connecting to '%s'...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, 
                                            CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("[ERROR] WiFi connection failed\n");
        return -1;
    }
    printf("[WiFi] Connected!\n");
    
    // Connect to server
    printf("[TCP] Connecting to %s:%d...\n", SERVER_IP, SERVER_PORT);
    if (!connect_to_server(SERVER_IP, SERVER_PORT)) {
        printf("[ERROR] Server connection failed\n");
        return -1;
    }
    
    // Initialize camera - SAME AS USB VERSION
    printf("[Camera] Initializing...\n");
    ov2640_init(&camera_config);
    
    uint8_t pid = ov2640_reg_read(&camera_config, 0x0A);
    uint8_t ver = ov2640_reg_read(&camera_config, 0x0B);
    printf("[Camera] ID: PID=0x%02X VER=0x%02X\n", pid, ver);
    printf("[Camera] Ready! %dx%d\n", FRAME_WIDTH, FRAME_HEIGHT);
    
    printf("\n[Main] Starting capture loop...\n\n");
    
    uint32_t frame_num = 0;
    
    // Main loop - simple and clean
    while (1) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        
        printf("[Frame %lu] Capturing...\n", frame_num);
        
        // Use the WORKING capture function from USB camera
        ov2640_capture_frame(&camera_config);
        
        printf("[Frame %lu] Sending %d bytes...\n", frame_num, sizeof(frame_buffer));
        
        if (send_frame_tcp(frame_buffer, sizeof(frame_buffer))) {
            printf("[Frame %lu] ✓ Sent successfully\n", frame_num);
        } else {
            printf("[Frame %lu] ✗ Send failed\n", frame_num);
        }
        
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        
        frame_num++;
        sleep_ms(2000);  // 2 second delay between frames
    }
    
    return 0;
}
