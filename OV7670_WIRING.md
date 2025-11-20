# OV7670 Camera Module - Wiring Diagram

## Pin Connections

### OV7670 to Raspberry Pi Pico

| OV7670 Pin (your board) | Connect To Pico                    | Purpose                                                    |
| ----------------------- | ---------------------------------- | ---------------------------------------------------------- |
| **VCC**                 | 3.3V                               | Camera power                                               |
| **GND**                 | GND                                | Ground                                                     |
| **SCL**                 | GP5                                | SCCB/I2C clock                                             |
| **SDA**                 | GP4                                | SCCB/I2C data                                              |
| **MCLK**                | **GP21**                           | Master clock input (Pico generates ~8–12 MHz)              |
| **PCLK**                | GP18                               | Pixel clock (output from camera → Pico input)              |
| **VS** (VSYNC)          | **GP19**                           | Frame start (camera → Pico)                                |
| **HS** (HSYNC)          | **GP20**                           | Row start (camera → Pico)                                  |
| **D0**                  | GP6                                | Pixel bit 0                                                |
| **D1**                  | GP7                                | Pixel bit 1                                                |
| **D2**                  | GP8                                | Pixel bit 2                                                |
| **D3**                  | GP9                                | Pixel bit 3                                                |
| **D4**                  | GP10                               | Pixel bit 4                                                |
| **D5**                  | GP11                               | Pixel bit 5                                                |
| **D6**                  | GP12                               | Pixel bit 6                                                |
| **D7**                  | GP13                               | Pixel bit 7                                                |
| **RST**                 | **3.3V** (or a GPIO if controlled) | Reset pin (active **high**)                                |
| **PWNN** (PWDN)         | **GND**                            | Power-down (active **high**) → tie to GND to enable camera |

## Wiring Notes

1. **Power Supply**: The OV7670 requires a stable 3.3V supply. The Pico's 3V3(OUT) pin can provide this.

2. **Pull-up Resistors**: Add 4.7kΩ pull-up resistors on SIOD (GPIO 4) and SIOC (GPIO 5) for I2C communication.

3. **Reset Pin**: If available, connect RESET to 3.3V through a 10kΩ resistor. If not using, tie directly to 3.3V.

4. **Power Down**: If available, connect PWDN to GND to keep the camera active.

5. **Decoupling Capacitors**: Add a 100nF capacitor between 3V3 and GND close to the camera module for stable operation.

## Schematic Diagram (Text Format)

```
Raspberry Pi Pico                    OV7670 Camera Module
┌─────────────────┐                  ┌──────────────────┐
│                 │                  │                  │
│ 3V3 ────────────┼──────────────────┤ VCC              │
│ GND ────────────┼──────────────────┤ GND              │
│                 │                  │                  │
│ GP4 ────────────┼──────────────────┤ SDA (SCCB)       │
│ GP5 ────────────┼──────────────────┤ SCL (SCCB)       │
│                 │                  │                  │
│ GP19 ───────────┼──────────────────┤ VS (VSYNC)       │
│ GP20 ───────────┼──────────────────┤ HS (HSYNC)       │
│ GP18 ───────────┼──────────────────┤ PCLK             │
│ GP21 ───────────┼──────────────────┤ MCLK (XCLK)      │
│                 │                  │                  │
│ GP6 ────────────┼──────────────────┤ D0               │
│ GP7 ────────────┼──────────────────┤ D1               │
│ GP8 ────────────┼──────────────────┤ D2               │
│ GP9 ────────────┼──────────────────┤ D3               │
│ GP10 ───────────┼──────────────────┤ D4               │
│ GP11 ───────────┼──────────────────┤ D5               │
│ GP12 ───────────┼──────────────────┤ D6               │
│ GP13 ───────────┼──────────────────┤ D7               │
│ 3.3V ───────────┼──────────────────┤ RST              │
│ GND ────────────┼──────────────────┤ PWNN (PWDN)      │
│                 │                  │                  │
└─────────────────┘                  └──────────────────┘

Pull-up Resistors (4.7kΩ):
  3V3 ──[4.7kΩ]── GPIO 4 (SIOD)
  3V3 ──[4.7kΩ]── GPIO 5 (SIOC)

Decoupling Capacitor:
  3V3 ──[100nF]── GND (near camera module)
```

## Physical Connection Tips

1. **Use Short Wires**: Keep data line wires (D0-D7) as short as possible to minimize noise and signal degradation.

2. **Breadboard Layout**: If using a breadboard:

   - Place the camera module at one end
   - Place the Pico at the other end
   - Keep I2C and data lines on opposite sides to reduce crosstalk

3. **Cable Management**: Bundle and twist pairs of wires (e.g., PCLK+GND, VSYNC+GND) to reduce EMI.

4. **Test Points**: Leave VSYNC, HREF, and PCLK accessible for oscilloscope probing during debugging.

5. **Common Ground**: Ensure all grounds are properly connected for reliable operation.

## Pin Customization

If you need to use different GPIO pins, modify the pin definitions in `ov7670.h`:

```c
#define OV7670_PIN_SIOD     4   // I2C SDA (GP4)
#define OV7670_PIN_SIOC     5   // I2C SCL (GP5)
#define OV7670_PIN_VSYNC    19  // Vertical sync (GP19)
#define OV7670_PIN_HREF     20  // Horizontal reference (GP20 - HSYNC)
#define OV7670_PIN_PCLK     18  // Pixel clock (GP18)
#define OV7670_PIN_XCLK     21  // Master clock (GP21 - MCLK)
#define OV7670_PIN_D0       6   // Data bit 0 (GP6, consecutive pins D0-D7)
```

**Important**: Data pins (D0-D7) should be consecutive for efficient PIO operation.
