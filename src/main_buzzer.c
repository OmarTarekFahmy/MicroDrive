/**
 * @file main.c
 * @brief Active Buzzer with Different Tones
 * 
 * Plays different frequencies on active buzzer every 10 seconds
 * Buzzer connected to GPIO 14
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define BUZZER_PIN 14

// PWM variables
static uint slice_num;
static uint channel;

/**
 * Initialize buzzer PWM
 */
void buzzer_init(uint gpio_pin) {
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(gpio_pin);
    channel = pwm_gpio_to_channel(gpio_pin);
}

/**
 * Play tone at specific frequency
 * @param frequency: Frequency in Hz (e.g., 440 for A4 note)
 * @param duration_ms: How long to play the tone
 */
void buzzer_play_tone(uint32_t frequency, uint32_t duration_ms) {
    if (frequency == 0) {
        // Stop/silence
        pwm_set_enabled(slice_num, false);
        return;
    }
    
    // Calculate PWM settings for desired frequency
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t divider = clock_freq / (frequency * 4096);  // Use 4096 as wrap for decent resolution
    
    if (divider < 1) divider = 1;
    if (divider > 255) divider = 255;
    
    uint32_t wrap = (clock_freq / divider / frequency) - 1;
    
    pwm_set_clkdiv(slice_num, divider);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, channel, wrap / 2);  // 50% duty cycle
    pwm_set_enabled(slice_num, true);
    
    if (duration_ms > 0) {
        sleep_ms(duration_ms);
        pwm_set_enabled(slice_num, false);
    }
}

/**
 * Stop buzzer
 */
void buzzer_stop() {
    pwm_set_enabled(slice_num, false);
}

int main() {
    // Initialize USB serial for debugging
    stdio_init_all();
    sleep_ms(2000);
    
    printf("\n===========================================\n");
    printf("  Active Buzzer - Different Tones Demo\n");
    printf("  Buzzer Pin: GP%d\n", BUZZER_PIN);
    printf("===========================================\n\n");
    
    // Initialize buzzer
    buzzer_init(BUZZER_PIN);
    
    printf("Playing different tones every 10 seconds...\n\n");
    
    uint8_t sound_index = 0;
    
    while (true) {
        switch (sound_index) {
            case 0:
                printf("[Sound 1] Low beep (200 Hz) - 2 seconds\n");
                buzzer_play_tone(200, 2000);
                break;
                
            case 1:
                printf("[Sound 2] Medium beep (500 Hz) - 2 seconds\n");
                buzzer_play_tone(500, 2000);
                break;
                
            case 2:
                printf("[Sound 3] High beep (1000 Hz) - 2 seconds\n");
                buzzer_play_tone(1000, 2000);
                break;
                
            case 3:
                printf("[Sound 4] Very high beep (2000 Hz) - 2 seconds\n");
                buzzer_play_tone(2000, 2000);
                break;
                
            case 4:
                printf("[Sound 5] Police siren pattern\n");
                for (int i = 0; i < 5; i++) {
                    buzzer_play_tone(800, 200);   // Low
                    sleep_ms(50);
                    buzzer_play_tone(1200, 200);  // High
                    sleep_ms(50);
                }
                break;
                
            case 5:
                printf("[Sound 6] Alarm pattern\n");
                for (int i = 0; i < 10; i++) {
                    buzzer_play_tone(1500, 100);
                    sleep_ms(100);
                }
                break;
                
            case 6:
                printf("[Sound 7] Musical notes (C-D-E-F-G)\n");
                buzzer_play_tone(262, 400);  // C4
                sleep_ms(100);
                buzzer_play_tone(294, 400);  // D4
                sleep_ms(100);
                buzzer_play_tone(330, 400);  // E4
                sleep_ms(100);
                buzzer_play_tone(349, 400);  // F4
                sleep_ms(100);
                buzzer_play_tone(392, 400);  // G4
                break;
                
            case 7:
                printf("[Sound 8] Descending tones\n");
                for (uint32_t freq = 2000; freq >= 400; freq -= 200) {
                    buzzer_play_tone(freq, 200);
                    sleep_ms(50);
                }
                break;
                
            case 8:
                printf("[Sound 9] Ascending tones\n");
                for (uint32_t freq = 400; freq <= 2000; freq += 200) {
                    buzzer_play_tone(freq, 200);
                    sleep_ms(50);
                }
                break;
                
            case 9:
                printf("[Sound 10] R2-D2 beep pattern\n");
                buzzer_play_tone(1000, 100);
                sleep_ms(50);
                buzzer_play_tone(1500, 150);
                sleep_ms(50);
                buzzer_play_tone(800, 100);
                sleep_ms(100);
                buzzer_play_tone(1200, 200);
                break;
        }
        
        // Move to next sound
        sound_index = (sound_index + 1) % 10;
        
        // Wait 10 seconds before next sound
        printf("Waiting 10 seconds...\n\n");
        sleep_ms(10000);
    }
    
    return 0;
}
