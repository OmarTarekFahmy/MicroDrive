#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include <stdbool.h>

// Tone frequencies (Hz) for different touch sensors
#define TONE_1  262  // C4
#define TONE_2  330  // E4
#define TONE_3  392  // G4
#define TONE_4  523  // C5

/**
 * @brief Initialize the buzzer on the given GPIO pin
 * @param gpio_pin GPIO pin connected to the active buzzer
 */
void buzzer_init(uint8_t gpio_pin);

/**
 * @brief Turn buzzer on (for active buzzer - just HIGH signal)
 */
void buzzer_on(void);

/**
 * @brief Turn buzzer off
 */
void buzzer_off(void);

/**
 * @brief Play a tone at specified frequency
 *        For active buzzer, simulates frequency by toggling
 * @param freq_hz Frequency in Hz
 */
void buzzer_tone(uint16_t freq_hz);

/**
 * @brief Play a beep for specified duration
 * @param freq_hz Frequency in Hz
 * @param duration_ms Duration in milliseconds
 */
void buzzer_beep(uint16_t freq_hz, uint16_t duration_ms);

/**
 * @brief Play tone for a touch sensor ID
 * @param touch_id Touch sensor ID (0-3)
 */
void buzzer_play_touch_tone(uint8_t touch_id);

/**
 * @brief Stop any playing tone
 */
void buzzer_stop(void);

#endif
