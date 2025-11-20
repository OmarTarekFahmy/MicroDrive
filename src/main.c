#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include <stdint.h>
#include <stdio.h>

// #include "servo.h"
// #include "lcd_i2c.h"
// #include "touch_sensor.h"
// #include "hardware/i2c.h"

// Pin definitions
#define PWM_PIN 15  // GPIO 15 for PWM test

// Global variables for PWM control
static uint slice_num;
static uint channel;
static uint32_t wrap;

// Function to move servo with specific pulse width in microseconds
void move_servo(uint32_t pulse_us) {
    // Calculate PWM level for the given pulse width
    // pulse_us = desired pulse width in microseconds (e.g., 1000-2000)
    // Period is 20ms = 20000us for 50Hz
    uint32_t level = (pulse_us * wrap) / 20000;
    pwm_set_chan_level(slice_num, channel, level);
}

// // Pin definitions
// #define TOUCH_SENSOR_PIN 6   // GPIO 6 for touch sensor
// #define LED_PIN          14  // GPIO 15 for LED

// int main() {
//     stdio_init_all();
//     
//     // Wait a bit for USB serial
//     sleep_ms(2000);
//     
//     printf("Starting Touch + LCD + LED System...\n");
//     
//     // Initialize LED on GPIO 14
//     gpio_init(LED_PIN);
//     gpio_set_dir(LED_PIN, GPIO_OUT);
//     gpio_put(LED_PIN, 0); // Start with LED off
//     printf("LED initialized on GPIO %d\n", LED_PIN);
//     
//     // Test LED - blink 3 times to verify it works
//     printf("Testing LED - blinking 3 times...\n");
//     for (int i = 0; i < 3; i++) {
//         gpio_put(LED_PIN, 1); // LED ON
//         printf("LED ON\n");
//         sleep_ms(500);
//         gpio_put(LED_PIN, 0); // LED OFF
//         printf("LED OFF\n");
//         sleep_ms(500);
//     }
//     printf("LED test complete.\n");
//     
//     // Initialize touch sensor
//     touch_init(TOUCH_SENSOR_PIN);
//     printf("Touch sensor initialized on GPIO %d\n", TOUCH_SENSOR_PIN);
//     
//     // Scan for I2C devices
//     i2c_init(i2c0, 100000);
//     gpio_set_function(4, GPIO_FUNC_I2C);
//     gpio_set_function(5, GPIO_FUNC_I2C);
//     gpio_pull_up(4);
//     gpio_pull_up(5);
//     
//     printf("Scanning I2C bus...\n");
//     bool found = false;
//     uint8_t found_addr = 0;
//     
//     for (uint8_t addr = 0; addr < 128; addr++) {
//         uint8_t data;
//         int ret = i2c_read_blocking(i2c0, addr, &data, 1, false);
//         if (ret >= 0) {
//             printf("Found I2C device at address 0x%02X\n", addr);
//             found = true;
//             found_addr = addr;
//         }
//     }
//     
//     if (!found) {
//         printf("No I2C devices found! Check wiring.\n");
//     }
//     
//     // Initialize I2C LCD with found address (or try 0x27 if not found)
//     uint8_t lcd_addr = found ? found_addr : 0x27;
//     printf("Initializing LCD at address 0x%02X...\n", lcd_addr);
//     
//     lcd_i2c_init(i2c0, 4, 5, lcd_addr);
//     
//     printf("LCD initialized. System ready!\n");
//     
//     // Display initial screen
//     lcd_i2c_clear();
//     lcd_i2c_set_cursor(0, 0);
//     lcd_i2c_print("Touch Sensor:");
//     lcd_i2c_set_cursor(1, 0);
//     lcd_i2c_print("Status: Ready");
//     
//     sleep_ms(2000);
//     
//     // Display touch status screen
//     lcd_i2c_clear();
//     lcd_i2c_set_cursor(0, 0);
//     lcd_i2c_print("Touch: No");
//     lcd_i2c_set_cursor(1, 0);
//     lcd_i2c_print("LED: OFF");
//     
//     int touch_count = 0;
//     bool last_touch_state = false;
//     
//     
//     // Main loop: monitor touch sensor and control LED
//     while (1) {
//         // Read touch sensor with debouncing
//         bool is_touched = touch_read_debounced();
//         
//         // Control LED based on touch
//         if (is_touched) {
//             gpio_put(LED_PIN, 1); // Turn LED ON
//             
//             // Count touch events (on rising edge)
//             if (!last_touch_state) {
//                 touch_count++;
//                 printf("Touch detected! Count: %d\n", touch_count);
//             }
//             
//             // Update LCD
//             lcd_i2c_set_cursor(0, 0);
//             lcd_i2c_print("Touch: YES       ");
//             lcd_i2c_set_cursor(1, 0);
//             lcd_i2c_print("LED: ON  #");
//             lcd_i2c_print_number(touch_count);
//             lcd_i2c_print("   ");
//         } else {
//             gpio_put(LED_PIN, 0); // Turn LED OFF
//             
//             // Update LCD only if state changed
//             if (last_touch_state) {
//                 lcd_i2c_set_cursor(0, 0);
//                 lcd_i2c_print("Touch: NO        ");
//                 lcd_i2c_set_cursor(1, 0);
//                 lcd_i2c_print("LED: OFF #");
//                 lcd_i2c_print_number(touch_count);
//                 lcd_i2c_print("   ");
//             }
//         }
//         
//         last_touch_state = is_touched;
//         
//         // Small delay
//         sleep_ms(10);
//     }
// }

int main() {
    stdio_init_all();
    
    // Wait a bit for USB serial
    sleep_ms(2000);
    
    printf("Starting Servo Control on GPIO %d...\n", PWM_PIN);
    
    // Set GPIO 15 to PWM function
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    
    // Find out which PWM slice is connected to GPIO 15
    slice_num = pwm_gpio_to_slice_num(PWM_PIN);
    channel = pwm_gpio_to_channel(PWM_PIN);
    
    printf("PWM Slice: %d, Channel: %d\n", slice_num, channel);
    
    // Get system clock frequency
    uint32_t sys_clk = clock_get_hz(clk_sys);
    printf("System Clock: %u Hz\n", sys_clk);
    
    // Configure PWM for 50Hz (servo frequency)
    float divider = 64.0f;
    wrap = (sys_clk / (divider * 50)) - 1;
    
    printf("PWM Divider: %.1f, Wrap: %u\n", divider, wrap);
    
    pwm_set_clkdiv(slice_num, divider);
    pwm_set_wrap(slice_num, wrap);
    
    // Enable PWM
    pwm_set_enabled(slice_num, true);
    
    printf("\nStarting continuous servo movement...\n");
    
    // Continuous loop: move servo back and forth
    while (1) {
        // Move clockwise
        printf("Moving CW (1000us)\n");
        move_servo(1000);
        sleep_ms(2000);
        
        // Stop
        printf("Stop (1500us)\n");
        move_servo(1500);
        sleep_ms(1000);
        
        // Move counter-clockwise
        printf("Moving CCW (2000us)\n");
        move_servo(2000);
        sleep_ms(2000);
        
        // Stop
        printf("Stop (1500us)\n");
        move_servo(1500);
        sleep_ms(1000);
    }
}
