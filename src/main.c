#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "drivers/MyMPUTest/yassinMpu.h" // Yassin's MPU6050 driver with calibration
#include "drivers/lcd/lcd_i2c.h"
#include "drivers/servo_test/servo_test.h"
#include "drivers/touch/touch_sensor.h"
#include "drivers/buzzer/buzzer.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

// LCD I2C on I2C0 (GPIO 0 = SDA, GPIO 1 = SCL)
#define LCD_SDA_PIN 0
#define LCD_SCL_PIN 1
#define LCD_I2C_ADDR 0x27

// Servo GPIO pins - MG996R servos
#define SERVO_X_PIN 14 // 360° servo for X-axis
#define SERVO_Y_PIN 15 // 360° servo for Y-axis
#define SERVO_Z_PIN 16 // 180° servo for Z-axis

// Servo IDs (matches initialization order)
#define SERVO_X 0  // Continuous servo on GPIO 14
#define SERVO_Y 1  // Continuous servo on GPIO 15
#define SERVO_Z 2  // Positional servo on GPIO 16

// Touch sensor pins (GPIO 9-12)
#define TOUCH_1_PIN 9
#define TOUCH_2_PIN 10
#define TOUCH_3_PIN 11
#define TOUCH_4_PIN 12

// Buzzer pin (GPIO 13)
#define BUZZER_PIN 13

// Electromagnet lock pin (GPIO 17) - connect through transistor/relay!
#define LOCK_PIN 17

// LED pins
#define LED_RED_PIN 18
#define LED_GREEN_PIN 19

// Lock sequence configuration
#define SEQUENCE_LENGTH 4
#define LOCK_OPEN_TIME_MS 5000 // Lock stays open for 5 seconds

// Secret sequence: Touch 1, 3, 2, 4 (like musical notes)
static const int8_t SECRET_SEQUENCE[SEQUENCE_LENGTH] = {0, 2, 1, 3}; // 0-indexed

// Sequence tracking
static int8_t entered_sequence[SEQUENCE_LENGTH];
static uint8_t sequence_index = 0;
static bool system_unlocked = false; // Track if system has been unlocked

#define UPDATE_INTERVAL_MS 200



// Initialize the electromagnet lock pin
void electromagnet_init(void)
{
    gpio_init(LOCK_PIN);
    gpio_set_dir(LOCK_PIN, GPIO_OUT);
    gpio_put(LOCK_PIN, 0); // Lock closed (LOW)
}

// Open the lock with initial nudge pulse
void electromagnet_open(void)
{
    // Nudge: rapid on-off pulses to help electromagnet engage
    for (int i = 0; i < 5; i++)
    {
        gpio_put(LOCK_PIN, 1);
        sleep_ms(50);
        gpio_put(LOCK_PIN, 0);
        sleep_ms(20);
    }

    // Now keep it ON
    gpio_put(LOCK_PIN, 1);
    printf("*** LOCK OPENED ***\n");
}

// Close the lock
void electromagnet_close(void)
{
    gpio_put(LOCK_PIN, 0); // LOW = electromagnet OFF = lock closed
    printf("*** LOCK CLOSED ***\n");
}

// Initialize LEDs
void leds_init(void)
{
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);

    // Start with RED on (locked state)
    gpio_put(LED_RED_PIN, 1);
    gpio_put(LED_GREEN_PIN, 0);
}

// Set LEDs for locked state (RED on, GREEN off)
void leds_locked(void)
{
    gpio_put(LED_RED_PIN, 1);
    gpio_put(LED_GREEN_PIN, 0);
}

// Set LEDs for unlocked state (RED off, GREEN on)
void leds_unlocked(void)
{
    gpio_put(LED_RED_PIN, 0);
    gpio_put(LED_GREEN_PIN, 1);
}

