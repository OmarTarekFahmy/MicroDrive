# Writing Drivers in Embedded C

This section explains how to **create and organize drivers** for your embedded systems project.  
Drivers allow you to abstract hardware control (e.g., sensors, motors, LEDs) into reusable, modular code.

---

## 🧩 Driver Structure

Each driver should be placed inside its own folder under `src/drivers/`.

Example:
```
src/
 ├── drivers/
 │    ├── rgb_led/
 │    │     ├── rgb_led.c
 │    │     └── rgb_led.h
 │    ├── motor_driver/
 │    │     ├── motor_driver.c
 │    │     └── motor_driver.h
 └── main/
      └── main.c
```

---

## 📁 The Header File (`.h`)

The **header file** defines:
- Function **declarations** (not definitions)
- Data **structures** and **enums**
- **Constants** or **macros**
- **Include guards** to prevent double inclusion

### Example: `rgb_led.h`
```c
#ifndef RGB_LED_H    // Start of include guard
#define RGB_LED_H

#include "pico/stdlib.h"

// Struct to represent the RGB LED pins
typedef struct {
    uint red_pin;
    uint green_pin;
    uint blue_pin;
} RGB_LED;

// Initialize the RGB LED
void rgb_led_init(RGB_LED *led, uint r, uint g, uint b);

// Set RGB LED color
void rgb_led_set_color(RGB_LED *led, uint8_t red, uint8_t green, uint8_t blue);

#endif  // End of include guard
```

### 🧠 Why `#ifndef`, `#define`, and `#endif`?
These **include guards** prevent the compiler from including the same header file more than once.  
If a file is included twice without guards, it can cause duplicate definitions and compilation errors.

---

## ⚙️ The Source File (`.c`)

The **source file** implements the functionality declared in the header.

### Example: `rgb_led.c`
```c
#include "rgb_led.h"

void rgb_led_init(RGB_LED *led, uint r, uint g, uint b) {
    led->red_pin = r;
    led->green_pin = g;
    led->blue_pin = b;

    gpio_init(r);
    gpio_init(g);
    gpio_init(b);

    gpio_set_dir(r, GPIO_OUT);
    gpio_set_dir(g, GPIO_OUT);
    gpio_set_dir(b, GPIO_OUT);
}

void rgb_led_set_color(RGB_LED *led, uint8_t red, uint8_t green, uint8_t blue) {
    gpio_put(led->red_pin, red);
    gpio_put(led->green_pin, green);
    gpio_put(led->blue_pin, blue);
}
```

---

## 🔗 Using the Driver in `main.c`

You can now include and use your driver abstractly:
```c
#include "rgb_led.h"

int main() {
    stdio_init_all();

    RGB_LED led;
    rgb_led_init(&led, 2, 3, 4);

    while (true) {
        rgb_led_set_color(&led, 1, 0, 0); // Red
        sleep_ms(500);
        rgb_led_set_color(&led, 0, 1, 0); // Green
        sleep_ms(500);
        rgb_led_set_color(&led, 0, 0, 1); // Blue
        sleep_ms(500);
    }
}
```

---

## ✅ Summary

| File | Purpose |
|------|----------|
| `.h` | Contains **function declarations**, **structs**, and **include guards** |
| `.c` | Contains **function definitions** (actual code implementation) |

By keeping this structure consistent, your project remains organized, modular, and easy to maintain.
