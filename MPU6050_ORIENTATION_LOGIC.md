# MPU6050 Orientation Logic (Delta Angles)

This document explains the exact logic used in `drivers/Gyro/mpu6050.c` so you can verify that it matches the intended behaviour.

---

## 1. High-Level Behaviour

- The driver maintains an **internal estimate** of the sensor orientation: `pitch`, `roll`, `yaw` (in degrees).
- At startup, the sensor is configured for:
  - Gyro range: ±250 °/s
  - Accel range: ±2 g
- On each update:
  1. Raw accel/gyro data is read over I2C.
  2. Raw values are converted to physical units (g and deg/s).
  3. Tilt from the accelerometer is computed (pitch/roll only).
  4. Gyro rates are integrated over time to get pitch/roll/yaw from rotation.
  5. A **complementary filter** mixes the two sources:
     - Gyro: good short term, but drifts.
     - Accel: noisy short term, but stable long term.
- The user sets a **reference orientation** once (or any time), and all outputs are **deltas** from that reference: Δroll, Δpitch, Δyaw.

---

## 2. Sensor Configuration & I2C

- Bus: `i2c1`
- Pins: GPIO 2 = SDA, GPIO 3 = SCL
- Address: `0x68` (`MPU6050_ADDR`)

On `mpu6050_init()`:

1. I2C1 is configured at 400 kHz and pins 2/3 are set to I2C with pull-ups.
2. Registers written:
   - `PWR_MGMT_1` (0x6B) ← `0x00` to wake from sleep.
   - `GYRO_CONFIG` (0x1B) ← `0x00` → ±250 °/s full-scale.
   - `ACCEL_CONFIG` (0x1C) ← `0x00` → ±2 g full-scale.
3. Angle state is initialised to zero via `angles_init()`.

Raw data is read using a single 14-byte burst starting at `ACCEL_XOUT_H` (0x3B):

- Accel: 6 bytes → X, Y, Z (16-bit signed each)
- Temp: 2 bytes
- Gyro: 6 bytes → X, Y, Z (16-bit signed each)

---

## 3. Raw → Physical Conversion

The code uses the standard MPU6050 scale factors for the selected ranges:

- Gyro: `MPU6050_GYRO_SCALE = 131.0f` LSB/(°/s) for ±250 °/s
- Accel: `MPU6050_ACCEL_SCALE = 16384.0f` LSB/g for ±2 g

So, inside `mpu6050_update()`:

```c
float ax = (float)accel_raw[0] / MPU6050_ACCEL_SCALE; // g
float ay = (float)accel_raw[1] / MPU6050_ACCEL_SCALE;
float az = (float)accel_raw[2] / MPU6050_ACCEL_SCALE;

float gx = (float)gyro_raw[0] / MPU6050_GYRO_SCALE;   // deg/s
float gy = (float)gyro_raw[1] / MPU6050_GYRO_SCALE;
float gz = (float)gyro_raw[2] / MPU6050_GYRO_SCALE;
```

Result:

- $(a_x, a_y, a_z)$ are in g.
- $(g_x, g_y, g_z)$ are in °/s.

No explicit gyro bias calibration is applied in this version; any bias shows up as slow drift and is handled (partly) by the complementary filter and by letting you re-zero the reference.

---

## 4. Time Base (dt)

`mpu6050_update()` uses `absolute_time_diff_us()` to get an accurate time step:

```c
absolute_time_t now = get_absolute_time();
float dt = absolute_time_diff_us(last_time, now) / 1e6f;
last_time = now;
```

- `dt` is measured in **seconds**.
- This allows the same integration logic to work even if your update loop timing is not perfectly constant (e.g. if the main loop is delayed).

---

## 5. Accelerometer Tilt Angles

The accelerometer is used to estimate **tilt** angles (pitch and roll) relative to gravity.

In code (MPU6050 axis convention: X=left-right, Y=forward-back, Z=up):

```c
float acc_roll  = atan2f(ay, az) * 57.29578f;                     // roll around X
float acc_pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.29578f; // pitch around Y
```

This corresponds to:

- $acc\_roll  = \arctan\left(\dfrac{a_y}{a_z}\right) \cdot \dfrac{180}{\pi}$
- $acc\_pitch = \arctan\left(\dfrac{-a_x}{\sqrt{a_y^2 + a_z^2}}\right) \cdot \dfrac{180}{\pi}$

Notes:

- These formulas assume that gravity is the dominant acceleration.
- They give a stable long-term estimate of tilt, but they are noisy and can be corrupted when the device is moving quickly.

---

## 6. Gyro Integration

The gyro gives rotational rates $(g_x, g_y, g_z)$ in °/s. Integrating over time gives incremental changes in orientation.

The driver keeps internal state:

```c
static float pitch = 0.0f;
static float roll  = 0.0f;
static float yaw   = 0.0f;
```

For each update:

```c
float pitch_gyro = pitch + gx * dt;
float roll_gyro  = roll  + gy * dt;
float yaw_gyro   = yaw   + gz * dt;
```

That is:

- $pitch\_{gyro} = pitch + g_x \cdot dt$
- $roll\_{gyro} = roll + g_y \cdot dt$
- $yaw\_{gyro} = yaw + g_z \cdot dt$

