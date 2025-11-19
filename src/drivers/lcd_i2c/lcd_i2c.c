/**
 * @file lcd_i2c.c
 * @brief LCD Display driver implementation with I2C interface
 */

#include "lcd_i2c.h"
#include <string.h>

// Row offsets for different LCD sizes
static const uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};

/**
 * @brief Write 4 bits to LCD via I2C
 */
static void lcd_write_nibble(lcd_t *lcd, uint8_t nibble) {
    uint8_t data = nibble | lcd->backlight;
    i2c_write_blocking(lcd->i2c_port, lcd->i2c_addr, &data, 1, false);
}

/**
 * @brief Pulse the enable bit
 */
static void lcd_pulse_enable(lcd_t *lcd, uint8_t data) {
    lcd_write_nibble(lcd, data | LCD_EN);
    sleep_us(1);
    lcd_write_nibble(lcd, data & ~LCD_EN);
    sleep_us(50);
}

/**
 * @brief Write 4 bits with enable pulse
 */
static void lcd_write_4bits(lcd_t *lcd, uint8_t value) {
    lcd_write_nibble(lcd, value);
    lcd_pulse_enable(lcd, value);
}

/**
 * @brief Send a byte to LCD in 4-bit mode
 */
static void lcd_send(lcd_t *lcd, uint8_t value, uint8_t mode) {
    uint8_t high_nibble = value & 0xF0;
    uint8_t low_nibble = (value << 4) & 0xF0;
    
    lcd_write_4bits(lcd, high_nibble | mode);
    lcd_write_4bits(lcd, low_nibble | mode);
}

/**
 * @brief Send command to LCD
 */
static void lcd_command(lcd_t *lcd, uint8_t cmd) {
    lcd_send(lcd, cmd, 0);
    if (cmd == LCD_CLEAR_DISPLAY || cmd == LCD_RETURN_HOME) {
        sleep_ms(2);
    }
}

/**
 * @brief Send data to LCD
 */
static void lcd_write_char(lcd_t *lcd, uint8_t data) {
    lcd_send(lcd, data, LCD_RS);
}

bool lcd_init(lcd_config_t *config, lcd_t *lcd) {
    if (lcd == NULL || config == NULL) return false;
    
    // Store configuration
    lcd->i2c_port = config->i2c_port;
    lcd->i2c_addr = config->i2c_addr;
    lcd->cols = config->cols;
    lcd->rows = config->rows;
    lcd->backlight = LCD_BACKLIGHT;
    lcd->display_control = LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF;
    
    // Initialize I2C pins
    gpio_set_function(config->sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(config->scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(config->sda_pin);
    gpio_pull_up(config->scl_pin);
    
    // Initialize I2C
    i2c_init(lcd->i2c_port, config->baudrate);
    
    // Wait for LCD to power up
    sleep_ms(50);
    
    // Initialize LCD in 4-bit mode
    // Send 0x03 three times (special initialization sequence)
    lcd_write_4bits(lcd, 0x30);
    sleep_ms(5);
    lcd_write_4bits(lcd, 0x30);
    sleep_us(150);
    lcd_write_4bits(lcd, 0x30);
    sleep_us(150);
    
    // Switch to 4-bit mode
    lcd_write_4bits(lcd, 0x20);
    sleep_us(150);
    
    // Function set: 4-bit mode, 2 lines, 5x8 dots
    lcd_command(lcd, LCD_FUNCTION_SET | LCD_4BIT_MODE | LCD_2LINE | LCD_5x8_DOTS);
    
    // Display control: display on, cursor off, blink off
    lcd_command(lcd, LCD_DISPLAY_CONTROL | lcd->display_control);
    
    // Clear display
    lcd_clear(lcd);
    
    // Entry mode: left to right, no shift
    lcd_command(lcd, LCD_ENTRY_MODE_SET | LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_DEC);
    
    // Return home
    lcd_home(lcd);
    
    return true;
}

void lcd_clear(lcd_t *lcd) {
    if (lcd == NULL) return;
    lcd_command(lcd, LCD_CLEAR_DISPLAY);
    sleep_ms(2);
}

void lcd_home(lcd_t *lcd) {
    if (lcd == NULL) return;
    lcd_command(lcd, LCD_RETURN_HOME);
    sleep_ms(2);
}

void lcd_set_cursor(lcd_t *lcd, uint8_t col, uint8_t row) {
    if (lcd == NULL) return;
    if (row >= lcd->rows) row = lcd->rows - 1;
    if (col >= lcd->cols) col = lcd->cols - 1;
    
    uint8_t address = col + row_offsets[row];
    lcd_command(lcd, LCD_SET_DDRAM_ADDR | address);
}

void lcd_print(lcd_t *lcd, const char *text) {
    if (lcd == NULL || text == NULL) return;
    
    while (*text) {
        lcd_write_char(lcd, *text++);
    }
}

void lcd_set_text(lcd_t *lcd, uint8_t col, uint8_t row, const char *text) {
    if (lcd == NULL || text == NULL) return;
    
    lcd_set_cursor(lcd, col, row);
    lcd_print(lcd, text);
}

void lcd_backlight_on(lcd_t *lcd) {
    if (lcd == NULL) return;
    lcd->backlight = LCD_BACKLIGHT;
    lcd_write_nibble(lcd, 0);
}

void lcd_backlight_off(lcd_t *lcd) {
    if (lcd == NULL) return;
    lcd->backlight = LCD_NO_BACKLIGHT;
    lcd_write_nibble(lcd, 0);
}

void lcd_display_on(lcd_t *lcd) {
    if (lcd == NULL) return;
    lcd->display_control |= LCD_DISPLAY_ON;
    lcd_command(lcd, LCD_DISPLAY_CONTROL | lcd->display_control);
}

void lcd_display_off(lcd_t *lcd) {
    if (lcd == NULL) return;
    lcd->display_control &= ~LCD_DISPLAY_ON;
    lcd_command(lcd, LCD_DISPLAY_CONTROL | lcd->display_control);
}

void lcd_cursor_on(lcd_t *lcd) {
    if (lcd == NULL) return;
    lcd->display_control |= LCD_CURSOR_ON;
    lcd_command(lcd, LCD_DISPLAY_CONTROL | lcd->display_control);
}

void lcd_cursor_off(lcd_t *lcd) {
    if (lcd == NULL) return;
    lcd->display_control &= ~LCD_CURSOR_ON;
    lcd_command(lcd, LCD_DISPLAY_CONTROL | lcd->display_control);
}

void lcd_blink_on(lcd_t *lcd) {
    if (lcd == NULL) return;
    lcd->display_control |= LCD_BLINK_ON;
    lcd_command(lcd, LCD_DISPLAY_CONTROL | lcd->display_control);
}

void lcd_blink_off(lcd_t *lcd) {
    if (lcd == NULL) return;
    lcd->display_control &= ~LCD_BLINK_ON;
    lcd_command(lcd, LCD_DISPLAY_CONTROL | lcd->display_control);
}

void lcd_create_char(lcd_t *lcd, uint8_t location, uint8_t charmap[8]) {
    if (lcd == NULL || charmap == NULL) return;
    
    location &= 0x7;  // Only 8 locations (0-7)
    lcd_command(lcd, LCD_SET_CGRAM_ADDR | (location << 3));
    
    for (uint8_t i = 0; i < 8; i++) {
        lcd_write_char(lcd, charmap[i]);
    }
}
