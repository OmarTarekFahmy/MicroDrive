#include "pico/stdlib.h"
#include "lcd_i2c.h"
#include "touch_sensor.h"
#include "hardware/i2c.h"
#include <stdint.h>
#include <stdio.h>

// Pin definitions
#define TOUCH_SENSOR_PIN 6   // GPIO 6 for touch sensor
#define LED_PIN          14  // GPIO 15 for LED

int main() {
    stdio_init_all();
    
    // Wait a bit for USB serial
    sleep_ms(2000);
    
    printf("Starting Touch + LCD + LED System...\n");
    
    // Initialize LED on GPIO 14
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0); // Start with LED off
    printf("LED initialized on GPIO %d\n", LED_PIN);
    
    // Test LED - blink 3 times to verify it works
    printf("Testing LED - blinking 3 times...\n");
    for (int i = 0; i < 3; i++) {
        gpio_put(LED_PIN, 1); // LED ON
        printf("LED ON\n");
        sleep_ms(500);
        gpio_put(LED_PIN, 0); // LED OFF
        printf("LED OFF\n");
        sleep_ms(500);
    }
    printf("LED test complete.\n");
    
    // Initialize touch sensor
    touch_init(TOUCH_SENSOR_PIN);
    printf("Touch sensor initialized on GPIO %d\n", TOUCH_SENSOR_PIN);
    
    // Scan for I2C devices
    i2c_init(i2c0, 100000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);
    
    printf("Scanning I2C bus...\n");
    bool found = false;
    uint8_t found_addr = 0;
    
    for (uint8_t addr = 0; addr < 128; addr++) {
        uint8_t data;
        int ret = i2c_read_blocking(i2c0, addr, &data, 1, false);
        if (ret >= 0) {
            printf("Found I2C device at address 0x%02X\n", addr);
            found = true;
            found_addr = addr;
        }
    }
    
    if (!found) {
        printf("No I2C devices found! Check wiring.\n");
    }
    
    // Initialize I2C LCD with found address (or try 0x27 if not found)
    uint8_t lcd_addr = found ? found_addr : 0x27;
    printf("Initializing LCD at address 0x%02X...\n", lcd_addr);
    
    lcd_i2c_init(i2c0, 4, 5, lcd_addr);
    
    printf("LCD initialized. System ready!\n");
    
    // Display initial screen
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Touch Sensor:");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Status: Ready");
    
    sleep_ms(2000);
    
    // Display touch status screen
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Touch: No");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("LED: OFF");
    
    int touch_count = 0;
    bool last_touch_state = false;
    
    
    // Main loop: monitor touch sensor and control LED
    while (1) {
        // Read touch sensor with debouncing
        bool is_touched = touch_read_debounced();
        
        // Control LED based on touch
        if (is_touched) {
            gpio_put(LED_PIN, 1); // Turn LED ON
            
            // Count touch events (on rising edge)
            if (!last_touch_state) {
                touch_count++;
                printf("Touch detected! Count: %d\n", touch_count);
            }
            
            // Update LCD
            lcd_i2c_set_cursor(0, 0);
            lcd_i2c_print("Touch: YES       ");
            lcd_i2c_set_cursor(1, 0);
            lcd_i2c_print("LED: ON  #");
            lcd_i2c_print_number(touch_count);
            lcd_i2c_print("   ");
        } else {
            gpio_put(LED_PIN, 0); // Turn LED OFF
            
            // Update LCD only if state changed
            if (last_touch_state) {
                lcd_i2c_set_cursor(0, 0);
                lcd_i2c_print("Touch: NO        ");
                lcd_i2c_set_cursor(1, 0);
                lcd_i2c_print("LED: OFF #");
                lcd_i2c_print_number(touch_count);
                lcd_i2c_print("   ");
            }
        }
        
        last_touch_state = is_touched;
        
        // Small delay
        sleep_ms(10);
    }
}
