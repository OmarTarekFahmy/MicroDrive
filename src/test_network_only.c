/**
 * @file test_network_only.c
 * @brief Test WiFi and network connectivity without camera
 * 
 * This test connects to WiFi, establishes TCP connection to server,
 * and sends a test message. Use this to verify network setup before
 * testing the camera.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"

// WiFi credentials
#define WIFI_SSID       "WE_3FAA50"
#define WIFI_PASSWORD   "7570ddaf"

// Server settings
#define SERVER_IP       "192.168.100.3"
#define SERVER_PORT     8888

static bool tcp_connected = false;
static struct tcp_pcb* client_pcb = NULL;

static err_t tcp_connected_callback(void* arg, struct tcp_pcb* tpcb, err_t err) {
    if (err == ERR_OK) {
        printf("[TCP] ✓ Connected to server!\n");
        tcp_connected = true;
        
        // Send test message
        const char* test_msg = "Hello from Pico W - Network Test!\n";
        err_t write_err = tcp_write(tpcb, test_msg, strlen(test_msg), TCP_WRITE_FLAG_COPY);
        if (write_err == ERR_OK) {
            tcp_output(tpcb);
            printf("[TCP] Test message sent: %s", test_msg);
        } else {
            printf("[TCP] Failed to send message: %d\n", write_err);
        }
    } else {
        printf("[TCP] Connection failed: %d\n", err);
    }
    return ERR_OK;
}

static void tcp_error_callback(void* arg, err_t err) {
    printf("[TCP] Error callback: %d\n", err);
    tcp_connected = false;
}

int main() {
    // Initialize USB serial
    stdio_init_all();
    sleep_ms(5000);
    
    printf("\n\n\n");
    printf("========================================\n");
    printf("    Network Connectivity Test\n");
    printf("========================================\n");
    printf("WiFi: %s\n", WIFI_SSID);
    printf("Server: %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("========================================\n\n");
    
    // Initialize CYW43 (WiFi)
    printf("[WiFi] Initializing CYW43...\n");
    if (cyw43_arch_init()) {
        printf("[ERROR] CYW43 init failed!\n");
        while (1) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            sleep_ms(100);
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            sleep_ms(100);
        }
    }
    printf("[WiFi] ✓ CYW43 initialized\n");
    
    // Enable station mode
    cyw43_arch_enable_sta_mode();
    
    // Blink LED 3 times
    printf("[LED] Blinking LED...\n");
    for (int i = 0; i < 3; i++) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(200);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(200);
    }
    
    // Connect to WiFi
    printf("[WiFi] Connecting to '%s'...\n", WIFI_SSID);
    int result = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID,
        WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK,
        30000
    );
    
    if (result != 0) {
        printf("[ERROR] WiFi connection failed: %d\n", result);
        while (1) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            sleep_ms(100);
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            sleep_ms(100);
        }
    }
    
    // Get IP address
    struct netif *netif = &cyw43_state.netif[CYW43_ITF_STA];
    printf("[WiFi] ✓ Connected!\n");
    printf("[WiFi] IP Address: %s\n", ip4addr_ntoa(&netif->ip_addr));
    printf("[WiFi] Gateway:    %s\n", ip4addr_ntoa(&netif->gw));
    printf("[WiFi] Netmask:    %s\n", ip4addr_ntoa(&netif->netmask));
    
    // Parse server IP
    ip_addr_t server_addr;
    if (!ip4addr_aton(SERVER_IP, &server_addr)) {
        printf("[ERROR] Invalid server IP\n");
        return -1;
    }
    
    // Create TCP connection
    printf("\n[TCP] Creating connection to %s:%d...\n", SERVER_IP, SERVER_PORT);
    printf("[TCP] Make sure Python server is running!\n");
    
    client_pcb = tcp_new();
    if (!client_pcb) {
        printf("[ERROR] Failed to create TCP PCB\n");
        return -1;
    }
    
    tcp_err(client_pcb, tcp_error_callback);
    
    err_t err = tcp_connect(client_pcb, &server_addr, SERVER_PORT, tcp_connected_callback);
    if (err != ERR_OK) {
        printf("[TCP] tcp_connect failed: %d\n", err);
        return -1;
    }
    
    printf("[TCP] Connection initiated, waiting...\n");
    
    // Wait for connection
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (!tcp_connected && (to_ms_since_boot(get_absolute_time()) - start) < 10000) {
        cyw43_arch_poll();
        sleep_ms(10);
    }
    
    if (!tcp_connected) {
        printf("[TCP] ✗ Connection timeout\n");
        printf("[TCP] Server may not be running or unreachable\n");
        printf("\n[Diagnostic] Checking connection...\n");
        printf("  Pico IP:    %s\n", ip4addr_ntoa(&netif->ip_addr));
        printf("  Server IP:  %s\n", SERVER_IP);
        printf("  Server Port: %d\n", SERVER_PORT);
        printf("\n[Action Required]\n");
        printf("  1. Check Python server is running:\n");
        printf("     python tools/aruco_verification_server.py\n");
        printf("  2. Verify server IP matches laptop IP:\n");
        printf("     ipconfig | findstr IPv4\n");
        printf("  3. Check firewall allows port %d\n", SERVER_PORT);
        printf("  4. Ping Pico from laptop: ping %s\n", ip4addr_ntoa(&netif->ip_addr));
    } else {
        printf("\n========================================\n");
        printf("    ✓✓✓ SUCCESS! ✓✓✓\n");
        printf("========================================\n");
        printf("Network is fully operational!\n");
        printf("Pico can communicate with server.\n");
        printf("\nNext step: Test with camera\n");
        printf("  Build and flash: wifi_camera.uf2\n");
        printf("========================================\n\n");
        
        // Keep connection alive and blink LED slowly
        printf("[Main] Keeping connection alive...\n");
        printf("[Main] LED blinking = connection active\n");
        printf("[Main] Press RESET to restart\n\n");
        
        uint32_t count = 0;
        while (true) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            printf("[%lu] Connection alive\n", count++);
            sleep_ms(1000);
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            sleep_ms(1000);
            
            // Poll network stack
            cyw43_arch_poll();
        }
    }
    
    return 0;
}
