# Gyro-Servo System - Quick Start

## Build Status
✅ **Build successful!**

## Generated Files
- **Binary**: `src/build/gyro_servo_system.uf2` (44KB)
- **ELF**: `src/build/gyro_servo_system.elf`

## Pin Configuration

### MPU6050 Gyroscope (I2C0)
| Pin Function | GPIO | Physical Pin |
|--------------|------|--------------|
| SDA          | GPIO 4 | Pin 6      |
| SCL          | GPIO 5 | Pin 7      |
| VCC          | 3.3V   | Pin 36     |
| GND          | GND    | Pin 38     |

### Servo Motors (PWM)
| Servo | Type | Axis | GPIO | Physical Pin |
|-------|------|------|------|--------------|
| X Axis | 360° Continuous | Roll | GPIO 16 | Pin 21 |
| Y Axis | 180° Standard | Pitch | GPIO 17 | Pin 22 |
| Z Axis | 360° Continuous | Yaw | GPIO 18 | Pin 24 |

**Important**: Servos must be powered from external 5V supply. Connect grounds together.

## Flashing Instructions

1. Hold BOOTSEL button on Pico while connecting USB
2. Copy `gyro_servo_system.uf2` to RPI-RP2 drive
3. Pico will reboot and run the program automatically

## Debug Output

Connect to UART (GPIO 0/1) at 115200 baud:
```bash
screen /dev/ttyACM0 115200
```

Or use GPIO 0 (TX) and GPIO 1 (RX) with USB-to-UART adapter.

## Operation

1. **Startup**: System initializes and shows status messages
2. **Calibration**: Keep MPU6050 still for 2-3 seconds
3. **Ready**: Move the sensor to control servos
   - Tilt left/right → X axis rotates
   - Tilt forward/backward → Y axis moves (0-180°)
   - Rotate sensor → Z axis rotates

## Adjusting Behavior

Edit `src/main_gyro_servo.c` and modify these values:

```c
gyro_servo_config_t config = {
    .update_rate_hz = 50,      // Control loop frequency (Hz)
    .angle_scale_x = 2.0f,     // X axis sensitivity (higher = faster)
    .angle_scale_y = 1.5f,     // Y axis sensitivity
    .angle_scale_z = 2.0f,     // Z axis sensitivity
    .deadzone = 3.0f,          // Ignore small movements (degrees)
    .invert_x = false,         // Reverse X direction
    .invert_y = false,         // Reverse Y direction
    .invert_z = false          // Reverse Z direction
};
```

## Rebuilding

```bash
cd src/build
make gyro_servo_system
```

## Troubleshooting

### Servos not responding
- Check external power supply (5V)
- Verify signal wire connections
- Increase scaling factors in config

### Jittery movement
- Increase deadzone (e.g., 5.0°)
- Reduce update_rate_hz (e.g., 25 Hz)
- Re-calibrate with sensor perfectly still

### MPU6050 not detected
- Check I2C wiring (SDA/SCL)
- Verify 3.3V power
- Check I2C address (default 0x68)

## System Architecture

```
┌──────────────┐
│   MPU6050    │
│  Gyroscope   │
└──────┬───────┘
       │ I2C
       │
┌──────▼───────────────────┐
│  Gyro-Servo Controller   │
│  - Read angles           │
│  - Apply filters         │
│  - Map to servo commands │
└──────┬───────────────────┘
       │
       ├─── PWM ──► Servo X (360°)
       ├─── PWM ──► Servo Y (180°)
       └─── PWM ──► Servo Z (360°)
```

## Files Overview

- `drivers/mpu6050/` - Gyroscope driver
- `drivers/servo_motor/` - Servo control driver
- `drivers/gyro_servo_controller/` - Integration layer
- `main_gyro_servo.c` - Main application
- `GYRO_SERVO_SYSTEM_GUIDE.md` - Detailed documentation
