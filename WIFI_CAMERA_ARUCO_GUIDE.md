# WiFi Camera ArUco Pose Verification System

## Overview

This system enables computer vision-based unlock verification using ArUco markers. The RP2040 Pico W captures frames from an OV7670 camera and streams them over WiFi to a laptop running an ArUco detection server. The server verifies the marker's 3D pose (position, rotation, scale) within tight tolerances for 2 consecutive seconds before signaling an unlock.

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                              PICO W (Embedded)                               │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────────┐                  │
│  │   OV7670    │───▶│  RP2040      │───▶│  CYW43 WiFi     │────────┐         │
│  │   Camera    │    │  (Capture)   │    │  (TCP Stream)   │        │         │
│  │   320x240   │    │              │    │                 │        │         │
│  └─────────────┘    └──────────────┘    └─────────────────┘        │         │
│                                                                     │         │
│                      ┌───────────────┐                              │         │
│                      │ Unlock GPIO   │◀─────────────────────────────┼─┐       │
│                      │ (Relay/LED)   │  (When verified)             │ │       │
│                      └───────────────┘                              │ │       │
└──────────────────────────────────────────────────────────────────────┼─┼──────┘
                                                                       │ │
                            TCP/IP over WiFi                           │ │
                                                                       │ │
┌──────────────────────────────────────────────────────────────────────┼─┼──────┐
│                              LAPTOP (Server)                         │ │       │
│                                                                      │ │       │
│  ┌───────────────────────────────────────────────────────────────┐   │ │       │
│  │                    Python ArUco Server                        │   │ │       │
│  │  ┌─────────────┐   ┌─────────────────┐   ┌─────────────────┐  │   │ │       │
│  │  │ TCP Socket  │──▶│ OpenCV ArUco    │──▶│ Pose Validation │  │◀──┘ │       │
│  │  │ Receiver    │   │ detectMarkers() │   │ (2s tolerance)  │  │     │       │
│  │  └─────────────┘   └─────────────────┘   └────────┬────────┘  │     │       │
│  │                                                    │          │     │       │
│  │                    ┌─────────────────┐            │          │     │       │
│  │                    │ Response Sender │◀───────────┘          │     │       │
│  │                    │ (pose + unlock) │────────────────────────┼─────┘       │
│  │                    └─────────────────┘                       │             │
│  └───────────────────────────────────────────────────────────────┘             │
│                                                                                │
│  ┌───────────────────┐                                                         │
│  │ OpenCV Window     │  (Optional visualization)                               │
│  │ - Marker detection│                                                         │
│  │ - Pose axes       │                                                         │
│  │ - Unlock status   │                                                         │
│  └───────────────────┘                                                         │
└────────────────────────────────────────────────────────────────────────────────┘
```

## Why OpenCV ArUco (Not Deep Learning)?

| Aspect           | OpenCV ArUco          | Deep Learning (YOLO, etc.)    |
| ---------------- | --------------------- | ----------------------------- |
| **Accuracy**     | Excellent for markers | Overkill for known patterns   |
| **Speed**        | 5-10ms per frame      | 50-200ms per frame            |
| **3D Pose**      | Built-in `solvePnP`   | Requires extra implementation |
| **Dependencies** | Just OpenCV           | TensorFlow/PyTorch/ONNX       |
| **Robustness**   | Perfect for ArUco     | Better for general objects    |
| **CPU Usage**    | Minimal               | Requires GPU ideally          |

**ArUco is the right choice because:**

1. We're detecting a **known marker pattern**, not arbitrary objects
2. We need **precise 3D pose** (position + rotation), which ArUco provides natively
3. Speed and low latency are critical for real-time verification
4. No training data or model fine-tuning required

## Communication Protocol

### Frame Transmission (Pico → Server)

```
┌────────────────────────────────────────────────────────────────┐
│                     Frame Header (20 bytes)                    │
├──────────┬──────────┬───────┬────────┬────────┬───────┬────────┤
│  Magic   │ Frame ID │ Width │ Height │ Format │ Size  │Checksum│
│  4 bytes │ 4 bytes  │  2B   │   2B   │   2B   │  2B   │ 4 bytes│
│ CAFEBABE │    N     │  320  │  240   │   0    │ 153600│  sum   │
└──────────┴──────────┴───────┴────────┴────────┴───────┴────────┘
                              │
                              ▼
