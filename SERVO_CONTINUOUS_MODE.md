# Servo Continuous Mode - User Guide

## Overview
The servo test program now includes a **Continuous Servo Mode** that tracks the servo's angular position and calculates precise movement times based on the servo's rotation speed.

## Features
- **Angle Tracking**: Maintains current position in degrees
- **Automatic Timing**: Calculates rotation time based on servo speed
- **Bi-directional Movement**: Automatically chooses shortest rotation path
- **Unlimited Range**: Can track angles beyond ±360°

## How It Works

### 1. Measure Your Servo's Speed
First, you need to determine how fast your continuous servo rotates:

```
1. Send command: p2500  (full speed clockwise)
2. Time one complete 360° rotation using a stopwatch
3. Calculate: Speed (dps) = 360 / time_in_seconds

Example:
- If 360° takes 4 seconds: Speed = 360/4 = 90 dps
- If 360° takes 2 seconds: Speed = 360/2 = 180 dps
```

### 2. Enable Continuous Mode
```
Command: m<speed>

Examples:
  m90   - Enable continuous mode with 90 degrees/second
  m120  - Enable continuous mode with 120 degrees/second
  m180  - Enable continuous mode with 180 degrees/second
```

### 3. Move to Angles
```
Command: g<angle>

Examples:
  g90    - Move to 90°
  g180   - Move to 180°
  g-45   - Move to -45°
  g360   - Move to 360° (one full rotation from 0)
  g720   - Move to 720° (two full rotations)
```

### 4. Other Commands
```
r  - Reset current angle to 0° (recalibrate position)
q  - Query current angle
c  - Center/stop servo (stops rotation but keeps angle tracking)
```

## Example Session

```
> m90                    # Set continuous mode, 90 dps speed
[2143 ms] Servo 0: Continuous mode enabled
[2143 ms]   Speed: 90.0 degrees/second
[2143 ms]   Starting angle: 0.0°

> g90                    # Move to 90°
[5234 ms] Servo 0: Moving from 0.0° to 90.0° (Δ=90.0°)
[5234 ms]   Direction: CW
[5234 ms]   Speed: 90.0 dps, Time: 1000 ms
[5234 ms]   Pulse: 2500 µs
[6234 ms] Servo 0: Movement complete, now at 90.0°

> g180                   # Move to 180°
[8456 ms] Servo 0: Moving from 90.0° to 180.0° (Δ=90.0°)
[8456 ms]   Direction: CW
[8456 ms]   Speed: 90.0 dps, Time: 1000 ms
[8456 ms]   Pulse: 2500 µs
[9456 ms] Servo 0: Movement complete, now at 180.0°

> g0                     # Return to 0°
[11234 ms] Servo 0: Moving from 180.0° to 0.0° (Δ=-180.0°)
[11234 ms]   Direction: CCW
[11234 ms]   Speed: 90.0 dps, Time: 2000 ms
[11234 ms]   Pulse: 500 µs
[13234 ms] Servo 0: Movement complete, now at 0.0°

> q                      # Check current position
[15123 ms] Servo 0: Current angle = 0.0°
```

## Movement Calculation

The servo automatically calculates:

1. **Angle Difference**: `Δ = target_angle - current_angle`
2. **Direction**: 
   - Positive Δ → Clockwise (CW) → Pulse = 2500µs
   - Negative Δ → Counter-Clockwise (CCW) → Pulse = 500µs
3. **Time**: `movement_time = |Δ| / speed_dps`

### Example Calculations:

If speed = 90 dps:
- Move 90°: Time = 90/90 = 1000ms (1 second)
- Move 180°: Time = 180/90 = 2000ms (2 seconds)
- Move 45°: Time = 45/90 = 500ms (0.5 seconds)
- Move 360°: Time = 360/90 = 4000ms (4 seconds)

## Typical Servo Speeds

Common continuous servo speeds (at 5V):
- **Standard servos**: 60-90 dps
- **High-speed servos**: 120-180 dps
- **Slow servos**: 30-60 dps

Always measure your specific servo as speeds vary by model and voltage!

## Tips for Accurate Movement

1. **Calibrate Speed**: Measure rotation speed multiple times and average
2. **Account for Load**: Speed may vary with different loads
3. **Reset Position**: Use 'r' to recalibrate if position drifts
4. **Test First**: Try small movements (g10, g20) to verify accuracy
5. **Consider Voltage**: Speed varies with supply voltage (4.8V vs 6V)

## Troubleshooting

**Servo overshoots/undershoots target:**
- Your measured speed may be incorrect
- Re-measure and adjust the speed setting

**Servo doesn't move:**
- Check that continuous mode is enabled (`m<speed>`)
- Verify servo is connected and powered
- Try manual pulse test first (`p2500`, `p500`)

**Position drifts over time:**
- Continuous servos may have slight speed variations
- Use 'r' to reset position periodically
- Consider using positional servos for precise positioning

## Comparison: Positional vs Continuous Mode

| Feature | Positional Servo | Continuous Mode |
|---------|------------------|------------------|
| Range | ±90° or ±180° | Unlimited |
| Accuracy | Very high | Depends on speed calibration |
| Holding | Yes, active hold | No, must stop |
| Feedback | Internal potentiometer | Software tracking only |
| Use Case | Precise positions | Multi-rotation movements |

## Advanced Usage

### Multiple Rotations
```
g360   # One full rotation CW
g720   # Two full rotations CW
g-360  # One full rotation CCW
```

### Sequential Movements
```
m90    # Enable mode
g90    # Move to 90°
g180   # Move to 180° (continues from 90°)
g0     # Return to start
```

### Speed Adjustment
```
m60    # Slower, more precise
g180   # Takes 3 seconds (180/60)

m180   # Faster
g0     # Takes 1 second (180/180)
```
