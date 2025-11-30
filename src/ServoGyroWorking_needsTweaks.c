#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "drivers/Gyro/mpu6050.h"
#include "drivers/lcd/lcd_i2c.h"
#include "drivers/servo/servo.h"
#include <stdio.h>
#include <math.h>

// LCD I2C on I2C0 (GPIO 0 = SDA, GPIO 1 = SCL)
#define LCD_SDA_PIN 0
#define LCD_SCL_PIN 1
#define LCD_I2C_ADDR 0x27

// Servo GPIO pins - MG996R servos
#define SERVO_X_PIN 14  // 360° servo for X-axis
#define SERVO_Y_PIN 15  // 360° servo for Y-axis
#define SERVO_Z_PIN 16  // 180° servo for Z-axis

#define UPDATE_INTERVAL_MS 50

int main() {
    stdio_init_all();
    sleep_ms(2000);
    
    printf("=== MG996R Gyro Servo Control ===\n\n");
    
    // Initialize LCD
    lcd_i2c_init(i2c0, LCD_SDA_PIN, LCD_SCL_PIN, LCD_I2C_ADDR);
    printf("LCD initialized\n");
    
    // Initialize MPU6050 gyro
    mpu6050_setup();
    printf("MPU6050 initialized\n");
    
    // Initialize servos: X and Y are 360°, Z is 180°
    servo_init(SERVO_X, SERVO_X_PIN, SERVO_TYPE_360);  // 360° servo
    servo_init(SERVO_Y, SERVO_Y_PIN, SERVO_TYPE_360);  // 360° servo
    servo_init(SERVO_Z, SERVO_Z_PIN, SERVO_TYPE_180);  // 180° servo
    printf("Servos: X=GPIO%d(360), Y=GPIO%d(360), Z=GPIO%d(180)\n\n", 
           SERVO_X_PIN, SERVO_Y_PIN, SERVO_Z_PIN);
    
    // Center all servos at startup
    servo_center_all();
    
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Gyro Servo Ctrl");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Starting...");
    sleep_ms(1000);
    
    lcd_i2c_clear();
    
    char line1[17];
    char line2[17];
    
    printf("Starting gyro-servo control...\n");
    printf("Direct 1:1 mapping: Gyro angle = Servo angle\n");
    printf("Range: -90 to +90 degrees (0 = center)\n\n");
    
    while (1) {
        // Update gyro readings
        mpu6050_update();
        
        // Get angles from gyro (-90 to +90 range)
        float gyro_x = mpu6050_get_angle_x();
        float gyro_y = mpu6050_get_angle_y();
        float gyro_z = mpu6050_get_angle_z();
        
        // Direct 1:1 mapping per datasheet:
        // Gyro 0° -> Servo 0° (center, 1.5ms pulse)
        // Gyro +45° -> Servo +45° (1.75ms pulse)
        // Gyro -45° -> Servo -45° (1.25ms pulse)
        // servo_set_angle already clamps to -90 to +90
        
        // Set servo positions directly from gyro angles
        servo_set_angle(SERVO_X, gyro_x);
        servo_set_angle(SERVO_Y, gyro_y);
        servo_set_angle(SERVO_Z, gyro_z);
        
        // Format for LCD
        snprintf(line1, sizeof(line1), "X:%+4.0f Y:%+4.0f", gyro_x, gyro_y);
        snprintf(line2, sizeof(line2), "Z:%+4.0f deg", gyro_z);
        
        // Update LCD
        lcd_i2c_set_cursor(0, 0);
        lcd_i2c_print(line1);
        lcd_i2c_set_cursor(1, 0);
        lcd_i2c_print(line2);
        
        // Debug output (gyro = servo, direct 1:1)
        printf("Angle[X:%+6.1f Y:%+6.1f Z:%+6.1f]\n", gyro_x, gyro_y, gyro_z);
        
        sleep_ms(UPDATE_INTERVAL_MS);
    }
    
    return 0;
}