┌────────────────────────────────────────────────────────────────┐
│                     Frame Data (153600 bytes)                  │
│                        YUV422 Format                           │
│              Y0 U0 Y1 V0  Y2 U1 Y3 V1  ...                     │
└────────────────────────────────────────────────────────────────┘
```

### Pose Response (Server → Pico)

```
┌────────────────────────────────────────────────────────────────┐
│                     Pose Response (28 bytes)                   │
├──────────┬───────┬────┬───────┬────────┬───────────┬───────────┤
│  Magic   │ Found │ ID │ Valid │ Unlock │  Position │  Rotation │
│ DEADBEEF │  1B   │ 1B │  1B   │   1B   │ 3×4 bytes │ 3×4 bytes │
└──────────┴───────┴────┴───────┴────────┴───────────┴───────────┘
```

## Pose Verification Logic

```
                    ┌───────────────────┐
                    │   Detect Marker   │
                    └─────────┬─────────┘
                              │
                    ┌─────────▼─────────┐
                    │  Correct ID?      │
                    └─────────┬─────────┘
                      No │         │ Yes
                         ▼         ▼
              ┌──────────────┐  ┌──────────────────────┐
              │ Reset Timer  │  │ Estimate 3D Pose     │
              └──────────────┘  │ cv2.aruco.estimate   │
                                │ PoseSingleMarkers()  │
                                └──────────┬───────────┘
                                           │
                                ┌──────────▼───────────┐
                                │ Within Tolerance?    │
                                │ Position: ±2cm X,Y   │
                                │           ±5cm Z     │
                                │ Rotation: ±10° X,Y   │
                                │           ±15° Z     │
                                └──────────┬───────────┘
                                   No │         │ Yes
                                      ▼         ▼
                           ┌──────────────┐  ┌──────────────────┐
                           │ Reset Timer  │  │ Start/Continue   │
                           │ pose_valid=0 │  │ Timer            │
                           └──────────────┘  └──────────┬───────┘
                                                        │
                                            ┌───────────▼───────────┐
                                            │  Timer >= 2 seconds?  │
                                            └───────────┬───────────┘
                                               No │         │ Yes
                                                  ▼         ▼
                                        ┌────────────┐  ┌──────────────┐
                                        │ Keep       │  │ UNLOCK!      │
                                        │ Waiting    │  │ unlock_ready │
                                        └────────────┘  └──────────────┘
