#ifndef LCD_JHD162A_H
#define LCD_JHD162A_H

#include <stdint.h>

// Initialize the LCD
// Pins: RS, E, D4, D5, D6, D7 (4-bit mode)
void lcd_init(uint8_t rs, uint8_t e, uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);

// Clear the display
void lcd_clear(void);

// Set cursor position (row: 0-1, col: 0-15)
void lcd_set_cursor(uint8_t row, uint8_t col);

// Print a string at current cursor position
void lcd_print(const char *str);

// Print a character at current cursor position
void lcd_putc(char c);

// Print a number at current cursor position
void lcd_print_number(int num);

// Turn display on/off
void lcd_display_on(void);
void lcd_display_off(void);

// Turn cursor on/off
void lcd_cursor_on(void);
void lcd_cursor_off(void);

// Turn cursor blink on/off
void lcd_blink_on(void);
void lcd_blink_off(void);

#endif
