#include "buzzer.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

static uint8_t buzzer_pin;
static uint buzzer_slice;
static uint buzzer_channel;
static bool is_initialized = false;

// Tones for each touch sensor
static const uint16_t touch_tones[4] = {
    TONE_1,  // Touch 1: C4 (262 Hz)
    TONE_2,  // Touch 2: E4 (330 Hz)
    TONE_3,  // Touch 3: G4 (392 Hz)
    TONE_4   // Touch 4: C5 (523 Hz)
};

void buzzer_init(uint8_t gpio_pin) {
    buzzer_pin = gpio_pin;
    
    // Set up PWM for tone generation
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    
    buzzer_slice = pwm_gpio_to_slice_num(gpio_pin);
    buzzer_channel = pwm_gpio_to_channel(gpio_pin);
    
    // Start with buzzer off
    pwm_set_enabled(buzzer_slice, false);
    
    is_initialized = true;
}

void buzzer_on(void) {
    if (!is_initialized) return;
    
    // For active buzzer, just set high
    // Use 50% duty cycle at a fixed frequency
    uint32_t sys_clk = clock_get_hz(clk_sys);
    uint32_t wrap = 1000;  // Simple wrap value
    float clkdiv = (float)sys_clk / (1000.0f * wrap);  // ~1kHz base
    
    pwm_set_clkdiv(buzzer_slice, clkdiv);
    pwm_set_wrap(buzzer_slice, wrap);
    pwm_set_chan_level(buzzer_slice, buzzer_channel, wrap / 2);  // 50% duty
    pwm_set_enabled(buzzer_slice, true);
}

void buzzer_off(void) {
    if (!is_initialized) return;
    pwm_set_chan_level(buzzer_slice, buzzer_channel, 0);
    pwm_set_enabled(buzzer_slice, false);
    // Also force the pin LOW
    gpio_set_function(buzzer_pin, GPIO_FUNC_SIO);
    gpio_set_dir(buzzer_pin, GPIO_OUT);
    gpio_put(buzzer_pin, 0);
}

void buzzer_tone(uint16_t freq_hz) {
    if (!is_initialized || freq_hz == 0) {
        buzzer_off();
        return;
    }
    
    // Restore PWM function
    gpio_set_function(buzzer_pin, GPIO_FUNC_PWM);
    
    // Configure PWM for the desired frequency
    uint32_t sys_clk = clock_get_hz(clk_sys);  // 125 MHz
    
    // Calculate wrap and divider for desired frequency
    // freq = sys_clk / (clkdiv * (wrap + 1))
    // Use wrap = 1000 for good resolution, calculate clkdiv
    uint32_t wrap = 1000;
    float clkdiv = (float)sys_clk / ((float)freq_hz * (wrap + 1));
    
    // Clamp clkdiv to valid range (1.0 to 255.0)
    if (clkdiv < 1.0f) clkdiv = 1.0f;
    if (clkdiv > 255.0f) {
        // Need larger wrap value for low frequencies
        clkdiv = 255.0f;
        wrap = sys_clk / (freq_hz * 255) - 1;
    }
    
    pwm_set_clkdiv(buzzer_slice, clkdiv);
    pwm_set_wrap(buzzer_slice, wrap);
    pwm_set_chan_level(buzzer_slice, buzzer_channel, wrap / 2);  // 50% duty cycle
    pwm_set_enabled(buzzer_slice, true);
}

void buzzer_beep(uint16_t freq_hz, uint16_t duration_ms) {
    buzzer_tone(freq_hz);
    sleep_ms(duration_ms);
    buzzer_off();
}

void buzzer_play_touch_tone(uint8_t touch_id) {
    if (touch_id >= 4) {
        buzzer_off();
        return;
    }
    buzzer_tone(touch_tones[touch_id]);
}

void buzzer_stop(void) {
    buzzer_off();
}
