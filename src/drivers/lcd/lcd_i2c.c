#include "lcd_i2c.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

// I2C LCD commands
#define LCD_CLEAR           0x01
#define LCD_HOME            0x02
#define LCD_ENTRY_MODE      0x04
#define LCD_DISPLAY_CONTROL 0x08
#define LCD_CURSOR_SHIFT    0x10
#define LCD_FUNCTION_SET    0x20
#define LCD_SET_CGRAM       0x40
#define LCD_SET_DDRAM       0x80

// Display control flags
#define LCD_DISPLAY_ON      0x04
#define LCD_CURSOR_ON       0x02
#define LCD_BLINK_ON        0x01

// Entry mode flags
#define LCD_ENTRY_LEFT      0x02
#define LCD_ENTRY_SHIFT_DEC 0x00

// Function set flags
#define LCD_4BIT_MODE       0x00
#define LCD_2_LINE          0x08
#define LCD_5x8_DOTS        0x00

// Backlight control
#define LCD_BACKLIGHT       0x08
#define LCD_NO_BACKLIGHT    0x00

// Enable bit
#define ENABLE              0x04

// Register select bit
#define RS_DATA             0x01
#define RS_COMMAND          0x00

static i2c_inst_t *i2c;
static uint8_t i2c_addr;
static uint8_t backlight_state = LCD_BACKLIGHT;
static uint8_t display_control = 0;

// Write a byte to the I2C expander
static void i2c_write_byte(uint8_t val) {
    i2c_write_blocking(i2c, i2c_addr, &val, 1, false);
}

// Send pulse to enable pin
static void lcd_pulse_enable(uint8_t data) {
    i2c_write_byte(data | ENABLE);
    sleep_us(1);
    i2c_write_byte(data & ~ENABLE);
    sleep_us(50);
}

// Write 4 bits to LCD
static void lcd_write_4bits(uint8_t data) {
    i2c_write_byte(data | backlight_state);
    lcd_pulse_enable(data);
}

// Write a byte to LCD in 4-bit mode
static void lcd_write_byte_i2c(uint8_t data, uint8_t mode) {
    uint8_t high_nibble = (data & 0xF0) | mode | backlight_state;
    uint8_t low_nibble = ((data << 4) & 0xF0) | mode | backlight_state;
    
    lcd_write_4bits(high_nibble);
    lcd_write_4bits(low_nibble);
}

// Send command to LCD
static void lcd_command(uint8_t cmd) {
    lcd_write_byte_i2c(cmd, RS_COMMAND);
    if (cmd == LCD_CLEAR || cmd == LCD_HOME) {
        sleep_ms(2);
    }
}

// Send data to LCD
static void lcd_data(uint8_t data) {
    lcd_write_byte_i2c(data, RS_DATA);
}

void lcd_i2c_init(void* i2c_port, uint8_t sda_pin, uint8_t scl_pin, uint8_t addr) {
    // Store I2C parameters
    i2c = (i2c_inst_t*)i2c_port;
    i2c_addr = addr;
    
    // Note: I2C already initialized in main for scanning
    // Just reconfigure to be safe
    i2c_init(i2c, 100000); // 100kHz
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);
    
    // Turn on backlight first
    backlight_state = LCD_BACKLIGHT;
    i2c_write_byte(backlight_state);
    
    // Wait for LCD to power up - LONGER DELAY
    sleep_ms(100);
    
    // Initialize LCD in 4-bit mode - HD44780 initialization sequence
    // Start in 8-bit mode
    lcd_write_4bits(0x30);
    sleep_ms(5);  // Wait > 4.1ms
    
    lcd_write_4bits(0x30);
    sleep_ms(1);  // Wait > 100us
    
    lcd_write_4bits(0x30);
    sleep_ms(1);
    
    // Set to 4-bit mode
    lcd_write_4bits(0x20);
    sleep_ms(1);
    
    // Function set: 4-bit mode, 2 lines, 5x8 font
    lcd_command(LCD_FUNCTION_SET | LCD_4BIT_MODE | LCD_2_LINE | LCD_5x8_DOTS);
    sleep_ms(1);
    
    // Display control: display off initially
    lcd_command(LCD_DISPLAY_CONTROL);
    sleep_ms(1);
    
    // Clear display
    lcd_command(LCD_CLEAR);
    sleep_ms(3);  // Clear needs longer delay
    
    // Entry mode: left to right, no shift
    lcd_command(LCD_ENTRY_MODE | LCD_ENTRY_LEFT);
    sleep_ms(1);
    
    // Display control: display on, cursor off, blink off
    display_control = LCD_DISPLAY_CONTROL | LCD_DISPLAY_ON;
    lcd_command(display_control);
    sleep_ms(1);
    
    // Ensure backlight is on
    backlight_state = LCD_BACKLIGHT;
    i2c_write_byte(backlight_state);
    
    sleep_ms(10);
}

void lcd_i2c_clear(void) {
    lcd_command(LCD_CLEAR);
    sleep_ms(2);
}

void lcd_i2c_set_cursor(uint8_t row, uint8_t col) {
    uint8_t row_offsets[] = {0x00, 0x40};
    
    if (row > 1) row = 1;
    if (col > 15) col = 15;
    
    lcd_command(LCD_SET_DDRAM | (col + row_offsets[row]));
}

void lcd_i2c_print(const char *str) {
    while (*str) {
        lcd_data(*str++);
    }
}

void lcd_i2c_putc(char c) {
    lcd_data(c);
}

void lcd_i2c_print_number(int num) {
    char buffer[12];
    sprintf(buffer, "%d", num);
    lcd_i2c_print(buffer);
}

void lcd_i2c_display_on(void) {
    display_control |= LCD_DISPLAY_ON;
    lcd_command(display_control);
}

void lcd_i2c_display_off(void) {
    display_control &= ~LCD_DISPLAY_ON;
    lcd_command(display_control);
}

void lcd_i2c_backlight_on(void) {
    backlight_state = LCD_BACKLIGHT;
    i2c_write_byte(backlight_state);
}

void lcd_i2c_backlight_off(void) {
    backlight_state = LCD_NO_BACKLIGHT;
    i2c_write_byte(backlight_state);
}

void lcd_i2c_cursor_on(void) {
    display_control |= LCD_CURSOR_ON;
    lcd_command(display_control);
}

void lcd_i2c_cursor_off(void) {
    display_control &= ~LCD_CURSOR_ON;
    lcd_command(display_control);
}

void lcd_i2c_blink_on(void) {
    display_control |= LCD_BLINK_ON;
    lcd_command(display_control);
}

void lcd_i2c_blink_off(void) {
    display_control &= ~LCD_BLINK_ON;
    lcd_command(display_control);
}

// Test functions
void lcd_i2c_send_command(uint8_t cmd) {
    lcd_command(cmd);
}

void lcd_i2c_send_data(uint8_t data) {
    lcd_data(data);
}
