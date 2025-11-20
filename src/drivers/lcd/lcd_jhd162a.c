#include "lcd_jhd162a.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

// LCD pins
static uint8_t pin_rs;
static uint8_t pin_e;
static uint8_t pin_d4;
static uint8_t pin_d5;
static uint8_t pin_d6;
static uint8_t pin_d7;

// LCD commands
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

static uint8_t display_control = 0;

// Send pulse to enable pin
static void lcd_pulse_enable(void) {
    gpio_put(pin_e, 1);
    sleep_us(1);
    gpio_put(pin_e, 0);
    sleep_us(50);
}

// Write 4 bits to data pins
static void lcd_write_4bits(uint8_t value) {
    gpio_put(pin_d4, (value >> 0) & 0x01);
    gpio_put(pin_d5, (value >> 1) & 0x01);
    gpio_put(pin_d6, (value >> 2) & 0x01);
    gpio_put(pin_d7, (value >> 3) & 0x01);
    lcd_pulse_enable();
}

// Write a byte in 4-bit mode
static void lcd_write_byte(uint8_t value, uint8_t mode) {
    gpio_put(pin_rs, mode); // 0 = command, 1 = data
    
    // Send high nibble
    lcd_write_4bits(value >> 4);
    
    // Send low nibble
    lcd_write_4bits(value & 0x0F);
}

// Send command to LCD
static void lcd_command(uint8_t cmd) {
    lcd_write_byte(cmd, 0);
}

// Send data to LCD
static void lcd_data(uint8_t data) {
    lcd_write_byte(data, 1);
}

void lcd_init(uint8_t rs, uint8_t e, uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7) {
    // Store pin numbers
    pin_rs = rs;
    pin_e = e;
    pin_d4 = d4;
    pin_d5 = d5;
    pin_d6 = d6;
    pin_d7 = d7;
    
    // Initialize GPIO pins
    gpio_init(pin_rs);
    gpio_init(pin_e);
    gpio_init(pin_d4);
    gpio_init(pin_d5);
    gpio_init(pin_d6);
    gpio_init(pin_d7);
    
    // Set all pins as outputs
    gpio_set_dir(pin_rs, GPIO_OUT);
    gpio_set_dir(pin_e, GPIO_OUT);
    gpio_set_dir(pin_d4, GPIO_OUT);
    gpio_set_dir(pin_d5, GPIO_OUT);
    gpio_set_dir(pin_d6, GPIO_OUT);
    gpio_set_dir(pin_d7, GPIO_OUT);
    
    // Wait for LCD to power up
    sleep_ms(50);
    
    // Initialize LCD in 4-bit mode
    gpio_put(pin_rs, 0);
    gpio_put(pin_e, 0);
    
    // Initialization sequence
    lcd_write_4bits(0x03);
    sleep_ms(5);
    
    lcd_write_4bits(0x03);
    sleep_us(150);
    
    lcd_write_4bits(0x03);
    sleep_us(150);
    
    // Set to 4-bit mode
    lcd_write_4bits(0x02);
    sleep_us(150);
    
    // Function set: 4-bit mode, 2 lines, 5x8 font
    lcd_command(LCD_FUNCTION_SET | LCD_4BIT_MODE | LCD_2_LINE | LCD_5x8_DOTS);
    
    // Display control: display on, cursor off, blink off
    display_control = LCD_DISPLAY_CONTROL | LCD_DISPLAY_ON;
    lcd_command(display_control);
    
    // Clear display
    lcd_clear();
    
    // Entry mode: left to right, no shift
    lcd_command(LCD_ENTRY_MODE | LCD_ENTRY_LEFT);
    
    sleep_ms(2);
}

void lcd_clear(void) {
    lcd_command(LCD_CLEAR);
    sleep_ms(2);
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
    // Row 0: 0x00-0x0F
    // Row 1: 0x40-0x4F
    uint8_t row_offsets[] = {0x00, 0x40};
    
    if (row > 1) row = 1;
    if (col > 15) col = 15;
    
    lcd_command(LCD_SET_DDRAM | (col + row_offsets[row]));
}

void lcd_print(const char *str) {
    while (*str) {
        lcd_data(*str++);
    }
}

void lcd_putc(char c) {
    lcd_data(c);
}

void lcd_print_number(int num) {
    char buffer[12];
    sprintf(buffer, "%d", num);
    lcd_print(buffer);
}

void lcd_display_on(void) {
    display_control |= LCD_DISPLAY_ON;
    lcd_command(display_control);
}

void lcd_display_off(void) {
    display_control &= ~LCD_DISPLAY_ON;
    lcd_command(display_control);
}

void lcd_cursor_on(void) {
    display_control |= LCD_CURSOR_ON;
    lcd_command(display_control);
}

void lcd_cursor_off(void) {
    display_control &= ~LCD_CURSOR_ON;
    lcd_command(display_control);
}

void lcd_blink_on(void) {
    display_control |= LCD_BLINK_ON;
    lcd_command(display_control);
}

void lcd_blink_off(void) {
    display_control &= ~LCD_BLINK_ON;
    lcd_command(display_control);
}
