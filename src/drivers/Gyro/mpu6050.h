#ifndef MPU6050_H
#define MPU6050_H

void mpu6050_setup(void);   // initializes I2C + MPU6050
void mpu6050_update(void);  // must be called every 10ms

float mpu6050_get_angle_x(void);
float mpu6050_get_angle_y(void);
float mpu6050_get_angle_z(void);

#endif
