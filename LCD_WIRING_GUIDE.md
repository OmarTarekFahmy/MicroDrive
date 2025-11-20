# LCD JHD162A Wiring Guide for Raspberry Pi Pico

## LCD Pin Connections

The JHD162A is a 16x2 character LCD with HD44780 controller. It has 16 pins.

### LCD Pinout (looking at the back of LCD):
```
Pin  Name   Description
---------------------------
1    VSS    Ground (0V)
2    VDD    Power (+5V)
3    V0     Contrast (connect to potentiometer)
4    RS     Register Select (GPIO 0)
5    RW     Read/Write (connect to GND for write-only)
6    E      Enable (GPIO 1)
7    D0     Data bit 0 (not used in 4-bit mode)
8    D1     Data bit 1 (not used in 4-bit mode)
9    D2     Data bit 2 (not used in 4-bit mode)
10   D3     Data bit 3 (not used in 4-bit mode)
11   D4     Data bit 4 (GPIO 2)
12   D5     Data bit 5 (GPIO 3)
13   D6     Data bit 6 (GPIO 4)
14   D7     Data bit 7 (GPIO 5)
15   A      Backlight Anode (+5V through 220Ω resistor)
16   K      Backlight Cathode (GND)
```

## Breadboard Wiring Connections

### From Raspberry Pi Pico to LCD:

**Power Connections:**
- Pico **VBUS (Pin 40)** → LCD **Pin 2 (VDD)** [+5V]
- Pico **GND (Pin 38 or any GND)** → LCD **Pin 1 (VSS)** [Ground]
- Pico **GND** → LCD **Pin 5 (RW)** [Write-only mode]

**Data Connections:**
- Pico **GPIO 0 (Pin 1)** → LCD **Pin 4 (RS)**
- Pico **GPIO 1 (Pin 2)** → LCD **Pin 6 (E)**
- Pico **GPIO 2 (Pin 4)** → LCD **Pin 11 (D4)**
- Pico **GPIO 3 (Pin 5)** → LCD **Pin 12 (D5)**
- Pico **GPIO 4 (Pin 6)** → LCD **Pin 13 (D6)**
- Pico **GPIO 5 (Pin 7)** → LCD **Pin 14 (D7)**

**Contrast Control (WITHOUT potentiometer):**

**Option 1 (Best contrast - Recommended):**
- LCD **Pin 3 (V0)** → 1kΩ resistor → GND

**Option 2 (Alternative if no 1kΩ resistor):**
- LCD **Pin 3 (V0)** → 2.2kΩ resistor → GND
- Or any resistor between 470Ω - 5kΩ

**Option 3 (Quick test - may not be optimal):**
- LCD **Pin 3 (V0)** → directly to GND (full contrast)

**If you have a potentiometer (for reference):**
- Potentiometer **Pin 1** → +5V
- Potentiometer **Pin 2 (wiper)** → LCD **Pin 3 (V0)**
- Potentiometer **Pin 3** → GND

**Backlight (optional):**
- +5V → 220Ω resistor → LCD **Pin 15 (A)**
- LCD **Pin 16 (K)** → GND

## Simplified Connection Table

| Pico Pin | Pico GPIO | → | LCD Pin | LCD Name |
|----------|-----------|---|---------|----------|
| Pin 1    | GPIO 0    | → | Pin 4   | RS       |
| Pin 2    | GPIO 1    | → | Pin 6   | E        |
| Pin 4    | GPIO 2    | → | Pin 11  | D4       |
| Pin 5    | GPIO 3    | → | Pin 12  | D5       |
| Pin 6    | GPIO 4    | → | Pin 13  | D6       |
| Pin 7    | GPIO 5    | → | Pin 14  | D7       |
| Pin 38   | GND       | → | Pin 1   | VSS      |
| Pin 38   | GND       | → | Pin 5   | RW       |
| Pin 38   | GND       | → | Pin 16  | K        |
| Pin 40   | VBUS      | → | Pin 2   | VDD      |

**Contrast (WITHOUT Potentiometer):**
- LCD Pin 3 (V0) → 1kΩ resistor → GND
- OR directly connect Pin 3 to GND if no resistor available

**Backlight resistor (220Ω):**
- +5V → 220Ω resistor → LCD Pin 15 (A)

## Notes:

1. **4-bit mode**: We're using 4-bit mode (D4-D7 only), which requires fewer GPIO pins
2. **RW pin**: Connected to GND for write-only operation (saves one GPIO pin)
3. **Contrast (NO potentiometer)**: 
   - Use a 1kΩ resistor from V0 to GND for good contrast
   - Or connect V0 directly to GND (may be very dark but readable)
   - If too dark/light, try different resistor values (470Ω-5kΩ)
4. **Backlight**: The 220Ω resistor limits current to the backlight LED (~20mA)
5. **Power**: The LCD runs on 5V, but the data pins are 3.3V tolerant

## What the Program Does:

1. Displays "Hello, World!" on line 1
2. Displays "Pico + LCD!" on line 2
3. After 3 seconds, clears the screen
4. Shows "Counter:" on line 1
5. Counts up on line 2 (Value: 0, 1, 2, ...)
6. Updates every second

## Testing:

1. Connect all wires as shown above
2. Flash the `microdrive.uf2` file to your Pico
3. Adjust the contrast potentiometer until text is visible
4. You should see the text appear immediately!

## Troubleshooting:

- **No text visible but backlight works**: 
  - Try connecting V0 (Pin 3) directly to GND
  - Or use a smaller resistor (470Ω - 1kΩ)
- **Text too dark (black boxes)**: 
  - V0 voltage is too low
  - Use a larger resistor (2kΩ - 5kΩ) or add voltage divider
- **Garbage characters**: Check D4-D7 connections
- **No backlight**: Check backlight connections (pins 15 & 16)
- **Text not updating**: Check RS and E pin connections