```

## Hardware Requirements

### Pico W Side

- **Raspberry Pi Pico W** (with CYW43 WiFi)
- **OV7670 Camera Module** (3.3V version)
- Jumper wires
- Optional: Relay module for physical unlock

### Laptop Side

- Python 3.8+
- OpenCV with ArUco module
- WiFi connection (same network as Pico W)

## Pin Connections

| Function   | Pico W Pin | OV7670 Pin |
| ---------- | ---------- | ---------- |
| I2C SDA    | GP4        | SDA        |
| I2C SCL    | GP21       | SCL        |
| XCLK       | GP3        | XCLK       |
| RESET      | GP17       | RST        |
| VSYNC      | GP16       | VSYNC      |
| HREF       | GP15       | HREF       |
| PCLK       | GP14       | PCLK       |
| D0-D7      | GP6-GP13   | D0-D7      |
| **Unlock** | GP22       | (Relay)    |
| Power      | 3.3V       | VCC        |
| Ground     | GND        | GND        |

## Setup Instructions

### 1. Generate ArUco Marker

```bash
cd tools
pip install -r requirements.txt
python generate_aruco.py --id 42 --size 200 --output marker.png
```

Print the marker and measure its actual size in meters.

### 2. Configure Pico W Firmware

Edit `src/drivers/wifi/wifi_camera.h`:

```c
#define WIFI_SSID       "277353"
#define WIFI_PASSWORD   "2004ahmed"
#define SERVER_IP       "192.168.1.100"  // Your laptop's IP
#define SERVER_PORT     8888
```

### 3. Build Firmware

```bash
cd src/build
cmake ..
cmake --build . --target wifi_camera
```

### 4. Flash Pico W

1. Hold BOOTSEL button while connecting Pico W
2. Copy `wifi_camera.uf2` to RPI-RP2 drive

### 5. Start Server on Laptop

```bash
cd tools
python aruco_server.py --marker-id 42 --marker-size 0.05
```

### 6. Test System

1. Power on Pico W - it will connect to WiFi and the server
2. Hold the ArUco marker in front of the camera
3. Keep marker within tolerance for 2 seconds
4. Unlock signal is triggered!

## Tuning Parameters

### Server Options

```bash
python aruco_server.py \
    --port 8888 \
    --marker-id 42 \
    --marker-size 0.05 \
    --pos-tol 0.02 0.02 0.05 \
    --rot-tol 10 10 15 \
    --target-pos 0 0 0.30 \
    --target-rot 0 0 0
```

| Parameter       | Description                  | Default        |
| --------------- | ---------------------------- | -------------- |
| `--port`        | TCP port                     | 8888           |
| `--marker-id`   | Target ArUco ID              | 42             |
| `--marker-size` | Marker size (meters)         | 0.05           |
| `--pos-tol`     | Position tolerance X Y Z (m) | 0.02 0.02 0.05 |
| `--rot-tol`     | Rotation tolerance (degrees) | 10 10 15       |
| `--target-pos`  | Expected position            | 0 0 0.30       |
| `--target-rot`  | Expected rotation            | 0 0 0          |

## Troubleshooting

### "No marker detected"

- Ensure marker is printed clearly without scaling
- Improve lighting conditions
- Hold marker closer to camera
- Check marker ID matches configuration

### "Pose not valid"

- Marker is outside position tolerance
- Marker is rotated too much
- Try relaxing tolerances in server

### "Connection failed"

- Check WiFi credentials in firmware
- Verify laptop IP address
- Ensure firewall allows port 8888
- Make sure Pico W and laptop on same network

### Low FPS

- Expected: 5-10 FPS with OV7670
- Network latency affects round-trip time
- Consider UDP for lower latency (trade reliability)

## File Structure

```
MicroDrive/
├── src/
│   ├── CMakeLists.txt              # Build configuration
│   ├── main_wifi_camera.c          # WiFi camera main application
│   └── drivers/
│       ├── camera/
│       │   ├── ov7670.c/.h         # Camera driver
│       │   └── image.pio           # PIO for capture
│       └── wifi/
│           ├── wifi_camera.c       # WiFi streaming driver
│           └── wifi_camera.h       # Driver header
├── tools/
│   ├── aruco_server.py             # Python detection server
│   ├── generate_aruco.py           # Marker generator
│   └── requirements.txt            # Python dependencies
└── WIFI_CAMERA_ARUCO_GUIDE.md      # This file
```

## Security Considerations

1. **Marker ID**: Use a unique, non-obvious marker ID
2. **Network**: Use WPA2/WPA3 encrypted WiFi
3. **Physical**: Keep marker secure; anyone with the marker can unlock
4. **Tolerance**: Tight tolerances prevent accidental unlocks
5. **Time**: 2-second verification prevents quick unauthorized attempts

## Future Enhancements

- [ ] Add encryption to TCP communication
- [ ] Support multiple valid markers
- [ ] Add time-based one-time markers
- [ ] Implement challenge-response protocol
- [ ] Add camera calibration for better accuracy
