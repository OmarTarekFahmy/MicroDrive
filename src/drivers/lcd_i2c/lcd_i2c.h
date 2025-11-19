/**
 * @file lcd_i2c.h
 * @brief LCD Display driver with I2C interface (PCF8574 I2C backpack) for RP2040
 * 
 * This driver supports standard 16x2 and 20x4 LCD displays with I2C backpack
 * using the PCF8574 I2C expander (HD44780 compatible displays).
 */

#ifndef LCD_I2C_H
#define LCD_I2C_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdbool.h>

// Default I2C address for LCD (can be 0x27 or 0x3F)
#define LCD_I2C_ADDR_DEFAULT 0x27

// LCD Commands
#define LCD_CLEAR_DISPLAY   0x01
#define LCD_RETURN_HOME     0x02
#define LCD_ENTRY_MODE_SET  0x04
#define LCD_DISPLAY_CONTROL 0x08
#define LCD_CURSOR_SHIFT    0x10
#define LCD_FUNCTION_SET    0x20
#define LCD_SET_CGRAM_ADDR  0x40
#define LCD_SET_DDRAM_ADDR  0x80

// Display entry mode flags
#define LCD_ENTRY_RIGHT          0x00
#define LCD_ENTRY_LEFT           0x02
#define LCD_ENTRY_SHIFT_INC      0x01
#define LCD_ENTRY_SHIFT_DEC      0x00

// Display control flags
#define LCD_DISPLAY_ON  0x04
#define LCD_DISPLAY_OFF 0x00
#define LCD_CURSOR_ON   0x02
#define LCD_CURSOR_OFF  0x00
#define LCD_BLINK_ON    0x01
#define LCD_BLINK_OFF   0x00

// Function set flags
#define LCD_8BIT_MODE 0x10
#define LCD_4BIT_MODE 0x00
#define LCD_2LINE     0x08
#define LCD_1LINE     0x00
#define LCD_5x10_DOTS 0x04
#define LCD_5x8_DOTS  0x00

// Backlight control
#define LCD_BACKLIGHT   0x08
#define LCD_NO_BACKLIGHT 0x00

// Enable bit
#define LCD_EN 0x04  // Enable bit
#define LCD_RW 0x02  // Read/Write bit
#define LCD_RS 0x01  // Register select bit

// LCD configuration structure
typedef struct {
    i2c_inst_t *i2c_port;  // I2C port (i2c0 or i2c1)
    uint8_t sda_pin;       // SDA pin number
    uint8_t scl_pin;       // SCL pin number
    uint8_t i2c_addr;      // I2C address of LCD
    uint32_t baudrate;     // I2C baudrate
    uint8_t cols;          // Number of columns (16 or 20)
    uint8_t rows;          // Number of rows (2 or 4)
} lcd_config_t;

// LCD instance structure
typedef struct {
    i2c_inst_t *i2c_port;
    uint8_t i2c_addr;
    uint8_t cols;
    uint8_t rows;
    uint8_t backlight;
    uint8_t display_control;
} lcd_t;

/**
 * @brief Initialize LCD display with I2C interface
 * 
 * @param lcd Pointer to LCD instance structure
 * @param config Configuration structure
 * @return true if initialization successful, false otherwise
 */
bool lcd_init(lcd_t *lcd, lcd_config_t *config);

/**
 * @brief Clear the LCD display
 * 
 * @param lcd Pointer to LCD instance
 */
void lcd_clear(lcd_t *lcd);

/**
 * @brief Set cursor position on LCD
 * 
 * @param lcd Pointer to LCD instance
 * @param col Column position (0-based)
 * @param row Row position (0-based)
 */
void lcd_set_cursor(lcd_t *lcd, uint8_t col, uint8_t row);

/**
 * @brief Display text on LCD at current cursor position
 * 
 * @param lcd Pointer to LCD instance
 * @param text Text string to display
 */
void lcd_print(lcd_t *lcd, const char *text);

/**
 * @brief Display text on LCD at specific position
 * 
 * @param lcd Pointer to LCD instance
 * @param col Column position (0-based)
 * @param row Row position (0-based)
 * @param text Text string to display
 */
void lcd_set_text(lcd_t *lcd, uint8_t col, uint8_t row, const char *text);

/**
 * @brief Turn backlight on
 * 
 * @param lcd Pointer to LCD instance
 */
void lcd_backlight_on(lcd_t *lcd);

/**
 * @brief Turn backlight off
 * 
 * @param lcd Pointer to LCD instance
 */
void lcd_backlight_off(lcd_t *lcd);

/**
 * @brief Turn display on
 * 
 * @param lcd Pointer to LCD instance
 */
void lcd_display_on(lcd_t *lcd);

/**
 * @brief Turn display off
 * 
 * @param lcd Pointer to LCD instance
 */
void lcd_display_off(lcd_t *lcd);

/**
 * @brief Show cursor
 * 
 * @param lcd Pointer to LCD instance
 */
void lcd_cursor_on(lcd_t *lcd);

/**
 * @brief Hide cursor
 * 
 * @param lcd Pointer to LCD instance
 */
void lcd_cursor_off(lcd_t *lcd);

/**
 * @brief Enable cursor blinking
 * 
 * @param lcd Pointer to LCD instance
 */
void lcd_blink_on(lcd_t *lcd);

/**
 * @brief Disable cursor blinking
 * 
 * @param lcd Pointer to LCD instance
 */
void lcd_blink_off(lcd_t *lcd);

/**
 * @brief Return cursor to home position (0, 0)
 * 
 * @param lcd Pointer to LCD instance
 */
void lcd_home(lcd_t *lcd);

/**
 * @brief Create custom character
 * 
 * @param lcd Pointer to LCD instance
 * @param location Character location (0-7)
 * @param charmap 8-byte array defining the character
 */
void lcd_create_char(lcd_t *lcd, uint8_t location, uint8_t charmap[8]);

#endif // LCD_I2C_H
