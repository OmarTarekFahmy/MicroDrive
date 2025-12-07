#ifndef MPU6050_H
#define MPU6050_H

// Initialize I2C and MPU6050, calibrates gyro (keep sensor still!)
void mpu6050_setup(void);

// Update readings - must be called regularly (every 50ms recommended)
void mpu6050_update(void);

// Get current angles in degrees
float mpu6050_get_angle_x(void);
float mpu6050_get_angle_y(void);
float mpu6050_get_angle_z(void);

// Reset all angles to zero
void mpu6050_reset_angles(void);

#endif
