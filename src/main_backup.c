/**
 * @file main.c
 * @brief OV7670 Camera Module Demo
 * 
 * Captures frames from OV7670 camera and sends them over USB CDC
 * for display on a PC viewer application.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "ov7670.h"

// Frame buffer for camera data
static uint8_t frame_buffer[OV7670_FRAME_SIZE];

int main() {
    // Initialize USB serial
    stdio_init_all();
    
    // Wait for USB connection
    sleep_ms(6000);
    
    printf("\n\n");
    printf("===========================================\n");
    printf("  OV7670 Camera Module Demo\n");
    printf("  Resolution: %dx%d (QQVGA)\n", OV7670_WIDTH, OV7670_HEIGHT);
    printf("  Format: Grayscale (8-bit Y)\n");
    printf("===========================================\n\n");
    
    // Initialize camera
    if (!ov7670_init()) {
        printf("ERROR: Camera initialization failed!\n");
        printf("Please check:\n");
        printf("  - Camera module is connected properly\n");
        printf("  - I2C wiring (SDA=%d, SCL=%d)\n", OV7670_PIN_SIOD, OV7670_PIN_SIOC);
        printf("  - Power supply (3.3V to camera module)\n");
        printf("  - All data pins are connected (D0-D7)\n");
        while (1) {
            tight_loop_contents();
        }
    }
    
    printf("Camera initialized successfully!\n");
    printf("Starting frame capture loop...\n\n");
    
    uint32_t frame_count = 0;
    
    // Main loop: capture and send frames
    while (true) {
        // Capture frame from camera
        if (ov7670_capture_frame(frame_buffer, OV7670_FRAME_SIZE)) {
            frame_count++;
            printf("Frame %lu captured\n", frame_count);
            
            // Send frame to PC via USB CDC
            ov7670_send_frame(frame_buffer, OV7670_FRAME_SIZE);
            
            // Small delay between frames (~5 FPS)
            sleep_ms(200);
        } else {
            printf("Frame capture failed!\n");
            sleep_ms(500);
        }
    }
    
    return 0;
}
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

/*
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
*/