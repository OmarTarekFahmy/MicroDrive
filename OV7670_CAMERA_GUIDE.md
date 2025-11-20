# OV7670 USB Camera Implementation Guide

## Overview

This implementation integrates the OV7670 camera module with the Raspberry Pi Pico (RP2040) to create a USB Video Class (UVC) camera. The camera streams 320x240 YUV422 video at ~5 FPS over USB.

## Project Structure

```
src/
├── main_camera.c              # Main application for USB camera
├── tusb_config.h              # TinyUSB configuration
├── usb_descriptors.c          # USB device descriptors
├── usb_descriptors.h          # USB descriptor definitions
├── CMakeLists.txt             # Updated with camera_test target
└── drivers/
    └── camera/
        ├── ov7670.c           # OV7670 driver implementation
        ├── ov7670.h           # Driver header
        ├── ov7670_init.h      # Camera initialization registers
        └── image.pio          # PIO program for image capture
```

## Hardware Requirements

- Raspberry Pi Pico (RP2040)
- OV7670 Camera Module (3.3V)
- USB cable (for both power and data)
- Jumper wires
- 10kΩ resistors (2x) for I2C pull-ups (if not on camera board)
- Breadboard (optional)

## Hardware Setup

See [CAMERA_WIRING.md](../CAMERA_WIRING.md) for detailed wiring instructions.

### Quick Pin Reference

| Function | Pico Pin | OV7670 Pin |
| -------- | -------- | ---------- |
| I2C SDA  | GP4      | SDA        |
| I2C SCL  | GP21     | SCL        |
| XCLK     | GP3      | XCLK       |
| RESET    | GP17     | RST        |
| VSYNC    | GP16     | VSYNC      |
| HREF     | GP15     | HREF       |
| PCLK     | GP14     | PCLK       |
| D0-D7    | GP6-GP13 | D0-D7      |
| Power    | 3.3V     | VCC        |
| Ground   | GND      | GND        |

## Software Setup

### Prerequisites

1. **Pico SDK** installed and `PICO_SDK_PATH` environment variable set
2. **CMake** (version 3.13 or higher)
3. **ARM GCC Compiler** (arm-none-eabi-gcc)
4. **Build tools** (Make or Ninja)

### Building the Firmware

1. Navigate to the project directory:

```bash
cd src/build
```

2. Configure the project (first time only):

```bash
cmake ..
```

3. Build the camera firmware:

```bash
cmake --build . --target camera_test
```

4. The output file will be `camera_test.uf2`

### Flashing the Firmware

1. Hold the BOOTSEL button on your Pico while connecting it to your computer
2. The Pico will appear as a USB mass storage device (RPI-RP2)
3. Copy `camera_test.uf2` to the RPI-RP2 drive
4. The Pico will automatically reboot and start running the camera firmware

## Using the Camera

### On Windows

1. After flashing, disconnect and reconnect the Pico
2. Windows should detect it as a "TinyUSB UVC" camera device
3. Open Camera app or any video software (VLC, OBS, Skype, etc.)
4. Select the TinyUSB device as your camera source

### On Linux

1. The camera should appear as `/dev/video0` (or similar)
2. Check with: `ls -l /dev/video*`
3. View stream with:
   ```bash
   vlc v4l2:///dev/video0
   # or
   cheese
   # or
   ffplay /dev/video0
   ```

### On macOS

1. The camera should be automatically detected
2. Use Photo Booth, QuickTime, or any camera app
3. Select "TinyUSB UVC" as camera source

## Camera Specifications

- **Resolution**: 320x240 pixels (QVGA)
- **Format**: YUV422 (YUY2)
- **Frame Rate**: ~5 FPS (configurable)
- **Interface**: USB 2.0 Full Speed (12 Mbps)
- **USB Device Class**: UVC (USB Video Class) 1.5

## Debugging

### UART Debug Output

The firmware outputs debug information via UART on GPIO 0 (TX) and GPIO 1 (RX) at 115200 baud.

To view debug output:

