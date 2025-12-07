# Gyro-Servo Control System

## Overview
This is a complete implementation of a gyroscope-controlled servo motor system built from scratch for the Raspberry Pi Pico. The system uses the MPU6050 gyroscope/accelerometer to read orientation angles and mirrors those angles to servo motor movements.

## Hardware Configuration

### Servo Motors
- **X Axis (Roll)**: 360° continuous rotation servo - GPIO 16
- **Y Axis (Pitch)**: 180° standard servo - GPIO 17  
- **Z Axis (Yaw)**: 360° continuous rotation servo - GPIO 18

### MPU6050 Gyroscope
- **SDA**: GPIO 4
- **SCL**: GPIO 5
- **I2C Instance**: i2c0
- **I2C Address**: 0x68

## Architecture

### Driver Components

#### 1. MPU6050 Driver (`drivers/mpu6050/`)
Complete gyroscope/accelerometer driver with:
- I2C communication
- Sensor initialization and configuration
- Raw and processed data reading
- Euler angle calculation using complementary filter
- Gyroscope calibration (offset compensation)
- Configurable full-scale ranges for gyro and accelerometer

**Key Features:**
- Complementary filter (98% gyro, 2% accel) for stable angle estimation
- Automatic gyroscope drift compensation
- Temperature reading support
- Connection verification

#### 2. Servo Motor Driver (`drivers/servo_motor/`)
Universal servo motor driver supporting both:
- **Standard Servos (180°)**: Position control (0-180 degrees)
- **Continuous Rotation Servos (360°)**: Speed control (-100 to +100)

**Key Features:**
- PWM-based control at 50Hz
- Configurable pulse width ranges
- Automatic PWM slice and channel assignment
- Enable/disable functionality
- Current position/speed tracking

#### 3. Gyro-Servo Controller (`drivers/gyro_servo_controller/`)
Integration layer that connects gyroscope readings to servo movements:
- Maps gyro angles to servo positions/speeds
- Deadzone filtering to prevent jitter
- Configurable scaling factors per axis
- Axis inversion support
- Automatic update loop management

**Mapping:**
- Roll angle → X axis servo speed (continuous)
- Pitch angle → Y axis servo position (standard 0-180°)
- Yaw angle → Z axis servo speed (continuous)

## Building and Running

### Prerequisites
- Pico SDK installed and configured
- CMake 3.13 or higher
- GCC ARM cross-compiler

### Build Instructions

```bash
cd src/build
cmake ..
make gyro_servo_system
```

This will generate `gyro_servo_system.uf2` in the build directory.

### Flashing to Pico

1. Hold the BOOTSEL button while connecting the Pico to USB
2. Copy the `gyro_servo_system.uf2` file to the RPI-RP2 drive
3. The Pico will reboot automatically and run the program

## Usage

### Basic Operation

1. **Power Up**: Connect the Pico to power
2. **Calibration**: Keep the MPU6050 sensor still during the initial calibration phase (2-3 seconds)
3. **Control**: Move the MPU6050 sensor - the servos will mirror the movements:
   - Tilt left/right → X axis servo rotates
   - Tilt forward/backward → Y axis servo moves to corresponding angle
   - Rotate sensor → Z axis servo rotates

### Configuration

The system can be configured in `main_gyro_servo.c`:

```c
gyro_servo_config_t config = {
    .update_rate_hz = 50,      // Control loop frequency
    .angle_scale_x = 2.0f,     // Roll sensitivity
    .angle_scale_y = 1.5f,     // Pitch sensitivity
    .angle_scale_z = 2.0f,     // Yaw sensitivity
    .deadzone = 3.0f,          // Deadzone in degrees
    .invert_x = false,         // Invert X axis
    .invert_y = false,         // Invert Y axis
    .invert_z = false          // Invert Z axis
};
```

### Adjusting Servo Pulse Widths

If your servos don't respond correctly, you can adjust the pulse width range:

```c
// After servo_init()
servo_set_pulse_range(&servo_x, 500, 2500, 1500);  // min, max, center (microseconds)
```

### Debugging

The system outputs status information via USB serial at 115200 baud:
- Initialization messages
- Calibration status
- Real-time angle readings
- Servo positions

