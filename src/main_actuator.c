#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

// WiFi credentials - SAME AS CAMERA
#define WIFI_SSID "277353"
#define WIFI_PASSWORD "2004ahmed"
#define SERVER_IP "172.20.10.3"
#define ACTUATOR_PORT 9999

// Command from server
#define CMD_UNLOCK 0x01

// TCP state
static struct tcp_pcb *client_pcb = NULL;
static bool connected = false;
static uint32_t unlock_count = 0;

// TCP callbacks
static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        printf("[TCP] Connection closed by server\n");
        tcp_close(tpcb);
        connected = false;
        return ERR_OK;
    }
    
    // Process received data
    uint8_t *data = (uint8_t *)p->payload;
    for (uint16_t i = 0; i < p->len; i++) {
        if (data[i] == CMD_UNLOCK) {
            unlock_count++;
            printf("\n");
            printf("========================================\n");
            printf("🔓 UNLOCK SIGNAL RECEIVED! (Count: %lu)\n", unlock_count);
            printf("========================================\n");
            printf("\n");
            
            // TODO: Trigger your motor/servo/relay here
            // Example: unlock_door();
            // Example: servo_move_to_position(90);
            // Example: gpio_put(RELAY_PIN, 1);
            
            // Blink LED to indicate unlock
            for (int j = 0; j < 5; j++) {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                sleep_ms(100);
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                sleep_ms(100);
            }
        } else {
            printf("[ACTUATOR] Unknown command: 0x%02X\n", data[i]);
        }
    }
    
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("[TCP] Connection failed: %d\n", err);
        connected = false;
        return err;
    }
    
    printf("[TCP] Connected to server!\n");
    connected = true;
    
    // Set receive callback
    tcp_recv(tpcb, tcp_client_recv);
    
    // Send ready message
    const char *ready_msg = "ACTUATOR_READY";
    err_t write_err = tcp_write(tpcb, ready_msg, strlen(ready_msg), TCP_WRITE_FLAG_COPY);
    if (write_err == ERR_OK) {
        tcp_output(tpcb);
        printf("[TCP] Sent ACTUATOR_READY message\n");
    }
    
    return ERR_OK;
}

static void tcp_client_err(void *arg, err_t err) {
    printf("[TCP] Connection error: %d\n", err);
    connected = false;
    client_pcb = NULL;
}

// Connect to server - SAME PATTERN AS CAMERA
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
    tcp_err(client_pcb, tcp_client_err);
    
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
    sleep_ms(2000);

    printf("\n\n=================================\n");
    printf("MicroDrive Actuator Client\n");
    printf("=================================\n\n");

    // Initialize WiFi - SAME AS CAMERA
    printf("[WiFi] Initializing...\n");
    if (cyw43_arch_init()) {
        printf("[ERROR] WiFi init failed\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();

    printf("[WiFi] Connecting to '%s'...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID,
            WIFI_PASSWORD,
            CYW43_AUTH_WPA2_AES_PSK,
            30000)) {
        printf("[ERROR] WiFi connection failed\n");
        cyw43_arch_deinit();
        return -1;
    }
    printf("[WiFi] Connected!\n");

    // Connect to actuator server port (9999)
    printf("[TCP] Connecting to %s:%d...\n", SERVER_IP, ACTUATOR_PORT);
    if (!connect_to_server(SERVER_IP, ACTUATOR_PORT)) {
        printf("[ERROR] Server connection failed\n");
        cyw43_arch_deinit();
        return -1;
    }

    printf("[Actuator] Waiting for unlock signals...\n");
    printf("[Actuator] Press Ctrl+C to exit\n\n");

    // Main loop - just poll and handle callbacks
    while (connected) {
        cyw43_arch_poll();
        sleep_ms(10);
    }

    // Cleanup
    if (client_pcb) {
        tcp_close(client_pcb);
    }
    cyw43_arch_deinit();
    
    printf("[Actuator] Exiting\n");
    return 0;
}
