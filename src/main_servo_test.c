/**
 * Servo Motor Test Program
 * 
 * This program allows you to test servo motors by specifying exact pulse widths.
 * Use this to determine if your servo is positional (180°) or continuous (360°):
 * 
 * POSITIONAL SERVO behavior:
 *   - 1000 µs: Moves to -90° and HOLDS position
 *   - 1500 µs: Moves to 0° (center) and HOLDS position
 *   - 2000 µs: Moves to +90° and HOLDS position
 *   
 * CONTINUOUS SERVO behavior:
 *   - 1000 µs: Rotates continuously counter-clockwise
 *   - 1500 µs: STOPS rotating
 *   - 2000 µs: Rotates continuously clockwise
 * 
 * Commands via USB Serial (115200 baud):
 *   p<value>  - Set pulse width in microseconds (e.g., "p1500" for 1500µs)
 *   a<value>  - Set angle in degrees -90 to +90 (e.g., "a45" for 45°)
 *   c         - Center servo (1500µs)
 *   s         - Stop/center servo (same as 'c')
 *   +         - Increase pulse by 50µs
 *   -         - Decrease pulse by 50µs
 *   i         - Print current servo info
 *   t         - Run sweep test (500µs to 2500µs)
 *   h         - Print help
 * 
 * Wiring:
 *   - Servo Signal (usually orange/yellow) -> GPIO pin (default: GPIO 15)
 *   - Servo VCC (usually red) -> 5V power supply (NOT from Pico!)
 *   - Servo GND (usually brown/black) -> GND (common with Pico GND)
 */

#include "pico/stdlib.h"
#include "pico/time.h"
#include "drivers/servo_test/servo_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============ CONFIGURATION ============
// Change this to the GPIO pin your servo is connected to
#define SERVO_GPIO_PIN 15

// If testing multiple servos, define their pins here:
// #define SERVO_2_GPIO_PIN 14
// #define SERVO_3_GPIO_PIN 16
// =======================================

// Buffer for serial input
#define INPUT_BUFFER_SIZE 32
static char input_buffer[INPUT_BUFFER_SIZE];
static int buffer_index = 0;

// Current servo being controlled
static uint8_t current_servo = 0;

// Helper function to get timestamp in milliseconds
static inline uint32_t get_timestamp_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

void print_help(void) {
    printf("\n");
    printf("========================================\n");
    printf("       SERVO MOTOR TEST PROGRAM        \n");
    printf("========================================\n");
    printf("\n");
    printf("Commands:\n");
    printf("  p<value>  Set pulse width in µs (e.g., p1500)\n");
    printf("  a<value>  Set angle in degrees (e.g., a45 or a-45)\n");
    printf("  c or s    Center/stop servo (1500µs)\n");
    printf("  +         Increase pulse by 50µs\n");
    printf("  -         Decrease pulse by 50µs\n");
    printf("  i         Print servo info\n");
    printf("  t         Run sweep test\n");
    printf("  d         Disable PWM (for manual servo adjustment)\n");
    printf("  e         Enable PWM\n");
    printf("  h         Print this help\n");
    printf("\n");
    printf("--- CONTINUOUS SERVO MODE ---\n");
    printf("  m<speed>  Enable continuous mode (e.g., m90 for 90 dps)\n");
    printf("  g<angle>  Move to angle in continuous mode (e.g., g180)\n");
    printf("  r         Reset continuous angle to 0\n");
    printf("  q         Query current continuous angle\n");
    printf("\n");
    printf("----------------------------------------\n");
    printf("  HOW TO IDENTIFY SERVO TYPE:\n");
    printf("----------------------------------------\n");
    printf("  1. Send 'p1500' (center pulse)\n");
    printf("  2. Send 'p1000' (minimum pulse)\n");
    printf("  3. Observe servo behavior:\n");
    printf("\n");
    printf("  POSITIONAL (180°) servo:\n");
    printf("    -> Moves to a position and STOPS\n");
    printf("\n");
    printf("  CONTINUOUS (360°) servo:\n");
    printf("    -> Keeps ROTATING until you send 'c'\n");
    printf("\n");
    printf("----------------------------------------\n");
    printf("  CONTINUOUS MODE USAGE:\n");
    printf("----------------------------------------\n");
    printf("  1. Measure your servo's speed:\n");
    printf("     - Send 'p2500' and time one full rotation\n");
    printf("     - Speed (dps) = 360 / time_in_seconds\n");
    printf("  2. Enable mode: 'm90' (for 90 dps speed)\n");
    printf("  3. Move to angles: 'g90', 'g180', 'g-45', etc.\n");
    printf("  4. Reset position: 'r' (sets current as 0°)\n");
    printf("========================================\n\n");
}

