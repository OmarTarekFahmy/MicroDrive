# Quick Start: Building and Using the OV7670 USB Camera

## Step-by-Step Build Instructions

### 1. Navigate to Build Directory
```powershell
cd c:\Users\Omar\Desktop\Uni Projects\MicroDrive\src\build
```

### 2. Configure CMake (First Time Only)
```powershell
cmake ..
```

### 3. Build the Camera Firmware
```powershell
cmake --build . --target camera_test
```

### 4. Flash to Pico
1. Hold BOOTSEL button on Pico
2. Connect Pico to computer via USB
3. Copy `camera_test.uf2` to RPI-RP2 drive
4. Pico will reboot automatically

## Hardware Connections

**Critical Connections:**
```
Pico 3.3V  → OV7670 VCC  (⚠️ NOT 5V!)
Pico GND   → OV7670 GND

Pico GP4   → OV7670 SDA  (with 10kΩ pullup)
Pico GP21  → OV7670 SCL  (with 10kΩ pullup)

Pico GP3   → OV7670 XCLK
Pico GP17  → OV7670 RST
Pico GP16  → OV7670 VSYNC
Pico GP15  → OV7670 HREF
Pico GP14  → OV7670 PCLK

Pico GP6   → OV7670 D0
Pico GP7   → OV7670 D1
Pico GP8   → OV7670 D2
Pico GP9   → OV7670 D3
Pico GP10  → OV7670 D4
Pico GP11  → OV7670 D5
Pico GP12  → OV7670 D6
Pico GP13  → OV7670 D7
```

See [CAMERA_WIRING.md](CAMERA_WIRING.md) for detailed wiring diagram.

## Using the Camera

### Windows
1. Open Camera app or VLC
2. Select "TinyUSB UVC" device

### Linux
```bash
vlc v4l2:///dev/video0
```

### macOS
1. Open Photo Booth or QuickTime
2. Select "TinyUSB UVC" camera

## Viewing Debug Output

Connect USB-to-Serial adapter:
- TX → Pico GP0
- RX → Pico GP1
- GND → Pico GND

Use terminal (115200 baud, 8N1):
```powershell
# Windows (PowerShell with appropriate COM port)
# Use PuTTY or similar terminal program

# Linux/macOS
screen /dev/ttyUSB0 115200
```

## Specifications

- **Resolution**: 320×240 (QVGA)
- **Format**: YUV422
- **Frame Rate**: ~5 FPS
- **Interface**: USB 2.0 (UVC)

## Troubleshooting

| Issue | Fix |
|-------|-----|
| Not detected | Check USB cable, try different port |
| No I2C | Check pullups on SDA/SCL |
| Corrupted image | Verify D0-D7 are on consecutive GP6-GP13 |
| No image | Check XCLK with scope, verify 3.3V power |

## Full Documentation

- [OV7670_CAMERA_GUIDE.md](OV7670_CAMERA_GUIDE.md) - Complete implementation guide
- [CAMERA_WIRING.md](CAMERA_WIRING.md) - Detailed wiring instructions

## Project Files

### Driver Files (src/drivers/camera/)
- `ov7670.c` - Camera driver
- `ov7670.h` - Driver header
- `ov7670_init.h` - Register configurations
- `image.pio` - PIO capture program

### Application Files (src/)
- `main_camera.c` - Main application
- `usb_descriptors.c` - USB descriptors
- `usb_descriptors.h` - Descriptor definitions
- `tusb_config.h` - TinyUSB config

### Build Files
- `CMakeLists.txt` - Build configuration