1. Connect a USB-to-Serial adapter to GP0 and GP1
2. Use a terminal program (PuTTY, minicom, screen, etc.)
3. Set baud rate to 115200, 8N1

Example debug output:

```
OV7670 USB Camera
Initializing camera...
Camera initialized successfully!
Resolution: 320x240
Frame buffer size: 153600 bytes
Device mounted
Capturing frame 0...
Frame captured (153600 bytes)
```

### Common Issues

| Problem                   | Solution                                        |
| ------------------------- | ----------------------------------------------- |
| Camera not detected by OS | Check USB connection, try different port/cable  |
| No I2C communication      | Verify I2C wiring and pull-up resistors         |
| Corrupted/no image        | Check data pin connections (D0-D7)              |
| Dark image                | Verify XCLK is running, check exposure settings |
| Low frame rate            | Normal - current implementation is ~5 FPS       |

## Driver Implementation Details

### OV7670 Driver (`ov7670.c`)

- **Initialization**: Configures camera registers for YUV422 output
- **Frame Control**: Sets resolution, windowing, and scaling
- **Image Capture**: Uses PIO state machine and DMA for efficient data transfer

### PIO Image Capture (`image.pio`)

The PIO program efficiently captures pixel data:

- Waits for HSYNC
- Captures data on PCLK rising edge
- Uses autopush to transfer data to FIFO
- DMA transfers from FIFO to frame buffer

### USB Video Class

The implementation uses TinyUSB to present the camera as a standard UVC device:

- Implements UVC 1.5 specification
- Provides standard video descriptors
- Handles streaming requests
- Compatible with standard video applications

## Customization

### Changing Resolution

To modify resolution, edit `usb_descriptors.h`:

```c
#define FRAME_WIDTH   320
#define FRAME_HEIGHT  240
```

And update the camera initialization in `main_camera.c`:

```c
OV7670_set_size(&camera_config, OV7670_SIZE_DIV2);  // QVGA
// Other options:
// OV7670_SIZE_DIV1  - 640x480 VGA
// OV7670_SIZE_DIV4  - 160x120 QQVGA
// OV7670_SIZE_DIV8  - 80x60
// OV7670_SIZE_DIV16 - 40x30
```

### Changing Frame Rate

Modify in `usb_descriptors.h`:

```c
#define FRAME_RATE    30  // Target FPS (actual will be lower)
```

### Changing Output Format

To switch to RGB565, edit `ov7670.c` initialization:

```c
ov2640_regs_write(config, OV7670_rgb);  // Instead of OV7670_yuv
```

And update USB descriptors accordingly.

## Performance Optimization

Current limitations and potential improvements:

1. **Frame Rate**: Currently ~5 FPS due to USB bandwidth and processing

   - Optimize DMA transfers
   - Use compression (MJPEG)
   - Reduce resolution

2. **Image Quality**:

   - Adjust camera registers in `ov7670_init.h`
   - Tune exposure, gain, white balance
   - Add post-processing

3. **USB Bandwidth**:
   - Implement MJPEG compression
   - Use isochronous transfers more efficiently

## References

- [Original Implementation](https://github.com/mxyxbb/rp2040_ov7670_usb_camera)
- [OV7670 Datasheet](https://web.mit.edu/6.111/www/f2016/tools/OV7670_2006.pdf)
- [OV7670 Implementation Guide](https://www.voti.nl/docs/OV7670.pdf)
- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [TinyUSB Documentation](https://docs.tinyusb.org/)
- [USB Video Class Specification](https://www.usb.org/document-library/video-class-v15-document-set)

## License

This implementation is based on:

- TinyUSB (MIT License)
- OV7670 driver from usedbytes/camera-pico-ov7670
- RP2040 USB Camera project by mxyxbb

## Troubleshooting and Support

For issues and questions:

1. Check the debug output via UART
2. Verify all connections match the wiring guide
3. Ensure using 3.3V power (not 5V!)
4. Try different USB cables/ports
5. Check that camera module is not damaged
