/**
 * @file test_wifi_simple.c
 * @brief Simple WiFi connection test (no camera)
 * 
 * Tests WiFi connectivity without camera complexity.
 * Connects to WiFi and attempts to reach server.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"

// WiFi credentials
#define WIFI_SSID       "WE_3FAA50"
#define WIFI_PASSWORD   "7570ddaf"    // CORRECTED PASSWORD

// Server settings
#define SERVER_IP       "192.168.100.3"
#define SERVER_PORT     8888

int main() {
    // Initialize USB serial
    stdio_init_all();
    sleep_ms(5000);  // Wait for serial connection
    
    printf("\n\n\n");
    printf("========================================\n");
    printf("    WiFi Connection Test\n");
    printf("========================================\n");
    printf("Board: Pico W (RP2040)\n");
    printf("SSID: %s\n", WIFI_SSID);
    printf("Server: %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("========================================\n\n");
    
    // Initialize onboard LED
    printf("[LED] Initializing onboard LED...\n");
    
    // Initialize CYW43 (WiFi + LED)
    printf("[WiFi] Initializing CYW43 driver...\n");
    if (cyw43_arch_init()) {
        printf("[ERROR] Failed to initialize CYW43!\n");
        while (1) {
            sleep_ms(1000);
        }
    }
    printf("[WiFi] ✓ CYW43 driver initialized\n");
    
    // Blink LED to show we're alive
    printf("[LED] Blinking LED...\n");
    for (int i = 0; i < 3; i++) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(200);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(200);
    }
    printf("[LED] ✓ LED test complete\n\n");
    
    // Enable station mode
    printf("[WiFi] Enabling station mode...\n");
    cyw43_arch_enable_sta_mode();
    printf("[WiFi] ✓ Station mode enabled\n");
    
    // Connect to WiFi
    printf("[WiFi] Connecting to '%s'...\n", WIFI_SSID);
    printf("[WiFi] This may take 10-15 seconds...\n");
    
    int result = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID, 
        WIFI_PASSWORD, 
        CYW43_AUTH_WPA2_AES_PSK,
        30000  // 30 second timeout
    );
    
    if (result != 0) {
        printf("[ERROR] WiFi connection failed! Error code: %d\n", result);
        printf("[ERROR] Possible reasons:\n");
        printf("  - Wrong SSID or password\n");
        printf("  - WiFi router too far\n");
        printf("  - 5GHz network (Pico W only supports 2.4GHz)\n");
        printf("  - Router security settings\n");
        
        // Blink LED rapidly to indicate error
        while (1) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            sleep_ms(100);
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            sleep_ms(100);
        }
    }
    
    printf("[WiFi] ✓ Connected to WiFi!\n");
    
    // Get and print IP address
    struct netif *netif = &cyw43_state.netif[CYW43_ITF_STA];
    printf("[WiFi] IP Address: %s\n", ip4addr_ntoa(&netif->ip_addr));
    printf("[WiFi] Netmask:    %s\n", ip4addr_ntoa(&netif->netmask));
    printf("[WiFi] Gateway:    %s\n", ip4addr_ntoa(&netif->gw));
    
    // Try to reach server
    printf("\n[TCP] Attempting to connect to %s:%d...\n", SERVER_IP, SERVER_PORT);
    printf("[TCP] Make sure Python server is running!\n");
    
    struct tcp_pcb* pcb = tcp_new();
    if (!pcb) {
        printf("[ERROR] Failed to create TCP PCB\n");
        while (1) { sleep_ms(1000); }
    }
    
    ip_addr_t server_addr;
    if (!ip4addr_aton(SERVER_IP, &server_addr)) {
        printf("[ERROR] Invalid server IP address\n");
        while (1) { sleep_ms(1000); }
    }
    
    printf("[TCP] Connecting...\n");
    err_t err = tcp_connect(pcb, &server_addr, SERVER_PORT, NULL);
    
    if (err != ERR_OK) {
        printf("[ERROR] tcp_connect failed: %d\n", err);
        printf("[ERROR] Check if server is running!\n");
    } else {
        printf("[TCP] Connection initiated...\n");
        printf("[TCP] Waiting for connection to establish...\n");
        
        // Give it some time
        sleep_ms(2000);
        
        if (pcb->state == ESTABLISHED) {
            printf("[TCP] ✓ Connected to server!\n");
            printf("\n========================================\n");
            printf("    SUCCESS! WiFi is working!\n");
            printf("========================================\n\n");
            
            // Send test message
            const char* msg = "Hello from Pico W!\n";
            tcp_write(pcb, msg, strlen(msg), TCP_WRITE_FLAG_COPY);
            tcp_output(pcb);
            
            printf("[TCP] Sent test message\n");
        } else {
            printf("[TCP] Connection not established. State: %d\n", pcb->state);
            printf("[TCP] Server might not be running or unreachable\n");
        }
    }
    
    printf("\n[Main] Test complete. LED will blink slowly.\n");
    printf("[Main] Press RESET to run test again.\n\n");
    
    // Slow blink loop
    uint32_t counter = 0;
    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        printf("[%lu] Still alive... WiFi connected\n", counter++);
        sleep_ms(1000);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(1000);
    }
    
    return 0;
}
