# Hardware Wiring Guide - Raspberry Pi Pico (RP2040)

This document describes the hardware connections for all components in the MicroDrive project.

## Component List

1. **MPU6050 Gyroscope/Accelerometer** (I2C)
2. **SG90 Servo Motor** (PWM)
3. **LCD Display with I2C Backpack** (I2C)
4. **Capacitive Touch Keys** (GPIO)

---

## I2C Bus Configuration

The Pico has two I2C buses (I2C0 and I2C1). We'll use **I2C0** for both the gyroscope and LCD to simplify wiring.

### I2C0 Recommended Pins:
- **SDA**: GPIO 4 (Physical Pin 6)
- **SCL**: GPIO 5 (Physical Pin 7)

> **Note**: Both devices share the same I2C bus but have different addresses:
> - MPU6050: 0x68
> - LCD I2C Backpack: 0x27 or 0x3F

---

## 1. MPU6050 Gyroscope Wiring

The MPU6050 provides 3-axis gyroscope and accelerometer data.

| MPU6050 Pin | Pico Pin      | Physical Pin | Description      |
|-------------|---------------|--------------|------------------|
| VCC         | 3.3V          | Pin 36       | Power supply     |
| GND         | GND           | Pin 38       | Ground           |
| SDA         | GPIO 4 (SDA)  | Pin 6        | I2C Data         |
| SCL         | GPIO 5 (SCL)  | Pin 7        | I2C Clock        |
| XDA         | Not connected | -            | Auxiliary I2C    |
| XCL         | Not connected | -            | Auxiliary I2C    |
| AD0         | GND           | -            | Address select (0x68) |
| INT         | Not connected | -            | Interrupt (optional) |

### Code Example:
```c
#include "gyroscope.h"

gyroscope_config_t gyro_config = {
    .i2c_port = i2c0,
    .sda_pin = 4,
    .scl_pin = 5,
    .baudrate = 400000  // 400kHz
};

gyroscope_angles_t angles;
gyroscope_init(&gyro_config);
gyroscope_calibrate(100);  // Calibrate with 100 samples

// In main loop:
gyroscope_get_angles(&angles);
printf("Pitch: %.2f, Roll: %.2f, Yaw: %.2f\n", 
       angles.pitch, angles.roll, angles.yaw);
```

---

## 2. SG90 Servo Motor Wiring

The SG90 is controlled via PWM signal. Each servo needs its own GPIO pin.

### Single Servo Connection:

| Servo Wire  | Pico Pin     | Physical Pin | Description      |
|-------------|--------------|--------------|------------------|
| Red (VCC)   | VBUS (5V)    | Pin 40       | Power (5V)       |
| Brown (GND) | GND          | Pin 38       | Ground           |
| Orange (Signal) | GPIO 15  | Pin 20       | PWM Signal       |

### Multiple Servos (up to 4 servos):

| Servo # | Signal Pin | Physical Pin | Notes                  |
|---------|------------|--------------|------------------------|
| Servo 1 | GPIO 15    | Pin 20       | PWM Channel 7B         |
| Servo 2 | GPIO 14    | Pin 19       | PWM Channel 7A         |
| Servo 3 | GPIO 13    | Pin 17       | PWM Channel 6B         |
| Servo 4 | GPIO 12    | Pin 16       | PWM Channel 6A         |

> **Important**: All servos share the same power (VBUS 5V) and ground. Connect all red wires together to VBUS and all brown wires to GND.

### Code Example:
```c
#include "servo.h"

servo_t servo1, servo2;

servo_init(&servo1, 15);  // Initialize on GPIO 15
servo_init(&servo2, 14);  // Initialize on GPIO 14

servo_set_angle(&servo1, 90);   // Center position
servo_set_angle(&servo2, 45);   // 45 degrees
servo_sweep(&servo1, 180, 1, 20);  // Smooth sweep to 180°
```

---

## 3. LCD Display (16x2 with I2C Backpack)

The LCD shares the I2C bus with the gyroscope.

| LCD I2C Pin | Pico Pin      | Physical Pin | Description      |
|-------------|---------------|--------------|------------------|
| VCC         | 5V (VBUS)     | Pin 40       | Power supply     |
| GND         | GND           | Pin 38       | Ground           |
| SDA         | GPIO 4 (SDA)  | Pin 6        | I2C Data         |
| SCL         | GPIO 5 (SCL)  | Pin 7        | I2C Clock        |

> **Note**: The LCD I2C backpack typically uses address 0x27 or 0x3F. If unsure, use an I2C scanner to detect the address.

### Code Example:
```c
#include "lcd_i2c.h"

lcd_t lcd;
lcd_config_t lcd_config = {
    .i2c_port = i2c0,
    .sda_pin = 4,
    .scl_pin = 5,
    .i2c_addr = 0x27,     // Try 0x3F if 0x27 doesn't work
    .baudrate = 400000,
    .cols = 16,
    .rows = 2
};

lcd_init(&lcd_config, &lcd);
lcd_clear(&lcd);
lcd_set_text(&lcd, 0, 0, "Hello Pico!");
lcd_set_text(&lcd, 0, 1, "MicroDrive v1.0");
lcd_backlight_on(&lcd);
```

---

## 4. Capacitive Touch Keys

Touch sensors (TTP223 or similar) output digital HIGH/LOW signals.

### Touch Key Connections (4 keys example):

