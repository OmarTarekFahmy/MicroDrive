#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <stdint.h>

// Initialize the LCD with I2C
// i2c_port: i2c0 or i2c1
// sda_pin: SDA GPIO pin
// scl_pin: SCL GPIO pin
// addr: I2C address (usually 0x27 or 0x3F)
void lcd_i2c_init(void* i2c_port, uint8_t sda_pin, uint8_t scl_pin, uint8_t addr);

// Clear the display
void lcd_i2c_clear(void);

// Set cursor position (row: 0-1, col: 0-15)
void lcd_i2c_set_cursor(uint8_t row, uint8_t col);

// Print a string at current cursor position
void lcd_i2c_print(const char *str);

// Print a character at current cursor position
void lcd_i2c_putc(char c);

// Print a number at current cursor position
void lcd_i2c_print_number(int num);

// Turn display on/off
void lcd_i2c_display_on(void);
void lcd_i2c_display_off(void);

// Turn backlight on/off
void lcd_i2c_backlight_on(void);
void lcd_i2c_backlight_off(void);

// Turn cursor on/off
void lcd_i2c_cursor_on(void);
void lcd_i2c_cursor_off(void);

// Turn cursor blink on/off
void lcd_i2c_blink_on(void);
void lcd_i2c_blink_off(void);

// Test function - send raw command
void lcd_i2c_send_command(uint8_t cmd);

// Test function - send raw data
void lcd_i2c_send_data(uint8_t data);

#endif
