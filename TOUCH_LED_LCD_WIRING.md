# Touch Sensor + LED + LCD Wiring Guide

## Complete System Wiring

### Components:
1. Raspberry Pi Pico
2. Capacitive Touch Sensor
3. LED (with 220Ω-330Ω resistor)
4. I2C LCD Module (with 16x2 LCD)
5. Breadboard
6. Jumper wires

## Wiring Connections

### LED Wiring (GPIO 14):
```
Important: LED MUST have a resistor!

Pico GPIO 14 (Pin 19) → 220Ω Resistor → LED Long Leg (+, Anode)
LED Short Leg (-, Cathode) → GND
```

**LED Polarity:**
- **Long leg** = Positive (+) = Anode → Connect to resistor → GPIO 14
- **Short leg** = Negative (-) = Cathode → Connect to GND

**Resistor Values:**
- 220Ω (Red-Red-Brown) - Recommended
- 330Ω (Orange-Orange-Brown) - Also good
- Without resistor: LED will burn out!

### Capacitive Touch Sensor (GPIO 6):
```
Touch Sensor VCC → Pico 3.3V (Pin 36)
Touch Sensor GND → GND
Touch Sensor OUT → Pico GPIO 6 (Pin 9)
```

### I2C LCD Module (GPIO 4 & 5):
```
LCD Module VCC → Pico VBUS/5V (Pin 40)
LCD Module GND → GND
LCD Module SDA → Pico GPIO 4 (Pin 6)
LCD Module SCL → Pico GPIO 5 (Pin 7)
```

## Pin Summary Table

| Component       | Pico Pin | Pico GPIO | → | Connection        |
|-----------------|----------|-----------|---|-------------------|
| LED (+)         | Pin 19   | GPIO 14   | → | 220Ω → LED Anode  |
| LED (-)         | -        | -         | → | GND               |
| Touch VCC       | Pin 36   | 3.3V      | → | Touch VCC         |
| Touch GND       | Pin 38   | GND       | → | Touch GND         |
| Touch OUT       | Pin 9    | GPIO 6    | → | Touch OUT         |
| LCD SDA         | Pin 6    | GPIO 4    | → | LCD SDA           |
| LCD SCL         | Pin 7    | GPIO 5    | → | LCD SCL           |
| LCD VCC         | Pin 40   | VBUS      | → | LCD VCC (5V)      |
| LCD GND         | Pin 38   | GND       | → | LCD GND           |

## Common Ground
All GND connections share the same ground rail:
- Pico GND
- LED Cathode (-)
- Touch Sensor GND
- LCD Module GND

## What the Program Does:

1. **On Startup:**
   - LED blinks 3 times to verify it works
   - LCD displays initialization messages
   - Scans for I2C devices

2. **Main Operation:**
   - When touch sensor is NOT touched:
     - LED is OFF
     - LCD shows "Touch: NO" and "LED: OFF"
   
   - When touch sensor IS touched:
     - LED turns ON
     - LCD shows "Touch: YES" and "LED: ON"
     - Counts number of touches

## Troubleshooting:

### LED Not Working:

1. **Check LED polarity:**
   - Long leg to resistor → GPIO 14
   - Short leg directly to GND

2. **Check resistor:**
   - MUST have 220Ω-330Ω resistor in series
   - Without resistor: LED won't light or will burn out

3. **Test with multimeter:**
   - Set to continuity/diode mode
   - Touch probes to LED legs (red to long leg, black to short leg)
   - LED should glow faintly

4. **Verify GPIO 14:**
   - Check you're connected to Pin 19 (GPIO 14)
   - Not Pin 14 (which is GPIO 10)

### LED Circuit Diagram:
```
    GPIO 14 (Pin 19)
         |
         |
        [R] 220Ω Resistor
         |
         |
        [+] LED Anode (Long leg)
         |
        [-] LED Cathode (Short leg)
         |
        GND
```

### Touch Sensor Not Working:

1. **Check power:** VCC to 3.3V (NOT 5V for most sensors)
2. **Check OUT pin:** Connected to GPIO 6
3. **Try touching:** Output should go HIGH when touched

### LCD Not Showing Text:

1. **Check backlight:** Should be lit
2. **Adjust contrast:** Turn small blue potentiometer on I2C module
3. **Check I2C address:** Try 0x27 or 0x3F

## Testing Sequence:

1. **Flash the code**
2. **Power on Pico**
3. **Watch for LED blinks** (3 times on startup)
   - If LED doesn't blink: Check LED wiring and polarity
4. **Check LCD display**
   - Should show initialization messages
5. **Touch the sensor**
   - LED should turn ON
   - LCD should show "Touch: YES"
6. **Release the sensor**
   - LED should turn OFF
   - LCD should show "Touch: NO"

## LED Resistor Color Codes:

**220Ω:** Red-Red-Brown-Gold
```
Red (2) - Red (2) - Brown (×10) - Gold (±5%)
```

**330Ω:** Orange-Orange-Brown-Gold
```
Orange (3) - Orange (3) - Brown (×10) - Gold (±5%)
```

**Without resistor:** LED will draw too much current and may damage GPIO or burn out!

## Physical Layout Suggestion:

```
[Pico]
  ├─ Pin 6 (GPIO4) ──→ LCD SDA
  ├─ Pin 7 (GPIO5) ──→ LCD SCL
  ├─ Pin 9 (GPIO6) ──→ Touch OUT
  ├─ Pin 19 (GPIO14) ──→ 220Ω ──→ LED(+)
  ├─ Pin 36 (3.3V) ──→ Touch VCC
  ├─ Pin 38 (GND) ──→ Common GND
  └─ Pin 40 (5V) ──→ LCD VCC

Common GND Rail:
  ├─ Pico GND
  ├─ LED(-)
  ├─ Touch GND
  └─ LCD GND
```

Flash the new code - the LED should blink 3 times on startup to confirm it's working!
