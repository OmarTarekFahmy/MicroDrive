# I2C LCD Module Wiring Guide for Raspberry Pi Pico

## What is an I2C LCD Module?

The I2C LCD module (also called PCF8574 I2C adapter) is a backpack that connects to your 16x2 LCD display. It reduces the number of wires from 16 pins to just 4 pins (VCC, GND, SDA, SCL)!

## I2C LCD Module Pinout

The I2C module typically has 4 pins:
```
Pin    Description
-------------------
VCC    Power (+5V)
GND    Ground (0V)
SDA    Data line
SCL    Clock line
```

## Breadboard Wiring - SUPER SIMPLE!

### From Raspberry Pi Pico to I2C LCD Module:

| Pico Pin | Pico GPIO | → | I2C Module Pin |
|----------|-----------|---|----------------|
| Pin 6    | GPIO 4    | → | SDA            |
| Pin 7    | GPIO 5    | → | SCL            |
| Pin 40   | VBUS      | → | VCC (+5V)      |
| Pin 38   | GND       | → | GND            |

That's it! Only **4 wires** needed! 🎉

## Detailed Connection

**Power:**
- Pico **VBUS (Pin 40)** → I2C Module **VCC** [+5V]
- Pico **GND (Pin 38)** → I2C Module **GND** [Ground]

**I2C Communication:**
- Pico **GPIO 4 (Pin 6)** → I2C Module **SDA** [Data]
- Pico **GPIO 5 (Pin 7)** → I2C Module **SCL** [Clock]

## I2C Address

Most I2C LCD modules use one of these addresses:
- **0x27** (most common)
- **0x3F** (alternative)

The code is set to `0x27` by default. If nothing shows up, try changing it to `0x3F` in main.c:

```c
lcd_i2c_init(i2c0, 4, 5, 0x3F);  // Change 0x27 to 0x3F
```

## Finding Your I2C Address (Optional)

If you want to scan for the I2C address, you can use this simple scanner code:

```c
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdio.h>

int main() {
    stdio_init_all();
    
    i2c_init(i2c0, 100000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);
    
    sleep_ms(2000);
    printf("I2C Scanner\n");
    
    for (uint8_t addr = 0; addr < 128; addr++) {
        uint8_t data;
        int ret = i2c_read_blocking(i2c0, addr, &data, 1, false);
        if (ret >= 0) {
            printf("Found device at address 0x%02X\n", addr);
        }
    }
    
    while(1) tight_loop_contents();
}
```

## What the Program Does:

1. Initializes the I2C LCD on GPIO 4 (SDA) and GPIO 5 (SCL)
2. Displays "Hello, World!" on row 0
3. Displays "Pico + I2C LCD!" on row 1
4. After 3 seconds, shows a counter that increments every second

## Advantages of I2C LCD:

✅ Only 4 wires needed (vs 16 for parallel)  
✅ No potentiometer needed (contrast is set on the module)  
✅ Backlight control via software  
✅ Multiple I2C devices can share the same bus  
✅ Easier to wire on breadboard  

## Notes:

1. **Power**: The module needs 5V, but I2C signals are 3.3V compatible
2. **Pull-up resistors**: Already included on most I2C modules
3. **Backlight**: Controlled by software (no resistor needed)
4. **Contrast**: Pre-set on the I2C module (some have a tiny potentiometer on the back)

## Troubleshooting:

- **No text visible**: 
  - Check if backlight is on (should see blue glow)
  - Try changing I2C address from 0x27 to 0x3F
  - Check SDA/SCL connections
  
- **Backlight on but no text**:
  - Adjust the small potentiometer on the back of the I2C module
  - Or change I2C address
  
- **Nothing works**:
  - Verify connections (especially SDA and SCL - don't swap them!)
  - Check power connections (5V and GND)
  - Use I2C scanner to find the correct address

## Contrast Adjustment (if needed):

Some I2C modules have a tiny blue potentiometer on the back. If you can't see the text clearly:
1. Find the small blue potentiometer on the I2C module
2. Use a small screwdriver to turn it
3. Adjust until text is clearly visible

## Physical Setup:

```
[Raspberry Pi Pico]
    Pin 6 (GPIO4/SDA) ─────→ SDA
    Pin 7 (GPIO5/SCL) ─────→ SCL    [I2C Module] ← [LCD Display]
    Pin 40 (VBUS/5V)  ─────→ VCC
    Pin 38 (GND)      ─────→ GND
```

Much simpler than parallel connection! 🚀