void run_sweep_test(void) {
    printf("\n[%lu ms] === Starting Sweep Test ===\n", get_timestamp_ms());
    printf("[%lu ms] Sweeping from 500µs to 2500µs...\n", get_timestamp_ms());
    printf("[%lu ms] Watch the servo behavior:\n", get_timestamp_ms());
    printf("[%lu ms]   - Positional: Should move to each position and hold\n", get_timestamp_ms());
    printf("[%lu ms]   - Continuous: Should vary rotation speed/direction\n\n", get_timestamp_ms());
    
    // Sweep from min to max
    printf("[%lu ms] Sweeping MIN -> MAX...\n", get_timestamp_ms());
    for (uint32_t pulse = 500; pulse <= 2500; pulse += 100) {
        printf("[%lu ms]   Pulse: %lu µs\n", get_timestamp_ms(), pulse);
        servo_test_set_pulse_us(current_servo, pulse);
        sleep_ms(500);
    }
    
    // Return to center
    printf("\n[%lu ms] Returning to center (1500µs)...\n", get_timestamp_ms());
    servo_test_center(current_servo);
    sleep_ms(1000);
    
    // Sweep from max to min
    printf("[%lu ms] Sweeping MAX -> MIN...\n", get_timestamp_ms());
    for (int32_t pulse = 2500; pulse >= 500; pulse -= 100) {
        printf("[%lu ms]   Pulse: %d µs\n", get_timestamp_ms(), pulse);
        servo_test_set_pulse_us(current_servo, (uint32_t)pulse);
        sleep_ms(500);
    }
    
    // Return to center
    printf("\n[%lu ms] Test complete! Returning to center (1500µs)...\n", get_timestamp_ms());
    servo_test_center(current_servo);
    printf("[%lu ms] === Sweep Test Done ===\n\n", get_timestamp_ms());
}