| Touch Key # | Signal Pin | Physical Pin | Description          |
|-------------|------------|--------------|----------------------|
| Key 1       | GPIO 16    | Pin 21       | Touch input 1        |
| Key 2       | GPIO 17    | Pin 22       | Touch input 2        |
| Key 3       | GPIO 18    | Pin 24       | Touch input 3        |
| Key 4       | GPIO 19    | Pin 25       | Touch input 4        |

Each touch sensor needs:
- **VCC** → 3.3V (Pin 36)
- **GND** → GND (Pin 38)
- **OUT** → GPIO pin (as shown above)

### Code Example:
```c
#include "touch_keys.h"

touch_keys_t touch;
touch_keys_init(&touch);

// Add 4 touch keys
touch_key_config_t key_config = {
    .active_high = true,
    .debounce_ms = 50,
    .callback = NULL
};

for (int i = 0; i < 4; i++) {
    key_config.gpio_pin = 16 + i;  // GPIO 16, 17, 18, 19
    touch_keys_add(&touch, &key_config);
}

// In main loop:
touch_keys_update(&touch);
if (touch_key_just_pressed(&touch, 0)) {
    printf("Key 1 pressed!\n");
}
```

---

## Complete Wiring Summary

### Power Rails:
- **3.3V** (Pin 36): MPU6050 VCC, Touch sensors VCC
- **5V VBUS** (Pin 40): LCD VCC, Servos VCC (Red wire)
- **GND** (Pins 3, 8, 13, 18, 23, 28, 33, 38): All GND connections

### GPIO Pin Assignments:

| GPIO Pin | Function          | Component        | Physical Pin |
|----------|-------------------|------------------|--------------|
| 4        | I2C0 SDA          | MPU6050 + LCD    | 6            |
| 5        | I2C0 SCL          | MPU6050 + LCD    | 7            |
| 12       | PWM (Servo 4)     | Servo motor      | 16           |
| 13       | PWM (Servo 3)     | Servo motor      | 17           |
| 14       | PWM (Servo 2)     | Servo motor      | 19           |
| 15       | PWM (Servo 1)     | Servo motor      | 20           |
| 16       | Digital Input     | Touch Key 1      | 21           |
| 17       | Digital Input     | Touch Key 2      | 22           |
| 18       | Digital Input     | Touch Key 3      | 24           |
| 19       | Digital Input     | Touch Key 4      | 25           |

---

## Full System Example

```c
#include "pico/stdlib.h"
#include "gyroscope.h"
#include "servo.h"
#include "lcd_i2c.h"
#include "touch_keys.h"
#include <stdio.h>

int main() {
    stdio_init_all();
    
    // Initialize Gyroscope
    gyroscope_config_t gyro_config = {
        .i2c_port = i2c0, .sda_pin = 4, .scl_pin = 5, .baudrate = 400000
    };
    gyroscope_init(&gyro_config);
    
    // Initialize LCD
    lcd_t lcd;
    lcd_config_t lcd_config = {
        .i2c_port = i2c0, .sda_pin = 4, .scl_pin = 5,
        .i2c_addr = 0x27, .baudrate = 400000, .cols = 16, .rows = 2
    };
    lcd_init(&lcd_config, &lcd);
    lcd_set_text(&lcd, 0, 0, "MicroDrive");
    
    // Initialize Servo
    servo_t servo;
    servo_init(&servo, 15);
    servo_set_angle(&servo, 90);
    
    // Initialize Touch Keys
    touch_keys_t touch;
    touch_keys_init(&touch);
    touch_key_config_t key_cfg = {
        .gpio_pin = 16, .active_high = true, .debounce_ms = 50
    };
    touch_keys_add(&touch, &key_cfg);
    
    gyroscope_angles_t angles;
    char buffer[17];
    
    while (true) {
        // Read gyroscope
        gyroscope_get_angles(&angles);
        
        // Update LCD with angle
        snprintf(buffer, sizeof(buffer), "Pitch: %.1f", angles.pitch);
        lcd_set_text(&lcd, 0, 1, buffer);
        
        // Update touch keys
        touch_keys_update(&touch);
        if (touch_key_just_pressed(&touch, 0)) {
            servo_set_angle(&servo, 180);  // Move servo on touch
        }
        
        sleep_ms(100);
    }
}
```

---

## Troubleshooting

### I2C Device Not Found:
1. Check wiring connections (SDA, SCL, VCC, GND)
2. Verify pull-up resistors are enabled (driver does this automatically)
3. Try scanning for I2C devices:
   ```c
   for (uint8_t addr = 0; addr < 128; addr++) {
       uint8_t data;
       if (i2c_read_blocking(i2c0, addr, &data, 1, false) >= 0) {
           printf("Found device at 0x%02X\n", addr);
       }
   }
   ```

### Servo Jittering:
- Ensure stable 5V power supply
- Add capacitor (100-470µF) between servo power and ground
- Check PWM signal quality

### Touch Keys Not Responding:
- Verify `active_high` setting matches your sensor module
- Adjust debounce time if needed
- Check pull-up/pull-down configuration

### LCD Not Displaying:
- Try alternate I2C address (0x3F instead of 0x27)
- Adjust contrast potentiometer on I2C backpack
- Verify backlight is enabled

---

## Additional Notes

- The RP2040 has 30 GPIO pins available
- All digital pins are 3.3V logic level
- I2C supports up to 1MHz (Fast-mode Plus)
- PWM frequency for servos is 50Hz (20ms period)
- Maximum current draw from 3.3V pin: ~300mA
- Use external power for multiple servos to avoid brownout
