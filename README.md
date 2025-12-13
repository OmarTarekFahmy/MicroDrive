# MicroDrive - Celestial Lock System

An advanced embedded security system built on the **Raspberry Pi Pico W** that combines gyroscope-controlled servo stabilization, computer vision-based ArUco marker verification, and multi-factor authentication for secure access control.

---

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [System Architecture](#system-architecture)
- [Hardware Requirements](#hardware-requirements)
- [Pin Configuration](#pin-configuration)
- [Software Dependencies](#software-dependencies)
- [Setup Instructions](#setup-instructions)
- [Building the Project](#building-the-project)
- [Flashing the Firmware](#flashing-the-firmware)
- [Server Setup (ArUco Verification)](#server-setup-aruco-verification)
- [Configuration](#configuration)
- [Usage](#usage)
- [API Reference](#api-reference)
- [Troubleshooting](#troubleshooting)
- [Project Structure](#project-structure)
- [Quick Start](#quick-start)

---

## Quick Start

**Prerequisites:** Raspberry Pi Pico SDK installed, ARM toolchain, Python with OpenCV.

```bash
# 1. Clone and build
git clone https://github.com/OmarTarekFahmy/MicroDrive.git
cd MicroDrive/src
mkdir build && cd build
cmake .. && make -j$(nproc)

# 2. Configure WiFi (edit src/main.c and src/main_wifi_camera.c)
#    Set: WIFI_SSID, WIFI_PASSWORD, SERVER_IP

# 3. Flash both Pico W boards
#    - Actuator Pico: Copy servo_test.uf2 to RPI-RP2
#    - Camera Pico: Copy wifi_camera.uf2 to RPI-RP2

# 4. Start the server on your laptop
cd ../../tools
python3 aruco_verification_server.py --verbose

# 5. Power on both Pico W boards
#    - Enter touch sequence: 1-3-2-4
#    - Align ArUco marker (ID 42) with camera
#    - System unlocks when marker pose is verified
```

---

## Overview

**MicroDrive** (Celestial Lock) is a sophisticated embedded security system that implements a multi-stage unlock mechanism:

1. **Touch Sequence Authentication**: 4-button capacitive touch sensor with secret code entry
2. **Gyroscope-Controlled Platform**: MPU6050-based orientation tracking with 3-axis servo stabilization
3. **Computer Vision Verification**: WiFi camera streaming with ArUco marker pose detection
4. **Actuator Control**: DC motor key mechanism with electromagnet lock release

The system uses two Pico W boards communicating over TCP/IP:
- **Camera Pico**: Streams OV7670 camera frames to a laptop server
- **Actuator Pico**: Controls servos, touch sensors, LCD display, and lock mechanisms

---

## Features

- 🔐 **Multi-factor authentication** with touch sequence + visual verification
- 🎯 **3-axis servo stabilization** using MPU6050 gyroscope with complementary filter
- 📷 **WiFi camera streaming** (320×240 YUV422) for ArUco marker detection
- 🖥️ **I2C LCD display** showing system status and orientation data
- 🔊 **Audio feedback** via buzzer with musical touch tones
- 💡 **RGB LED status indicators** for system state visualization
- 🔒 **Electromagnet lock control** with relay driver support
- 🌐 **TCP/IP communication** between embedded devices and server

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         ACTUATOR PICO W                                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │ MPU6050  │  │  Touch   │  │  Servos  │  │ DC Motor │  │   LCD    │   │
│  │ (I2C1)   │  │ Sensors  │  │  (PWM)   │  │ H-Bridge │  │  (I2C0)  │   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘   │
│       │             │             │             │             │          │
│       └─────────────┴─────────────┴──────┬──────┴─────────────┘          │
│                                          │                               │
│                              ┌───────────┴───────────┐                   │
│                              │   RP2040 + CYW43      │                   │
│                              │   WiFi (TCP Client)   │                   │
│                              └───────────┬───────────┘                   │
└──────────────────────────────────────────┼───────────────────────────────┘
                                           │ TCP Port 9999
                                           ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                    LAPTOP (ArUco Verification Server)                    │
│  ┌────────────────────────────────────────────────────────────────────┐  │
│  │  Python Server (aruco_verification_server.py)                      │  │
│  │  • Camera Port 8888: Receives frames, detects ArUco markers        │  │
│  │  • Actuator Port 9999: Sends UNLOCK commands                       │  │
│  │  • OpenCV ArUco: Pose estimation with tolerance verification       │  │
│  └────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────┘
                                           ▲
                                           │ TCP Port 8888
┌──────────────────────────────────────────┼───────────────────────────────┐
│                         CAMERA PICO W                                    │
│  ┌──────────┐                ┌───────────┴───────────┐                   │
│  │  OV7670  │───────────────▶│   RP2040 + CYW43      │                   │
│  │  Camera  │  PIO + DMA     │   WiFi (TCP Client)   │                   │
│  └──────────┘                └───────────────────────┘                   │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Requirements

### Main Components

| Component | Quantity | Description |
|-----------|----------|-------------|
| Raspberry Pi Pico W | 2 | Main microcontroller with WiFi |
| MPU6050 | 1 | 6-axis gyroscope/accelerometer |
| OV7670 Camera | 1 | QVGA camera module |
| MG996R Servo | 4 | 2× 360° continuous, 2× 180° positional |
| I2C LCD 16×2 | 1 | Display with PCF8574 backpack |
| TTP223 Touch Sensors | 4 | Capacitive touch modules |
| Active Buzzer | 1 | Audio feedback |
| RGB LED (Common Cathode) | 1 | Status indicator |
| L298N/TB6612 | 1 | DC motor H-bridge driver |
| DC Motor | 1 | Key rotation mechanism |
| Electromagnet Lock | 1 | 12V lock mechanism |
| Relay Module | 1 | For electromagnet control |
| 5V Power Supply | 1 | External power for servos |

### Additional Materials

- Breadboard(s) and jumper wires
- 220Ω–330Ω resistors for LEDs
- ArUco marker (ID 42, 4×4 dictionary) printed

---

## Pin Configuration

### Actuator Pico W Pin Mapping

| Component | GPIO | Pin | Function |
|-----------|------|-----|----------|
| **I2C0 - LCD** | | | |
| LCD SDA | GPIO 0 | Pin 1 | I2C Data |
| LCD SCL | GPIO 1 | Pin 2 | I2C Clock |
| **I2C1 - MPU6050** | | | |
| MPU SDA | GPIO 2 | Pin 4 | I2C Data |
| MPU SCL | GPIO 3 | Pin 5 | I2C Clock |
| **DC Motor (H-Bridge)** | | | |
| IN1 | GPIO 4 | Pin 6 | Direction |
| IN2 | GPIO 5 | Pin 7 | Direction |
| PWM/EN | GPIO 6 | Pin 9 | Speed Control |
| **Touch Sensors** | | | |
| Touch 1 | GPIO 9 | Pin 12 | Input |
| Touch 2 | GPIO 10 | Pin 14 | Input |
| Touch 3 | GPIO 11 | Pin 15 | Input |
| Touch 4 | GPIO 12 | Pin 16 | Input |
| **Buzzer** | GPIO 13 | Pin 17 | PWM Output |
| **Servo Motors** | | | |
| X-Axis (360°) | GPIO 14 | Pin 19 | PWM |
| Y-Axis (360°) | GPIO 15 | Pin 20 | PWM |
| Z-Axis (180°) | GPIO 18 | Pin 24 | PWM |
| Lock Servo (360°) | GPIO 20 | Pin 26 | PWM |
| **Electromagnet** | GPIO 17 | Pin 22 | Digital Out |
| **RGB LED** | | | |
| Red | GPIO 21 | Pin 27 | PWM |
| Green | GPIO 22 | Pin 29 | PWM |
| Blue | GPIO 26 | Pin 31 | PWM |

### Camera Pico W Pin Mapping

| Component | GPIO | Pin | Function |
|-----------|------|-----|----------|
| **OV7670 Camera (SCCB/I2C0)** | | | |
| SIOD (SDA) | GPIO 4 | Pin 6 | I2C Data |
| SIOC (SCL) | GPIO 21 | Pin 27 | I2C Clock |
| **Camera Control** | | | |
| XCLK | GPIO 3 | Pin 5 | Master Clock (PIO) |
| VSYNC | GPIO 16 | Pin 21 | Vertical Sync |
| RESET | GPIO 17 | Pin 22 | Camera Reset |
| **Camera Data Bus (PIO)** | | | |
| D2 (Y2) | GPIO 6 | Pin 9 | Data bit 0 (PIO base) |
| D3 (Y3) | GPIO 7 | Pin 10 | Data bit 1 |
| D4 (Y4) | GPIO 8 | Pin 11 | Data bit 2 |
| D5 (Y5) | GPIO 9 | Pin 12 | Data bit 3 |
| D6 (Y6) | GPIO 10 | Pin 14 | Data bit 4 |
| D7 (Y7) | GPIO 11 | Pin 15 | Data bit 5 |
| D8 (Y8) | GPIO 12 | Pin 16 | Data bit 6 |
| PCLK | GPIO 13 | Pin 17 | Pixel Clock |
| HREF | GPIO 14 | Pin 19 | Horizontal Ref |

**Note:** The OV7670 camera uses PIO (Programmable I/O) and DMA for high-speed parallel data capture at 320×240 resolution in YUV422 format.

### Power Connections

| Rail | Source | Components |
|------|--------|------------|
| 3.3V | Pico Pin 36 | MPU6050, Touch sensors |
| 5V (VBUS) | Pico Pin 40 | LCD module |
| 5V External | Power Supply | Servos, DC Motor |
| 12V External | Power Supply | Electromagnet (via relay) |
| GND | Common | All components |

---

## Software Dependencies

### Embedded (Pico W)

- **Raspberry Pi Pico SDK** (v1.5.0+)
- **ARM GCC Toolchain** (arm-none-eabi-gcc)
- **CMake** (v3.13+)

### Server (Python)

```bash
# Required Python packages
pip install opencv-python numpy
```

| Package | Version | Purpose |
|---------|---------|---------|
| opencv-python | ≥4.5.0 | ArUco detection & pose estimation |
| numpy | ≥1.20.0 | Numerical operations |

---

## Setup Instructions

### 1. Install Build Tools

**Arch Linux:**
```bash
sudo pacman -S --needed base-devel cmake ninja arm-none-eabi-gcc arm-none-eabi-newlib git python-opencv python-numpy
```

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi git python3-opencv python3-numpy
```

**macOS:**
```bash
brew install cmake ninja arm-none-eabi-gcc git
pip3 install opencv-python numpy
```

### 2. Install Raspberry Pi Pico SDK

```bash
# Clone the SDK
mkdir -p ~/pico
cd ~/pico
git clone -b master https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init

# Set environment variable
# For Bash/Zsh:
echo 'export PICO_SDK_PATH=~/pico/pico-sdk' >> ~/.bashrc
source ~/.bashrc

# For Fish shell:
set -Ux PICO_SDK_PATH ~/pico/pico-sdk
```

### 3. Clone the Repository

```bash
git clone https://github.com/OmarTarekFahmy/MicroDrive.git
cd MicroDrive
```

---

## Building the Project

### Configure and Build

```bash
cd src
mkdir -p build
cd build

# Configure with CMake
cmake ..

# If SDK path is not found:
cmake .. -DPICO_SDK_PATH=$PICO_SDK_PATH

# Build all targets
make -j$(nproc)
```

### Build Targets

| Target | Output File | Description | Pico Board |
|--------|-------------|-------------|------------|
| `servo_test` | `servo_test.uf2` | Main Celestial Lock (actuator) system | Actuator Pico W |
| `servo_pulse_test` | `servo_pulse_test.uf2` | Servo testing utility | Actuator Pico W |
| `wifi_camera` | `wifi_camera.uf2` | OV7670 WiFi camera streaming | Camera Pico W |

```bash
# Build actuator firmware
make servo_test

# Build camera firmware
make wifi_camera
```

---

## Flashing the Firmware

### Flashing Procedure

1. **Enter bootloader mode**: Hold the **BOOTSEL** button while connecting USB
2. **Mount the device**: The Pico appears as a USB drive named `RPI-RP2`
3. **Copy the firmware**:

### Flash Actuator Pico W

```bash
# Linux
cp src/build/servo_test.uf2 /run/media/$USER/RPI-RP2/

# macOS
cp src/build/servo_test.uf2 /Volumes/RPI-RP2/
```

### Flash Camera Pico W

```bash
# Linux
cp src/build/wifi_camera.uf2 /run/media/$USER/RPI-RP2/

# macOS
cp src/build/wifi_camera.uf2 /Volumes/RPI-RP2/
```

**Note:** Flash each Pico W separately. The Pico will automatically reboot and run the program after copying.

---

## Server Setup (ArUco Verification)

### 1. Configure WiFi Credentials

**For Actuator Pico** - Edit `src/main.c`:

```c
#define WIFI_SSID       "your_network_name"
#define WIFI_PASSWORD   "your_password"
#define SERVER_IP       "your_laptop_ip"  // e.g., "192.168.1.100"
#define ACTUATOR_PORT   9999
```

**For Camera Pico** - Edit `src/main_wifi_camera.c`:

```c
#define WIFI_SSID       "your_network_name"
#define WIFI_PASSWORD   "your_password"
#define SERVER_IP       "your_laptop_ip"  // e.g., "192.168.1.100"
#define SERVER_PORT     8888              // Camera streaming port
```

**Important:** Both Pico W boards must use the same WiFi network and point to the same server IP address.
#define ACTUATOR_PORT   9999
```

### 2. Find Your Laptop's IP Address

```bash
# Linux
ip addr show | grep "inet "

# macOS
ifconfig | grep "inet "

# Windows
ipconfig
```

### 3. Start the ArUco Server

```bash
cd tools

# Run with default settings
python3 aruco_verification_server.py

# Run with custom options
python3 aruco_verification_server.py \
    --camera-port 8888 \
    --actuator-port 9999 \
    --marker-id 42 \
    --verbose
```

### Server Configuration

Edit the `Config` class in `aruco_verification_server.py`:

```python
class Config:
    # Network
    HOST = "0.0.0.0"
    CAMERA_PORT = 8888
    ACTUATOR_PORT = 9999

    # ArUco Detection
    ARUCO_DICT = cv2.aruco.DICT_4X4_50
    TARGET_MARKER_ID = 42

    # Position Tolerances (mm)
    TOLERANCE_POSITION_X_MM = 30.0
    TOLERANCE_POSITION_Y_MM = 30.0
    TOLERANCE_POSITION_Z_MM = 60.0

    # Rotation Tolerances (degrees)
    TOLERANCE_ROTATION_DEG = 20.0
    TOLERANCE_YAW_DEG = 25.0

    # Verification
    CONSECUTIVE_FRAMES_REQUIRED = 2
    VERIFICATION_WINDOW_SECONDS = 5.0
```

### 4. Print ArUco Marker

Generate and print ArUco marker ID 42 from the 4×4_50 dictionary:

```python
import cv2
import cv2.aruco as aruco

# Generate marker
aruco_dict = aruco.getPredefinedDictionary(aruco.DICT_4X4_50)
marker_image = aruco.generateImageMarker(aruco_dict, 42, 200)

# Save to file
cv2.imwrite("aruco_marker_42.png", marker_image)
```

---

## Configuration

### Touch Sequence

The default unlock sequence is **Touch 1 → Touch 3 → Touch 2 → Touch 4**

To change it, edit `src/main.c`:

```c
#define SEQUENCE_LENGTH 4
static const int8_t SECRET_SEQUENCE[SEQUENCE_LENGTH] = {0, 2, 1, 3}; // 0-indexed
```

### Servo Calibration

For continuous servos (360°), calibrate the speed:

```c
// In main.c - measured speed: 187.6 degrees per second
servo_test_set_continuous_mode(SERVO_X, 187.6f);
servo_test_set_continuous_mode(SERVO_Y, 187.6f);
servo_test_set_continuous_mode(SERVO_LOCK, 187.6f);
```

### MPU6050 Calibration

The system auto-calibrates on startup. Keep the sensor **completely still** during the "Calibrating..." message (approximately 3 seconds).

### Camera Configuration

The OV7670 camera configuration is set in `src/main_wifi_camera.c`:

```c
// Frame settings
#define FRAME_WIDTH 320
#define FRAME_HEIGHT 240

// Camera pin configuration
static struct ov2640_config camera_config = {
    .sccb = i2c0,
    .pin_sioc = 21,           // SCCB Clock (I2C SCL)
    .pin_siod = 4,            // SCCB Data (I2C SDA)
    .pin_resetb = 17,         // Camera reset
    .pin_xclk = 3,            // Master clock output
    .pin_vsync = 16,          // Vertical sync
    .pin_y2_pio_base = 6,     // First data pin (D2-D9 consecutive)
    .pio = pio0,              // PIO instance
    .pio_sm = 0,              // PIO state machine
    .dma_channel = 0,         // DMA channel for capture
    .image_buf = frame_buffer,
    .image_buf_size = sizeof(frame_buffer)
};
```

**Frame Rate:** The camera captures at approximately 0.5 FPS (2 second intervals) to allow reliable TCP transmission of 153KB frames over WiFi.

---

## Usage

### System Operation Flow

1. **Power On**: Both Pico W boards initialize and connect to WiFi
   - Camera Pico connects to server port 8888
   - Actuator Pico connects to server port 9999
2. **Phase 1 - Authentication**: 
   - Enter the secret touch sequence (default: 1-3-2-4)
   - Buzzer plays tones for each touch
   - Wrong sequence triggers error beep
3. **Phase 2 - Unlock**:
   - LEDs change from RED to GREEN
   - LCD displays "Celestial Lock Active"
   - Electromagnet engages
4. **Phase 3 - Gyroscope Control**:
   - Servos mirror MPU6050 orientation
   - LCD shows Roll/Pitch/Yaw values
   - Press Touch 1 to reset orientation
5. **Phase 4 - Camera Verification**:
   - Camera Pico streams frames to server
   - Server detects ArUco marker and validates pose
   - 2 consecutive valid frames trigger unlock signal
6. **Phase 5 - Key Sequence**:
   - DC motor rotates (CW 100% 1.5s, CCW 50% 2.5s)
   - Lock servo rotates 135°
   - Electromagnet releases

### Serial Monitor

Connect via USB serial at 115200 baud:

```bash
# Linux
screen /dev/ttyACM0 115200

# or
minicom -D /dev/ttyACM0 -b 115200

# macOS
screen /dev/tty.usbmodem* 115200
```

---

## API Reference

### Servo Driver (`servo_test.h`)

```c
void servo_test_init(uint8_t servo_id, uint32_t gpio_pin);
void servo_test_set_pulse_us(uint8_t servo_id, uint32_t pulse_us);
void servo_test_set_angle(uint8_t servo_id, float angle);
void servo_test_center(uint8_t servo_id);
void servo_test_set_continuous_mode(uint8_t servo_id, float speed_dps);
bool servo_test_move_continuous_angle(uint8_t servo_id, float target_angle);
```

### MPU6050 Driver (`yassinMpu.h`)

```c
bool mpu6050_init(void);
void mpu6050_calibrate(uint16_t samples);
void mpu6050_update(float dt);
float mpu6050_get_roll(void);
float mpu6050_get_pitch(void);
float mpu6050_get_yaw(void);
float mpu6050_get_temperature(void);
void mpu6050_reset_orientation(void);
```

### LCD Driver (`lcd_i2c.h`)

```c
void lcd_i2c_init(void* i2c_port, uint8_t sda_pin, uint8_t scl_pin, uint8_t addr);
void lcd_i2c_clear(void);
void lcd_i2c_set_cursor(uint8_t row, uint8_t col);
void lcd_i2c_print(const char *str);
```

### DC Motor Driver (`dc_motor.h`)

```c
void dc_motor_init(dc_motor_t *motor, uint in1, uint in2, uint pwm, uint32_t freq);
void dc_motor_set(dc_motor_t *motor, float value);  // -1.0 to 1.0
void dc_motor_brake(dc_motor_t *motor);
void dc_motor_coast(dc_motor_t *motor);
```

### OV7670 Camera Driver (`ov7670.h`)

```c
// Camera configuration structure
struct ov2640_config {
    i2c_inst_t *sccb;      // I2C instance for SCCB
    uint pin_sioc;          // SCCB Clock pin
    uint pin_siod;          // SCCB Data pin
    uint pin_resetb;        // Reset pin
    uint pin_xclk;          // Master clock pin
    uint pin_vsync;         // Vertical sync pin
    uint pin_y2_pio_base;   // First data pin (PIO)
    PIO pio;                // PIO instance
    uint pio_sm;            // PIO state machine
    uint dma_channel;       // DMA channel
    uint8_t *image_buf;     // Frame buffer pointer
    size_t image_buf_size;  // Buffer size
};

void ov2640_init(struct ov2640_config *config);
void ov2640_capture_frame(struct ov2640_config *config);
uint8_t ov2640_reg_read(struct ov2640_config *config, uint8_t reg);
void ov2640_reg_write(struct ov2640_config *config, uint8_t reg, uint8_t value);
```

### WiFi Frame Protocol

The camera streams frames using a custom TCP protocol:

```c
// Frame header (24 bytes) - sent before each frame
typedef struct __attribute__((packed)) {
    uint32_t magic;        // 0xCAFEBABE - frame start marker
    uint32_t frame_id;     // Sequential frame number
    uint16_t width;        // Image width (320)
    uint16_t height;       // Image height (240)
    uint16_t format;       // 0 = YUV422
    uint16_t reserved;     // Padding (set to 0)
    uint32_t data_size;    // Image data size (153600 bytes)
    uint32_t checksum;     // Sum of all image bytes
} frame_header_t;

// Server response (28 bytes)
typedef struct __attribute__((packed)) {
    uint32_t magic;        // 0xDEADBEEF - response marker
    uint8_t  marker_found; // 1 if ArUco marker detected
    uint8_t  marker_id;    // Detected marker ID
    uint8_t  pose_valid;   // 1 if pose within tolerance
    uint8_t  unlock_ready; // 1 if unlock conditions met
    float    pos_x, pos_y, pos_z;  // Position (mm)
    float    rot_x, rot_y, rot_z;  // Rotation (degrees)
} pose_response_t;
```

---

## Troubleshooting

### Build Errors

| Error | Solution |
|-------|----------|
| `PICO_SDK_PATH not set` | Export the environment variable |
| `arm-none-eabi-gcc not found` | Install ARM toolchain |
| `CMake version too old` | Upgrade CMake to 3.13+ |

### Hardware Issues

| Issue | Check |
|-------|-------|
| Servos not moving | External 5V power, common ground |
| MPU6050 not detected | I2C connections (SDA/SCL), address 0x68 |
| LCD blank | I2C address (0x27 or 0x3F), contrast potentiometer |
| WiFi not connecting | SSID/password, network availability |
| Camera not initializing | Check SCCB (I2C) connections, verify PID=0x76/VER=0x73 |
| No camera frames | Verify XCLK, VSYNC, PCLK connections; check DMA/PIO setup |
| Blurry/corrupted images | Adjust OV7670 lens focus, check data bus wiring |

### Runtime Issues

| Issue | Solution |
|-------|----------|
| Gyro drift | Re-calibrate, ensure sensor is still during startup |
| Touch not responding | Check GPIO connections, try pull-up resistors |
| Server not receiving | Verify IP address, firewall settings |
| ArUco not detected | Proper lighting, marker size, camera focus |
| Camera frames not arriving | Check WiFi signal strength, server port 8888 |
| TCP connection drops | Increase TCP timeout, check network stability |
| Slow frame rate | Normal ~0.5 FPS due to YUV422 data size (153KB/frame) |

### Debug Output

Enable verbose logging:
```bash
# Server
python3 aruco_verification_server.py --verbose

# Check Pico serial output
screen /dev/ttyACM0 115200
```

---

## Project Structure

```
MicroDrive/
├── README.md                      # This file
├── src/
│   ├── main.c                     # Actuator Pico - Celestial Lock application
│   ├── main_wifi_camera.c         # Camera Pico - WiFi camera streaming
│   ├── main_servo_test.c          # Servo testing utility
│   ├── CMakeLists.txt             # Build configuration
│   ├── lwipopts.h                 # lwIP network stack config
│   ├── build/                     # Build output directory
│   │   ├── servo_test.uf2         # Actuator firmware
│   │   ├── wifi_camera.uf2        # Camera firmware
│   │   └── servo_pulse_test.uf2   # Servo test firmware
│   └── drivers/                   # Hardware drivers
│       ├── buzzer/                # Active buzzer driver
│       │   ├── buzzer.c
│       │   └── buzzer.h
│       ├── camera/                # OV7670 camera driver
│       │   ├── ov7670.c           # Camera initialization & capture
│       │   ├── ov7670.h           # Camera API
│       │   ├── ov7670_init.h      # Register initialization sequences
│       │   └── image.pio          # PIO program for parallel capture
│       ├── dc_motor/              # H-bridge DC motor driver
│       │   ├── dc_motor.c
│       │   └── dc_motor.h
│       ├── lcd/                   # I2C LCD driver
│       │   ├── lcd_i2c.c
│       │   └── lcd_i2c.h
│       ├── led/                   # Simple LED driver
│       ├── MyMPUTest/             # MPU6050 gyro driver
│       │   ├── yassinMpu.c        # Sensor driver with complementary filter
│       │   └── yassinMpu.h
│       ├── rgb_led/               # RGB LED driver
│       │   ├── rgb_led.c
│       │   └── rgb_led.h
│       ├── servo_test/            # Servo driver (360° & 180°)
│       │   ├── servo_test.c
│       │   └── servo_test.h
│       ├── touch/                 # Capacitive touch sensor driver
│       │   ├── touch_sensor.c
│       │   └── touch_sensor.h
│       └── wifi/                  # WiFi communication
│           ├── wifi_camera.c      # Camera WiFi streaming
│           ├── wifi_camera.h
│           ├── wifi_comm.c        # General WiFi communication
│           └── wifi_comm.h
├── tools/
│   ├── aruco_verification_server.py  # Python ArUco server (main server)
│   ├── elf2uf2.py                    # ELF to UF2 converter utility
│   └── aruco_logs/                   # Server log directory
│       ├── images/                   # Successfully captured frames
│       └── failed/                   # Failed verification frames
├── camT/                          # Camera test project
├── *.md                           # Additional documentation
│   ├── GYRO_SERVO_QUICKSTART.md
│   ├── WIFI_CAMERA_ARUCO_GUIDE.md
│   ├── TOUCH_LED_LCD_WIRING.md
│   ├── MPU6050_ORIENTATION_LOGIC.md
│   ├── OV7670_WIRING.md
│   ├── OV7670_CAMERA_GUIDE.md
│   └── ...
└── aruco_logs/                    # ArUco verification logs
```

---

## Additional Documentation

- [GYRO_SERVO_QUICKSTART.md](./GYRO_SERVO_QUICKSTART.md) - Quick start for gyro-servo system
- [GYRO_SERVO_SYSTEM_GUIDE.md](./GYRO_SERVO_SYSTEM_GUIDE.md) - Detailed gyro-servo guide
- [WIFI_CAMERA_ARUCO_GUIDE.md](./WIFI_CAMERA_ARUCO_GUIDE.md) - WiFi camera setup
- [MPU6050_ORIENTATION_LOGIC.md](./MPU6050_ORIENTATION_LOGIC.md) - Orientation math explained
- [TOUCH_LED_LCD_WIRING.md](./TOUCH_LED_LCD_WIRING.md) - Wiring diagrams
- [SERVO_CONTINUOUS_MODE.md](./SERVO_CONTINUOUS_MODE.md) - 360° servo control
- [WRITING_DRIVERS.md](./WRITING_DRIVERS.md) - Driver development guide

---

## License

This project is developed for educational purposes.

---


## Acknowledgments

- Raspberry Pi Foundation for the Pico SDK
- OpenCV team for ArUco marker detection
- The embedded systems community