If used alone, these would drift over time due to small biases in the gyro readings.

---

## 7. Complementary Filter

To combine short-term accurate gyro data with long-term stable accel data, the driver uses a **complementary filter**:

```c
const float alpha = MPU6050_ALPHA; // 0.98f

pitch = alpha * pitch_gyro + (1.0f - alpha) * acc_pitch;
roll  = alpha * roll_gyro  + (1.0f - alpha) * acc_roll;
yaw   = yaw_gyro; // no correction source for yaw
```

Mathematically:

- $pitch = \alpha \cdot pitch\_{gyro} + (1 - \alpha) \cdot acc\_{pitch}$
- $roll  = \alpha \cdot roll\_{gyro}  + (1 - \alpha) \cdot acc\_{roll}$
- $yaw   = yaw\_{gyro}$

With $\alpha = 0.98$:

- 98% of the new estimate comes from the **integrated gyro**.
- 2% comes from the **accelerometer tilt**.

This keeps pitch and roll responsive (because of the gyro) while slowly pulling them back toward the gravity-defined tilt to avoid long-term drift.

Yaw has no corresponding accelerometer reference, so it is currently **pure gyro integration** and will slowly drift over time.

---

## 8. Reference Orientation and Deltas

The driver provides a way to define a **zero reference** for orientation:

```c
static float pitch0 = 0.0f;
static float roll0  = 0.0f;
static float yaw0   = 0.0f;
```

When you call `mpu6050_set_reference()`:

```c
void mpu6050_set_reference(void) {
    pitch0 = pitch;
    roll0  = roll;
    yaw0   = yaw;
}
```

You are telling the driver: “From now on, treat the current pitch/roll/yaw as zero.”

To retrieve the deltas, you call `mpu6050_get_deltas()`:

```c
void mpu6050_get_deltas(float *dRoll, float *dPitch, float *dYaw) {
    float d_pitch = pitch - pitch0;
    float d_roll  = roll  - roll0;
    float d_yaw   = yaw   - yaw0;
    ...
}
```

So the outputs are:

- $\Delta roll = roll - roll_0$
- $\Delta pitch = pitch - pitch_0$
- $\Delta yaw = yaw - yaw_0$

Any pointer can be `NULL` if a particular axis is not needed.

In `main.c`:

- After `mpu6050_init()`, the program waits ~1 s while the device is still.
- Then it calls `mpu6050_set_reference()` to capture that pose as the baseline.
- In the main loop, it repeatedly calls:
  - `mpu6050_update()`
  - `mpu6050_get_deltas(&dRoll, &dPitch, &dYaw)`
- These deltas are printed over USB serial and displayed on the LCD.
- Touch 1 calls `mpu6050_set_reference()` again, effectively re-zeroing the deltas.

---

## 9. Temperature and Raw Access

For completeness:

- Temperature in °C is computed as:

  ```c
  float temp_c = (float)temp_raw / 340.0f + 36.53f;
  ```

  which matches the MPU6050 datasheet.

- Raw accel and gyro values (16-bit signed) are accessible via:

  - `mpu6050_get_raw_accel(&ax, &ay, &az);`
  - `mpu6050_get_raw_gyro(&gx, &gy, &gz);`

These functions simply return the last-read raw values.

---

## 10. Assumptions, Limitations, and Tuning

**Assumptions**

- During the reference capture (initial 1 s and any time you call `mpu6050_set_reference()`), the device is **approximately still** and only experiencing gravity.
- The update loop calls `mpu6050_update()` regularly (e.g. 50–200 Hz) so that `dt` stays reasonably small.

**Limitations**

- **Yaw drift**: Without a magnetometer or other absolute heading reference, yaw will drift over time.
- **Dynamic acceleration**: Strong linear accelerations (e.g. fast movement) can temporarily corrupt the accelerometer-based tilt, which may slightly pull pitch/roll estimates in the wrong direction.

**Tuning**

- `MPU6050_ALPHA` controls the blend between gyro and accel:
  - Closer to 1.0 → more responsive but more gyro drift.
  - Closer to 0.9 → more stable but more influenced by accel noise.
- If you observe:
  - Too much noise in pitch/roll → slightly decrease `ALPHA`.
  - Too much long-term drift → also decrease `ALPHA` or call `mpu6050_set_reference()` more frequently.

---

## 11. Sanity Checks for Correctness

To validate the logic in practice:

1. **Static test:**

   - Place the board flat on a table.
   - Set reference.
   - Verify that Δpitch, Δroll, Δyaw are near 0 and remain stable.

2. **Single-axis tilt:**

   - Slowly tilt the board forward/back (pitch) while keeping roll and yaw roughly constant.
   - Check that Δpitch changes smoothly while Δroll and Δyaw stay near 0.

3. **Side tilt:**

   - Tilt left/right (roll) and verify Δroll behaves similarly.

4. **Full rotation:**

   - Rotate around yaw; observe that Δyaw changes and slowly drifts over time, which is expected for gyro-only yaw.

5. **Re-zero:**
   - Use Touch 1 to call `mpu6050_set_reference()` in any pose.
   - All deltas should jump back to ~0 at that moment and then track changes from the new pose.

If all of these tests behave as described, the implementation in `mpu6050.c` is consistent with the logic defined here.