// Check sequence and return true if correct
bool check_sequence(void)
{
    if (sequence_index == SEQUENCE_LENGTH)
    {
        // Check if sequence matches
        bool match = true;
        for (int i = 0; i < SEQUENCE_LENGTH; i++)
        {
            if (entered_sequence[i] != SECRET_SEQUENCE[i])
            {
                match = false;
                break;
            }
        }

        // Reset sequence for next attempt
        sequence_index = 0;
        memset(entered_sequence, -1, sizeof(entered_sequence));

        if (match)
        {
            printf("Correct sequence!\n");
            return true;
        }
        else
        {
            // Wrong sequence
            printf("Wrong sequence! Try again.\n");

            // Play error tone
            buzzer_beep(200, 300);
            return false;
        }
    }
    return false;
}

int main()
{
    // Initialize stdio FIRST
    stdio_init_all();

    // Give USB time to enumerate properly (critical for reliable connection)
    sleep_ms(3000);

    // Immediate feedback that the program is running
    printf("\n\n\n");
    printf("====================================\n");
    printf("=== Celestial Lock System START ===\n");
    printf("====================================\n");
    printf("Build: %s %s\n", __DATE__, __TIME__);
    printf("System initializing...\n\n");

    // Initialize LCD first (keep it blank initially)
    lcd_i2c_init(i2c0, LCD_SDA_PIN, LCD_SCL_PIN, LCD_I2C_ADDR);
    lcd_i2c_clear();
    printf("LCD initialized (blank)\n");

    // Initialize touch sensors
    touch_init_all(TOUCH_1_PIN, TOUCH_2_PIN, TOUCH_3_PIN, TOUCH_4_PIN);
    printf("Touch sensors: GPIO %d, %d, %d, %d\n",
           TOUCH_1_PIN, TOUCH_2_PIN, TOUCH_3_PIN, TOUCH_4_PIN);

    // Initialize buzzer
    buzzer_init(BUZZER_PIN);
    printf("Buzzer: GPIO %d\n", BUZZER_PIN);

    // Initialize LEDs (RED on at start)
    leds_init();
    printf("LEDs: RED=GPIO %d, GREEN=GPIO %d\n", LED_RED_PIN, LED_GREEN_PIN);

    // Initialize electromagnet lock
    electromagnet_init();
    printf("Lock: GPIO %d\n", LOCK_PIN);
    printf("Secret sequence: Touch 1-3-2-4\n\n");

    // Initialize sequence tracking
    memset(entered_sequence, -1, sizeof(entered_sequence));

    char line1[17];
    char line2[17];
    static int8_t last_touched = -1;

    printf("=== PHASE 1: Waiting for unlock sequence ===\n");

    //    ========== PHASE 1: LOCK PHASE - Wait for correct sequence ==========
    while (!system_unlocked)
    {
        int8_t touched = touch_get_pressed();

        if (touched >= 0)
        {
            buzzer_play_touch_tone(touched);

            if (touched != last_touched)
            {
                printf("Touch %d pressed\n", touched + 1);

                // Add to sequence
                entered_sequence[sequence_index] = touched;
                sequence_index++;

                // Check if sequence complete and correct
                if (check_sequence())
                {
                    system_unlocked = true;
                }
            }
            last_touched = touched;
        }
        else
        {
            buzzer_stop();
            last_touched = -1;
        }

        sleep_ms(50);
    }

    // ========== CORRECT SEQUENCE ENTERED ==========
    printf("\n=== PHASE 2: System Unlocked ===\n");

    // Switch to green LED (stays green permanently)
    leds_unlocked();

    // Play success melody
    buzzer_beep(523, 100); // C5
    buzzer_beep(659, 100); // E5
    buzzer_beep(784, 200); // G5

    // Open electromagnet
    electromagnet_open();

    // Display "Celestial Lock Active" message for 5 seconds
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Celestial");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Lock Active.");
    sleep_ms(2500);

    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("MPU6050 Test");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Initializing...");
    sleep_ms(1000);

    // ========== PHASE 3: MPU6050 GYROSCOPE TEST ==========
    printf("\n=== PHASE 3: MPU6050 Gyroscope Test ===\n");

    // Initialize MPU6050 with yassinMpu driver
    printf("Initializing MPU6050 (I2C1: GPIO 2=SDA, GPIO 3=SCL)...\n");
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Initializing");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("MPU6050...");

    // Initialize the MPU6050 sensor
    bool mpu_initialized = false;
    while (!mpu_initialized) {
        if (mpu6050_init()) {
            mpu_initialized = true;
            break;
        }
        
        // Initialization failed
        printf("ERROR: MPU6050 initialization failed!\n");
        lcd_i2c_clear();
        lcd_i2c_set_cursor(0, 0);
        lcd_i2c_print("MPU6050 FAILED!");
        lcd_i2c_set_cursor(1, 0);
        lcd_i2c_print("Press touch 1");
        
        // Wait for touch 1 to retry
        printf("Press touch sensor 1 to retry initialization...\n");
        last_touched = -1;
        bool retry_requested = false;
        
        while (!retry_requested) {
            int8_t touched = touch_get_pressed();
            
            if (touched == 0 && touched != last_touched) {
                printf("Retry requested! Attempting MPU6050 initialization...\n");
                buzzer_beep(800, 100);
                lcd_i2c_clear();
                lcd_i2c_set_cursor(0, 0);
                lcd_i2c_print("Retrying MPU...");
                sleep_ms(500);
                retry_requested = true;
            }
            
            last_touched = touched;
            sleep_ms(50);
        }
    }

    printf("MPU6050 initialized successfully!\n");
    printf("Keep device COMPLETELY STILL for calibration...\n");

    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Calibrating...");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Keep still!");

    // Short settle time before calibration
    sleep_ms(2000);

    // Calibrate gyro and accelerometer offsets (1000 samples)
    mpu6050_calibrate(CALIBRATION_SAMPLES);

    // Give a moment after calibration
    sleep_ms(500);

    printf("Calibration complete! Starting orientation tracking.\n\n");

    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("MPU6050 Ready!");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Gyro calib+ref");
    sleep_ms(1500);
    lcd_i2c_clear();

    printf("Starting MPU6050 test loop...\n\n");

    // Initialize servos for gyro-controlled platform
    printf("Initializing servos...\n");
    servo_test_init(SERVO_X, SERVO_X_PIN);  // X-axis: continuous
    servo_test_init(SERVO_Y, SERVO_Y_PIN);  // Y-axis: continuous
    servo_test_init(SERVO_Z, SERVO_Z_PIN);  // Z-axis: positional
    
    // Enable continuous mode for X and Y servos (360° servos)
    // Speed: 187.6 dps based on your measurement (3600° in 19.19s)
    printf("\nEnabling continuous mode for X and Y servos...\n");
    servo_test_set_continuous_mode(SERVO_X, 187.6f);
    servo_test_set_continuous_mode(SERVO_Y, 187.6f);
    printf("Servos initialized: X=GPIO%d (cont), Y=GPIO%d (cont), Z=GPIO%d (pos)\n\n",
           SERVO_X_PIN, SERVO_Y_PIN, SERVO_Z_PIN);
    
    last_touched = -1;

    // Reference orientation for mirroring (captured at startup)
    float ref_roll = 0.0f;
    float ref_pitch = 0.0f;
    float ref_yaw = 0.0f;

    // Reset orientation to zero as our reference point
    mpu6050_reset_orientation();

    // Timing for dt calculation
    uint32_t last_time = to_ms_since_boot(get_absolute_time());
    uint32_t loop_count = 0;

    printf("=== Serial Monitor: Orientation Data ===\n");
    printf("Format: [Roll, Pitch, Yaw] in degrees\n\n");

    float OLDROLL = 0.0f;
    float OLDPITCH = 0.0f;
    float OLDYAW = 0.0f;

    // ========== MAIN TEST LOOP ==========
    while (1)
    {
        // Check for touch input
        int8_t touched = touch_get_pressed();

        if (touched >= 0 && touched != last_touched)
        {
            printf("Touch %d pressed\n", touched + 1);
            buzzer_beep(1000, 100);

            // Touch 1: Re-zero reference to current orientation
            if (touched == 0)
            {
                // Reset orientation angles to zero (new reference point)
                mpu6050_reset_orientation();
                ref_roll = 0.0f;
                ref_pitch = 0.0f;
                ref_yaw = 0.0f;

                // Center servos when resetting reference
                servo_test_reset_continuous_angle(SERVO_X);
                servo_test_reset_continuous_angle(SERVO_Y);
                servo_test_center(SERVO_Z);

                printf("[RESET] Orientation reset to zero. Servos centered.\n");
            }

            last_touched = touched;
        }
        else if (touched < 0)
        {
            last_touched = -1;
        }

        // Calculate time delta for integration
        uint32_t now = to_ms_since_boot(get_absolute_time());
        float dt = (now - last_time) / 1000.0f;  // Convert ms to seconds
        last_time = now;

        // Clamp dt to reasonable range (avoid huge jumps)
        if (dt > 0.5f) dt = 0.02f;  // Default to 20ms if too large
        if (dt < 0.001f) dt = 0.001f;  // Minimum 1ms

        // Update orientation using complementary filter
        mpu6050_update(dt);

        // Get current orientation angles
        float roll = mpu6050_get_roll();
        float pitch = mpu6050_get_pitch();
        float yaw = mpu6050_get_yaw();

        // These are already relative to the reset point (no ref subtraction needed)
        float dRoll = roll;
        float dPitch = pitch;
        float dYaw = yaw;

        // Apply rotation mirroring: servos move opposite to board tilt
        float servo_roll = -dRoll;   // mirror roll
        float servo_pitch = -dPitch; // mirror pitch
        float servo_yaw = -dYaw;     // optional yaw mirroring

        // Clamp to safe servo range (-90..+90)
        if (servo_roll > 90.0f) servo_roll = 90.0f;
        if (servo_roll < -90.0f) servo_roll = -90.0f;
        if (servo_pitch > 90.0f) servo_pitch = 90.0f;
        if (servo_pitch < -90.0f) servo_pitch = -90.0f;
        if (servo_yaw > 90.0f) servo_yaw = 90.0f;
        if (servo_yaw < -90.0f) servo_yaw = -90.0f;


        // IF DELTA ANGLE EXCEEDS 1 DEGREE, MOVE SERVOS

        if(fabs(servo_roll - OLDROLL) >1){
            servo_test_move_continuous_angle(SERVO_Y, roll);  // Y servo mirrors roll
            OLDROLL = servo_roll;
        }
        if(fabs(servo_pitch - OLDPITCH) >1){
            servo_test_move_continuous_angle(SERVO_X, pitch); // X servo mirrors pitch
            OLDPITCH = servo_pitch;
        }
        if(fabs(servo_yaw - OLDYAW) >1){
            servo_test_set_angle(SERVO_Z, yaw); // Z servo mirrors yaw (positional)
            OLDYAW = servo_yaw;
        }

        // Drive servos with mirrored angles
        // For continuous servos (X and Y): use servo_test_move_continuous_angle
        // For positional servo (Z): use servo_test_set_angle
        // servo_test_move_continuous_angle(SERVO_X, pitch); // X servo mirrors pitch
        // servo_test_move_continuous_angle(SERVO_Y, roll);  // Y servo mirrors roll
        // servo_test_set_angle(SERVO_Z, yaw); // Z servo mirrors yaw (positional)

        // Get temperature from sensor
        float temp_c = mpu6050_get_temperature();

        // Display on LCD - show live orientation angles
        // Line 1: Roll and Pitch
        snprintf(line1, sizeof(line1), "R:%+5.1f P:%+5.1f", roll, pitch);
        // Line 2: Yaw
        snprintf(line2, sizeof(line2), "Y:%+6.1f  %.1fC", yaw, temp_c);

        lcd_i2c_set_cursor(0, 0);
        lcd_i2c_print(line1);
        lcd_i2c_set_cursor(1, 0);
        lcd_i2c_print(line2);

        // Console output - detailed serial monitoring
        printf("[%6lu] Roll:%+7.2f° | Pitch:%+7.2f° | Yaw:%+7.2f° | Temp:%.1f°C | dt:%.3fs\n",
               loop_count, roll, pitch, yaw, temp_c, dt);

        loop_count++;

        sleep_ms(UPDATE_INTERVAL_MS); // 200ms = 5Hz update rate (smooth servo control)
    }

    return 0;
}