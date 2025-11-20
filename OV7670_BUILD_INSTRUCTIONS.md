# OV7670 Camera Module - Build and Run Instructions

This guide explains how to build and run the OV7670 camera module firmware on Raspberry Pi Pico and the desktop viewer application.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Building Pico Firmware](#building-pico-firmware)
3. [Flashing the Pico](#flashing-the-pico)
4. [Building Desktop Viewer](#building-desktop-viewer)
5. [Running the System](#running-the-system)
6. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### For Pico Firmware

- **Raspberry Pi Pico SDK** installed and configured
- **CMake** (version 3.13 or later)
- **ARM GCC toolchain** (arm-none-eabi-gcc)
- **Python 3** (for SDK tools)
- **OV7670 camera module** (without FIFO)

### For Desktop Viewer

#### Windows:

- **MinGW-w64** or **MSVC** compiler
- **CMake** (version 3.10 or later)
- **SDL2 library** (download from https://libsdl.org/)

#### Linux:

- **GCC** compiler
- **CMake** (version 3.10 or later)
- **SDL2 development libraries**

Install SDL2 on Linux:

```bash
# Ubuntu/Debian
sudo apt-get install libsdl2-dev

# Fedora
sudo dnf install SDL2-devel

# Arch Linux
sudo pacman -S sdl2
```

---

## Building Pico Firmware

### Step 1: Set Up Environment

Ensure the Pico SDK environment variable is set:

**Windows (PowerShell):**

```powershell
$env:PICO_SDK_PATH = "C:\path\to\pico-sdk"
```

**Linux/macOS:**

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
```

### Step 2: Navigate to Source Directory

```bash
cd "C:\Users\Omar\Desktop\Uni Projects\MicroDrive\src"
```

### Step 3: Create Build Directory

```bash
mkdir build
cd build
```

### Step 4: Configure with CMake

**Windows:**

```powershell
cmake .. -G "MinGW Makefiles"
```

**Linux/macOS:**

```bash
cmake ..
```

### Step 5: Build the Firmware

**Windows:**

```powershell
mingw32-make
```

**Linux/macOS:**

```bash
make
```

### Step 6: Locate the Firmware

After successful build, you'll find:

- `microdrive.uf2` - Firmware file for flashing
- `microdrive.elf` - Debugging symbols
- `microdrive.bin` - Raw binary

---

## Flashing the Pico

### Method 1: USB Mass Storage (Recommended)

1. **Disconnect** the Pico from your computer
2. Hold the **BOOTSEL** button on the Pico
3. While holding BOOTSEL, **connect** the Pico via USB
4. Release BOOTSEL - the Pico will appear as a USB drive (RPI-RP2)
5. **Copy** `microdrive.uf2` to the drive
6. The Pico will automatically reboot and run the firmware

### Method 2: Using Picotool

If you have picotool installed:

```bash
picotool load -x microdrive.uf2
```

---

## Building Desktop Viewer

### Windows Build

#### Step 1: Install SDL2

1. Download SDL2 development libraries from https://libsdl.org/
2. Extract to `C:\SDL2` (or your preferred location)

#### Step 2: Configure and Build

```powershell
cd "C:\Users\Omar\Desktop\Uni Projects\MicroDrive\viewer"
mkdir build
cd build

# Set SDL2 path
$env:SDL2DIR = "C:\SDL2"

# Configure
cmake .. -G "MinGW Makefiles"

# Build
mingw32-make
```

#### Step 3: Copy SDL2 DLL

Copy `SDL2.dll` from the SDL2 library to the same directory as `viewer.exe`:

```powershell
copy C:\SDL2\lib\x64\SDL2.dll .\viewer.exe
```

### Linux Build

#### Step 1: Install Dependencies

```bash
sudo apt-get install libsdl2-dev  # Ubuntu/Debian
```

#### Step 2: Configure and Build

```bash
cd ~/MicroDrive/viewer
mkdir build
cd build

# Configure
cmake ..

# Build
make
```

---

## Running the System

### Step 1: Connect the Hardware

1. Wire the OV7670 camera to the Pico according to [OV7670_WIRING.md](OV7670_WIRING.md)
2. Connect the Pico to your computer via USB
3. Wait for the USB serial device to appear

### Step 2: Identify the Serial Port

**Windows:**

- Open Device Manager
- Look under "Ports (COM & LPT)"
- Note the COM port (e.g., COM3, COM4)

**Linux:**

- Run: `ls /dev/ttyACM*`
- Typically `/dev/ttyACM0`

### Step 3: Run the Viewer

**Windows:**

```powershell
.\viewer.exe COM3
```

**Linux:**

```bash
./viewer /dev/ttyACM0
```

### Step 4: View the Camera Feed

- The viewer window will open
- You should see live grayscale video from the camera (160x120 resolution)
- Press **ESC** or close the window to exit

---

## Expected Output

### Pico Serial Output

```
===========================================
  OV7670 Camera Module Demo
  Resolution: 160x120 (QQVGA)
  Format: Grayscale (8-bit Y)
===========================================

Initializing OV7670 camera module...
I2C/SCCB initialized on pins SDA:4 SCL:5
XCLK initialized on pin 9 (10 MHz)
Camera Info - PID: 0x76, VER: 0x73, MID: 0x7FA2
Resetting OV7670...
Reset complete
Configuring camera registers for QQVGA grayscale...
Camera configuration complete
Initializing PIO for pixel capture...
OV7670 initialization complete!

Camera initialized successfully!
Starting frame capture loop...

Frame 1 captured
Frame captured: 19200 pixels
FRAME_START
SIZE:19200
[binary data]
FRAME_END
```

### Viewer Console Output

```
OV7670 Camera Viewer v1.0
============================

Serial port COM3 opened successfully
Viewer initialized. Waiting for frames...
Frame received: 19200 bytes
Frame received: 19200 bytes
FPS: 4.85
```

---

## Troubleshooting

### Camera Initialization Fails

**Symptom:** "Failed to communicate with camera!"

**Solutions:**

1. Check I2C wiring (SDA=GPIO4, SCL=GPIO5)
2. Verify pull-up resistors (4.7kΩ) on I2C lines
3. Ensure camera has stable 3.3V power
4. Check camera module is OV7670 (PID should be 0x76)

### No Frames Captured

**Symptom:** "Frame capture failed!" or "Frame sync lost!"

**Solutions:**

1. Check VSYNC, HREF, PCLK connections
2. Verify data lines (D0-D7) are connected correctly
3. Ensure XCLK is generating (check with oscilloscope - should be ~10 MHz)
4. Check all grounds are connected

### Corrupted Image

**Symptom:** Image appears noisy or incorrect

**Solutions:**

1. Use shorter wires for data lines (D0-D7)
2. Add decoupling capacitor (100nF) near camera module
3. Reduce clock speed in `ov7670.c` (lower XCLK frequency)
4. Check for loose connections

### Viewer Cannot Open Serial Port

**Symptom:** "Failed to open serial port"

**Windows Solutions:**

1. Check COM port number in Device Manager
2. Close any other programs using the port (e.g., Arduino IDE, PuTTY)
3. Try different COM port

**Linux Solutions:**

1. Check permissions: `sudo chmod 666 /dev/ttyACM0`
2. Add user to dialout group: `sudo usermod -a -G dialout $USER` (logout/login required)
3. Verify device exists: `ls /dev/ttyACM*`

### Low Frame Rate

**Symptom:** FPS < 3

**Solutions:**

1. This is expected for non-FIFO OV7670 with software capture
2. Typical frame rates: 3-10 FPS
3. USB bandwidth and processing overhead limit speed
4. Consider using OV7670 with FIFO for higher frame rates

### SDL2 Not Found (Windows)

**Symptom:** CMake error "Could NOT find SDL2"

**Solutions:**

1. Set SDL2DIR environment variable: `$env:SDL2DIR = "C:\SDL2"`
2. Ensure SDL2 development libraries are installed
3. Check CMakeLists.txt has correct SDL2 find path

### SDL2 Not Found (Linux)

**Symptom:** CMake error "Could NOT find SDL2"

**Solutions:**

```bash
sudo apt-get install libsdl2-dev
pkg-config --cflags --libs sdl2  # Verify installation
```

---

## Performance Notes

- **Frame Rate:** 3-10 FPS (typical for non-FIFO OV7670)
- **Resolution:** 160x120 pixels (QQVGA)
- **Format:** 8-bit grayscale (Y channel from YUV422)
- **Latency:** ~200-500ms (USB transfer and processing)

---

## Advanced Configuration

### Changing Resolution

Edit `ov7670.h` and `ov7670.c` to modify resolution. Supported modes:

- QQVGA: 160x120 (default)
- QVGA: 320x240
- VGA: 640x480 (requires more memory)

### Adjusting Image Quality

Edit register values in `ov7670.c`:

- **Brightness:** `OV7670_REG_BRIGHT` (0x00 = default)
- **Contrast:** `OV7670_REG_CONTRAS` (0x40 = default)
- **Gain:** `OV7670_REG_GAIN`

### Enabling RGB565 Mode

Modify `ov7670_qqvga_config[]` in `ov7670.c`:

```c
{OV7670_REG_COM7, COM7_FMT_QVGA | COM7_RGB},
{OV7670_REG_COM15, 0xd0},  // RGB565 output
```

Update `FRAME_SIZE` to `160 * 120 * 2` for RGB565.

---

## Additional Resources

- **OV7670 Datasheet:** [omnivision.com](https://www.ovt.com/)
- **Pico SDK Documentation:** [raspberrypi.com/documentation](https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html)
- **SDL2 Documentation:** [libsdl.org](https://wiki.libsdl.org/)

---

## License

This project follows the MicroDrive project license. See main repository for details.