void process_command(char *cmd) {
    // Trim leading/trailing whitespace
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    size_t len = strlen(cmd);
    while (len > 0 && (cmd[len-1] == ' ' || cmd[len-1] == '\t' || 
                       cmd[len-1] == '\r' || cmd[len-1] == '\n')) {
        cmd[--len] = '\0';
    }
    
    if (len == 0) return;
    
    char first_char = cmd[0];
    
    switch (first_char) {
        case 'p':
        case 'P': {
            // Set pulse width: p1500
            int pulse = atoi(&cmd[1]);
            if (pulse >= 500 && pulse <= 2500) {
                servo_test_set_pulse_us(current_servo, (uint32_t)pulse);
            } else {
                printf("[%lu ms] Error: Pulse must be between 500 and 2500 µs\n", get_timestamp_ms());
            }
            break;
        }
        
        case 'a':
        case 'A': {
            // Set angle: a45 or a-45
            float angle = (float)atof(&cmd[1]);
            servo_test_set_angle(current_servo, angle);
            break;
        }
        
        case 'c':
        case 'C':
        case 's':
        case 'S':
            // Center/stop
            printf("[%lu ms] Centering servo (1500µs)...\n", get_timestamp_ms());
            servo_test_center(current_servo);
            break;
        
        case '+': {
            // Increase pulse by 50µs
            uint32_t current = servo_test_get_pulse_us(current_servo);
            servo_test_set_pulse_us(current_servo, current + 50);
            break;
        }
        
        case '-': {
            // Decrease pulse by 50µs
            uint32_t current = servo_test_get_pulse_us(current_servo);
            if (current >= 50) {
                servo_test_set_pulse_us(current_servo, current - 50);
            }
            break;
        }
        
        case 'i':
        case 'I':
            // Print info
            servo_test_print_info(current_servo);
            break;
        
        case 't':
        case 'T':
            // Run sweep test
            run_sweep_test();
            break;
        
        case 'd':
        case 'D':
            // Disable PWM
            servo_test_disable(current_servo);
            break;
        
        case 'e':
        case 'E':
            // Enable PWM
            servo_test_enable(current_servo);
            break;
        
        case 'm':
        case 'M': {
            // Enable continuous mode: m90
            float speed = (float)atof(&cmd[1]);
            if (speed > 0 && speed <= 500.0f) {
                servo_test_set_continuous_mode(current_servo, speed);
            } else {
                printf("[%lu ms] Error: Speed must be between 0 and 500 dps\n", get_timestamp_ms());
            }
            break;
        }
        
        case 'g':
        case 'G': {
            // Move to angle in continuous mode: g180
            float angle = (float)atof(&cmd[1]);
            servo_test_move_continuous_angle(current_servo, angle);
            break;
        }
        
        case 'r':
        case 'R':
            // Reset continuous angle
            servo_test_reset_continuous_angle(current_servo);
            break;
        
        case 'q':
        case 'Q': {
            // Query current continuous angle
            float angle = servo_test_get_continuous_angle(current_servo);
            printf("[%lu ms] Servo %d: Current angle = %.1f°\n", get_timestamp_ms(), current_servo, angle);
            break;
        }
        
        case 'h':
        case 'H':
        case '?':
            // Help
            print_help();
            break;
        
        default:
            printf("[%lu ms] Unknown command: '%s'. Type 'h' for help.\n", get_timestamp_ms(), cmd);
            break;
    }
}

int main() {
    // Initialize stdio (USB serial)
    stdio_init_all();
    
    // Wait for USB connection
    sleep_ms(2000);
    
    printf("\n\n");
    printf("[%lu ms] ========================================\n", get_timestamp_ms());
    printf("[%lu ms]    Pico Servo Motor Test Program       \n", get_timestamp_ms());
    printf("[%lu ms] ========================================\n", get_timestamp_ms());
    printf("\n");
    
    // Initialize servo on specified GPIO
    printf("[%lu ms] Initializing servo on GPIO %d...\n\n", get_timestamp_ms(), SERVO_GPIO_PIN);
    servo_test_init(0, SERVO_GPIO_PIN);
    
    // Print help on startup
    print_help();
    
    printf("[%lu ms] Ready! Enter commands:\n", get_timestamp_ms());
    printf("> ");
    
    // Main loop - read serial commands
    while (true) {
        int c = getchar_timeout_us(10000);  // 10ms timeout
        
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == '\r' || c == '\n') {
                // End of command
                if (buffer_index > 0) {
                    input_buffer[buffer_index] = '\0';
                    printf("\n");
                    process_command(input_buffer);
                    buffer_index = 0;
                    printf("> ");
                }
            } else if (c == '\b' || c == 127) {
                // Backspace
                if (buffer_index > 0) {
                    buffer_index--;
                    printf("\b \b");
                }
            } else if (buffer_index < INPUT_BUFFER_SIZE - 1) {
                // Add character to buffer
                input_buffer[buffer_index++] = (char)c;
                putchar(c);  // Echo
            }
        }
    }
    
    return 0;
}