To view debug output:
```bash
# Linux/Mac
screen /dev/ttyACM0 115200

# Windows - use PuTTY or similar terminal program
```

## API Reference

### MPU6050 Driver

```c
// Initialize MPU6050
bool mpu6050_init(void* i2c_inst, uint8_t sda_pin, uint8_t scl_pin);

// Read angles with complementary filter
bool mpu6050_get_angles(mpu6050_angles_t* angles, float dt);

// Calibrate gyroscope (removes drift)
bool mpu6050_calibrate_gyro(uint16_t samples);

// Test connection
bool mpu6050_test_connection(void);
```

### Servo Motor Driver

```c
// Initialize servo
bool servo_init(servo_t* servo, uint8_t gpio_pin, servo_type_t type, servo_axis_t axis);

// Control standard servo (180°)
bool servo_set_angle(servo_t* servo, float angle);  // 0-180 degrees

// Control continuous servo (360°)
bool servo_set_speed(servo_t* servo, float speed);  // -100 to +100

// Stop continuous servo
bool servo_stop(servo_t* servo);
```

### Gyro-Servo Controller

```c
// Initialize controller
bool gyro_servo_init(gyro_servo_controller_t* controller, 
                     servo_t* servo_x, servo_t* servo_y, servo_t* servo_z);

// Configure parameters
bool gyro_servo_configure(gyro_servo_controller_t* controller, 
                          const gyro_servo_config_t* config);

// Start control loop
bool gyro_servo_start(gyro_servo_controller_t* controller);

// Update servos based on gyro angles (call in loop)
bool gyro_servo_update(gyro_servo_controller_t* controller, 
                       const mpu6050_angles_t* angles);

// Stop control loop
void gyro_servo_stop(gyro_servo_controller_t* controller);
```

## Wiring Guide

### MPU6050 Connections
```
MPU6050     Raspberry Pi Pico
-------     -----------------
VCC    -->  3.3V (Pin 36)
GND    -->  GND (Pin 38)
SDA    -->  GPIO 4 (Pin 6)
SCL    -->  GPIO 5 (Pin 7)
```

### Servo Connections
```
Servo       Raspberry Pi Pico
-----       -----------------
Signal -->  GPIO 16/17/18 (X/Y/Z)
VCC    -->  External 5V power supply
GND    -->  GND (shared with Pico)
```

**Important**: Servos should be powered from an external power supply, not the Pico's 3.3V or 5V pins. Connect grounds together.

## Troubleshooting

### MPU6050 Not Detected
- Check I2C wiring (SDA/SCL)
- Verify 3.3V power connection
- Ensure pull-up resistors on SDA/SCL (usually built-in)
- Try alternate I2C address (0x69) if AD0 pin is high

### Servos Not Moving
- Verify PWM signal connections
- Check servo power supply (external 5V)
- Adjust pulse width ranges using `servo_set_pulse_range()`
- Increase scaling factors in configuration

### Jittery Servo Movement
- Increase deadzone value (e.g., 5.0 degrees)
- Reduce update rate (e.g., 25 Hz)
- Re-calibrate gyroscope while keeping sensor perfectly still

### Servo Moving in Wrong Direction
- Set invert flags in configuration: `invert_x`, `invert_y`, `invert_z`
- Or swap the axis assignment in hardware

## Technical Details

### PWM Configuration
- Frequency: 50 Hz (20ms period)
- Pulse width range: 500-2500 μs (configurable)
- Center pulse: 1500 μs

### Gyroscope Settings
- Full scale range: ±250 °/s (configurable)
- Sample rate: 125 Hz
- Digital low pass filter: enabled
- Complementary filter: 98% gyro, 2% accelerometer

### Control Loop
- Default update rate: 50 Hz (20ms)
- Time delta calculation for gyro integration
- Non-blocking update mechanism

## Future Enhancements

Possible improvements:
1. Kalman filter for even better angle estimation
2. PID control for smooth servo movements
3. Limits and safety features
4. Multiple controller profiles
5. EEPROM storage of calibration data
6. Wireless control interface

## License
Custom implementation - feel free to modify and use.

## Author
Created from scratch for MicroDrive project
Date: December 6, 2025
