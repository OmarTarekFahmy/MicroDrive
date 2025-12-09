#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "drivers/Gyro/mpu6050.h" // Use the working gyro library
#include "drivers/lcd/lcd_i2c.h"
#include "drivers/servo/servo.h"
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

    // Initialize MPU6050 with new complementary-filter driver
    printf("Initializing MPU6050 (I2C1: GPIO 2=SDA, GPIO 3=SCL)...\n");
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Initializing");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("MPU6050...");

    mpu6050_init();

    printf("MPU6050 ready!\n");
    printf("Keep device COMPLETELY STILL for gyro calibration...\n");

    // Short settle, then calibrate gyro bias while still
    sleep_ms(500);
    printf("Calibrating gyro...\n");
    mpu6050_calibrate_gyro();

    // Give a moment after calibration, then capture reference orientation
    sleep_ms(500);
    mpu6050_set_reference();

    printf("Reference orientation set. Streaming delta angles.\n\n");

    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("MPU6050 Ready!");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Gyro calib+ref");
    sleep_ms(1500);
    lcd_i2c_clear();

    printf("Starting MPU6050 test loop...\n\n");

    // // Initialize servos for gyro-controlled platform (TEMPORARILY DISABLED)
    // printf("Initializing servos...\n");
    // servo_init(SERVO_X, SERVO_X_PIN, SERVO_TYPE_180); // Roll servo (180° positional)
    // servo_init(SERVO_Y, SERVO_Y_PIN, SERVO_TYPE_180); // Pitch servo (180° positional)
    // servo_init(SERVO_Z, SERVO_Z_PIN, SERVO_TYPE_180); // Yaw servo (180° positional)
    // printf("Servos initialized: X=GPIO%d, Y=GPIO%d, Z=GPIO%d\n\n",
    //        SERVO_X_PIN, SERVO_Y_PIN, SERVO_Z_PIN);

    printf("NOTE: Servo control is TEMPORARILY DISABLED - only printing angle deltas\n\n");

    last_touched = -1;
    uint32_t loop_count = 0;

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
                mpu6050_set_reference();
                printf("Reference orientation updated. Deltas reset to 0.\n");
            }

            last_touched = touched;
        }
        else if (touched < 0)
        {
            last_touched = -1;
        }

        // Update gyro readings (complementary filter runs internally)
        mpu6050_update();

        // Get change in orientation relative to the reference
        float dRoll = 0.0f;
        float dPitch = 0.0f;
        float dYaw = 0.0f;
        mpu6050_get_deltas(&dRoll, &dPitch, &dYaw);

        // SERVO UPDATES TEMPORARILY DISABLED
        // servo_set_angle(SERVO_X, -roll);  // Roll compensation
        // servo_set_angle(SERVO_Y, -pitch); // Pitch compensation
        // float yaw_scaled = yaw / 2.0f;
        // servo_set_angle(SERVO_Z, yaw_scaled);

        // Display on LCD - show live delta angle updates
        // Line 1: ΔRoll and ΔPitch
        snprintf(line1, sizeof(line1), "dR:%+5.1f dP:%+5.1f", dRoll, dPitch);
        // Line 2: ΔYaw
        snprintf(line2, sizeof(line2), "dY:%+6.1f deg", dYaw);

        lcd_i2c_set_cursor(0, 0);
        lcd_i2c_print(line1);
        lcd_i2c_set_cursor(1, 0);
        lcd_i2c_print(line2);

        // Console output - print every update (USB serial)
        printf("[dRoll:%+6.1f° dPitch:%+6.1f° dYaw:%+6.1f°] Temp:%.1f°C\n",
               dRoll, dPitch, dYaw, mpu6050_get_temperature_c());

        loop_count++;

        sleep_ms(UPDATE_INTERVAL_MS); // 200ms = 5Hz update rate (smooth servo control)
    }

    return 0;
}