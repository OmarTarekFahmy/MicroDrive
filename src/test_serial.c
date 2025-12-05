/**
 * @file test_serial.c
 * @brief Simple serial test to verify USB communication
 * 
 * This program just prints messages repeatedly to verify
 * that serial communication is working properly.
 */

#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    // Initialize USB serial
    stdio_init_all();
    
    // Wait longer for serial connection to establish
    sleep_ms(5000);
    
    printf("\n\n\n");
    printf("========================================\n");
    printf("    PICO W SERIAL TEST\n");
    printf("========================================\n");
    printf("If you see this, USB serial is working!\n");
    printf("Board: Pico W (RP2040)\n");
    printf("Date: December 5, 2025\n");
    printf("========================================\n\n");
    

    
    uint32_t counter = 0;
    
    while (true) {
        // Print message
        printf("[%lu] Hello from Pico W! Serial is working.\n", counter);
        
        // Toggle LED
        
        // Wait 1 second
        sleep_ms(1000);
        
        counter++;
        
        // Print extra info every 5 seconds
        if (counter % 5 == 0) {
            printf("--- 5 seconds elapsed. Total messages: %lu ---\n", counter);
        }
    }
    
    return 0;
}
