#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/watchdog.h"
#include "drivers/mpu9250/mpu9250.h" // MPU9250 9-DOF IMU with magnetometer
#include "drivers/lcd/lcd_i2c.h"
#include "drivers/servo_test/servo_test.h"
#include "drivers/touch/touch_sensor.h"
#include "drivers/buzzer/buzzer.h"
#include "drivers/rgb_led/rgb_led.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "drivers/dc_motor/dc_motor.h"
#include "FreeRTOS.h"
#include "task.h"

// DC Motor (H-bridge) pins
#define PIN_IN1 4
#define PIN_IN2 5
#define PIN_PWM 6
#define PWM_FREQ_HZ 20000u

// LCD I2C on I2C0 (GPIO 0 = SDA, GPIO 1 = SCL)
#define LCD_SDA_PIN 0
#define LCD_SCL_PIN 1
#define LCD_I2C_ADDR 0x27

// Servo GPIO pins - MG996R servos (ALL POSITIONAL 180°)
#define SERVO_X_PIN 14 // 180° positional servo for X-axis (Pitch)
#define SERVO_Y_PIN 15 // 180° positional servo for Y-axis (Roll)
#define SERVO_Z_PIN 18 // 180° positional servo for Z-axis (Yaw)

// Servo IDs (matches initialization order)
#define SERVO_X 0     // Positional servo on GPIO 14 (Pitch)
#define SERVO_Y 1     // Positional servo on GPIO 15 (Roll)
#define SERVO_Z 2     // Positional servo on GPIO 18 (Yaw)
#define SERVO_LOCK 3  // Lock servo on GPIO 20

// Lock servo pin (CONTINUOUS SERVO for door open/close)
#define SERVO_LOCK_PIN 20  // Continuous servo for door mechanism

// Touch sensor pins (GPIO 9-12)
// Touch 4 (GPIO 12) is used as RESET after access granted
#define TOUCH_1_PIN 9
#define TOUCH_2_PIN 10
#define TOUCH_3_PIN 11
#define TOUCH_4_PIN 12  // Also used as RESET touch
#define RESET_TOUCH_ID 3  // Touch 4 = index 3 (0-indexed)

// Buzzer pin (GPIO 13)
#define BUZZER_PIN 13

// Electromagnet lock pin (GPIO 17) - connect through transistor/relay!
#define LOCK_PIN 17

// LED pins
#define LED_RED_PIN 18
#define LED_GREEN_PIN 19

// RGB LED pins
#define RGB_RED_PIN 21
#define RGB_GREEN_PIN 22
#define RGB_BLUE_PIN 26

// Lock sequence configuration
#define SEQUENCE_LENGTH 4
#define LOCK_OPEN_TIME_MS 5000 // Lock stays open for 5 seconds

// Secret sequence: Touch 1, 3, 2, 4 (like musical notes)
static const int8_t SECRET_SEQUENCE[SEQUENCE_LENGTH] = {0, 2, 1, 3}; // 0-indexed

// Sequence tracking
static int8_t entered_sequence[SEQUENCE_LENGTH];
static uint8_t sequence_index = 0;
static bool system_unlocked = false; // Track if system has been unlocked
static volatile bool reset_requested = false; // Flag to reset game

// RGB LED instance
static RGB_LED rgb_led;

#define UPDATE_INTERVAL_MS 200

// WiFi credentials - SAME AS CAMERA
#define WIFI_SSID "Yassin"
#define WIFI_PASSWORD "YassoSchool1234"
#define SERVER_IP "172.20.10.6"
#define ACTUATOR_PORT 9999

// Command from server
#define CMD_UNLOCK 0x01


// TCP state
static struct tcp_pcb *client_pcb = NULL;
static bool connected = false;
static uint32_t unlock_count = 0;


// TCP callbacks
static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        printf("[TCP] Connection closed by server\n");
        tcp_close(tpcb);
        connected = false;
        return ERR_OK;
    }
    
    // Process received data
    uint8_t *data = (uint8_t *)p->payload;
    for (uint16_t i = 0; i < p->len; i++) {
        if (data[i] == CMD_UNLOCK) {
            unlock_count++;
            printf("\n");
            printf("========================================\n");
            printf("🔓 UNLOCK SIGNAL RECEIVED! (Count: %lu)\n", unlock_count);
            printf("========================================\n");
            printf("\n");
            
            // TODO: Trigger your motor/servo/relay here
            // Example: unlock_door();
            // Example: servo_move_to_position(90);
            // Example: gpio_put(RELAY_PIN, 1);
            
            // Blink LED to indicate unlock
            for (int j = 0; j < 5; j++) {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                sleep_ms(100);
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                sleep_ms(100);
            }
        } else {
            printf("[ACTUATOR] Unknown command: 0x%02X\n", data[i]);
        }
    }
    
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("[TCP] Connection failed: %d\n", err);
        connected = false;
        return err;
    }
    
    printf("[TCP] Connected to server!\n");
    connected = true;
    
    // Set receive callback
    tcp_recv(tpcb, tcp_client_recv);
    
    // Send ready message
    const char *ready_msg = "ACTUATOR_READY";
    err_t write_err = tcp_write(tpcb, ready_msg, strlen(ready_msg), TCP_WRITE_FLAG_COPY);
    if (write_err == ERR_OK) {
        tcp_output(tpcb);
        printf("[TCP] Sent ACTUATOR_READY message\n");
    }
    
    return ERR_OK;
}

static void tcp_client_err(void *arg, err_t err) {
    printf("[TCP] Connection error: %d\n", err);
    connected = false;
    client_pcb = NULL;
}

// Connect to server - SAME PATTERN AS CAMERA
bool connect_to_server(const char *ip, uint16_t port) {
    client_pcb = tcp_new();
    if (!client_pcb) {
        printf("[TCP] Failed to create PCB\n");
        return false;
    }
    
    ip_addr_t server_addr;
    if (!ip4addr_aton(ip, &server_addr)) {
        printf("[TCP] Invalid IP address\n");
        return false;
    }
    
    tcp_arg(client_pcb, NULL);
    tcp_err(client_pcb, tcp_client_err);
    
    err_t err = tcp_connect(client_pcb, &server_addr, port, tcp_client_connected);
    if (err != ERR_OK) {
        printf("[TCP] Connect failed: %d\n", err);
        return false;
    }
    
    // Wait for connection
    int timeout = 100;
    while (!connected && timeout-- > 0) {
        cyw43_arch_poll();
        sleep_ms(100);
    }
    
    return connected;
}



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

// Check if reset touch (Touch 4) is pressed
bool reset_touch_pressed(void)
{
    return touch_get_pressed() == RESET_TOUCH_ID;  // Returns true when Touch 4 pressed
}

// Flash RGB LED blue rapidly (for final alignment indication)
void rgb_led_flash_blue(int times, int delay_ms)
{
    for (int i = 0; i < times; i++)
    {
        rgb_led_preset_color(&rgb_led, COLOR_BLUE);
        sleep_ms(delay_ms);
        rgb_led_off(&rgb_led);
        sleep_ms(delay_ms);
    }
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

// FreeRTOS task for WiFi polling (runs in background)
void wifi_polling_task(void *params) {
    while (1) {
        if (connected) {
            cyw43_arch_poll();
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Poll every 50ms for responsiveness
    }
}

int main()
{
    // Initialize stdio FIRST
    stdio_init_all();

    dc_motor_t motor;
    dc_motor_init(&motor, PIN_IN1, PIN_IN2, PIN_PWM, PWM_FREQ_HZ);


    // Initialize WiFi - SAME AS CAMERA
    printf("[WiFi] Initializing...\n");
    if (cyw43_arch_init()) {
        printf("[ERROR] WiFi init failed\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();

    printf("[WiFi] Connecting to '%s'...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID,
            WIFI_PASSWORD,
            CYW43_AUTH_WPA2_AES_PSK,
            30000)) {
        printf("[ERROR] WiFi connection failed\n");
        cyw43_arch_deinit();
        return -1;
    }
    printf("[WiFi] Connected!\n");
    // Connect to actua
    // Give USB time to enumerate properly (critical for reliable connection)
    sleep_ms(3000);

    // Immediate feedback that the program is running
    printf("\n\n\n");
    printf("====================================\n");
    printf("=== Celestial Lock System START ===\n");
    printf("====================================\n");
    printf("Build: %s %s\n", __DATE__, __TIME__);
    printf("System initializing...\n\n");

    printf("[TCP] Connecting to %s:%d...\n", SERVER_IP, ACTUATOR_PORT);
    if (!connect_to_server(SERVER_IP, ACTUATOR_PORT)) {
        printf("[ERROR] Server connection failed\n");
        cyw43_arch_deinit();
        return -1;
    }

    // ========== CREATE FREERTOS TASK FOR WIFI POLLING ==========
    // Task creation right after TCP connection to keep it alive during all phases
    // TCP connections need continuous polling to prevent timeout
    printf("[FreeRTOS] Creating WiFi polling task...\n");
    xTaskCreate(
        wifi_polling_task,    // Task function
        "WiFi_Poll",          // Task name
        256,                  // Stack size (words)
        NULL,                 // Parameters
        1,                    // Priority
        NULL                  // Task handle
    );
    printf("[FreeRTOS] WiFi polling task started (keeps TCP connection alive)\n");


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

    // Initialize RGB LED (starts red - inactive/locked)
    rgb_led_init(&rgb_led, RGB_RED_PIN, RGB_GREEN_PIN, RGB_BLUE_PIN);
    rgb_led_preset_color(&rgb_led, COLOR_RED);
    printf("RGB LED: R=GPIO%d, G=GPIO%d, B=GPIO%d (RED=inactive)\n", 
           RGB_RED_PIN, RGB_GREEN_PIN, RGB_BLUE_PIN);

    // Note: Touch 4 will be used as RESET after access is granted
    printf("Reset: Touch 4 (GPIO %d) - press after access granted to restart\n", TOUCH_4_PIN);

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



    printf("[Actuator] Waiting for unlock signals...\n");
    
    // Switch to green LED (stays green permanently)
    leds_unlocked();
    
    // Switch RGB LED to green (system unlocked)
    rgb_led_preset_color(&rgb_led, COLOR_GREEN);
    printf("RGB LED: GREEN (unlocked)\n");

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

    // Initialize the MPU9250 sensor
    bool mpu_initialized = false;
    while (!mpu_initialized) {
        if (mpu9250_init()) {
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
    mpu9250_calibrate_accel_gyro(1000);

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

    // Initialize servos for IMU-controlled platform
    printf("Initializing servos...\n");
    servo_test_init(SERVO_X, SERVO_X_PIN);  // X-axis: positional (Pitch)
    servo_test_init(SERVO_Y, SERVO_Y_PIN);  // Y-axis: positional (Roll)
    servo_test_init(SERVO_Z, SERVO_Z_PIN);  // Z-axis: positional (Yaw)
    servo_test_init(SERVO_LOCK, SERVO_LOCK_PIN);  // Lock servo
    
    // All servos are now positional 180° servos
    printf("Servos initialized: X=GPIO%d (Pitch), Y=GPIO%d (Roll), Z=GPIO%d (Yaw), LOCK=GPIO%d\n\n",
           SERVO_X_PIN, SERVO_Y_PIN, SERVO_Z_PIN, SERVO_LOCK_PIN);
    
    last_touched = -1;

    // Reference orientation for mirroring (captured at startup)
    float ref_roll = 0.0f;
    float ref_pitch = 0.0f;
    float ref_yaw = 0.0f;
    
    // Yaw offset for mapping 0-360° to -90 to +90°
    float yaw_offset = 0.0f;

    // Reset orientation to zero as our reference point
    mpu9250_reset_orientation();

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
                // Get current orientation to calculate yaw offset
                mpu9250_orientation_t current_orient;
                mpu9250_get_orientation(&current_orient);
                
                // Set yaw offset to map current yaw to center (0°)
                yaw_offset = current_orient.yaw;
                
                // Reset orientation angles to zero (new reference point)
                mpu9250_reset_orientation();
                ref_roll = 0.0f;
                ref_pitch = 0.0f;
                ref_yaw = 0.0f;

                // Center all servos (all are positional now)
                servo_test_center(SERVO_X);
                servo_test_center(SERVO_Y);
                servo_test_center(SERVO_Z);

                printf("[RESET] Orientation reset to zero. Servos centered. Yaw offset: %.2f°\n", yaw_offset);
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

        // Update orientation using complementary filter + magnetometer
        mpu9250_update_orientation(dt);

        // Get current orientation angles from MPU9250
        mpu9250_orientation_t orientation;
        mpu9250_get_orientation(&orientation);
        
        float roll = orientation.roll;
        float pitch = orientation.pitch;
        float yaw_raw = orientation.yaw;  // MPU9250 gives 0-360°
        
        // Map yaw from 0-360° to -90 to +90° range for servo control
        // Apply offset and wrap to -90..+90 range
        float yaw = yaw_raw - yaw_offset;
        
        // Normalize to -180..+180
        while (yaw > 180.0f) yaw -= 360.0f;
        while (yaw < -180.0f) yaw += 360.0f;
        
        // Clamp to -90..+90 for servo range
        if (yaw > 90.0f) yaw = 90.0f;
        if (yaw < -90.0f) yaw = -90.0f;

        // These are already relative to the reset point (no ref subtraction needed)
        float dRoll = roll;
        float dPitch = pitch;
        float dYaw = yaw;

        // Apply rotation mirroring: servos move opposite to board tilt
        float servo_roll = dRoll;   // mirror roll
        float servo_pitch = dPitch; // mirror pitch
        float servo_yaw = dYaw;     // optional yaw mirroring

        // Clamp to safe servo range (-90..+90)
        if (servo_roll > 90.0f) servo_roll = 90.0f;
        if (servo_roll < -90.0f) servo_roll = -90.0f;
        if (servo_pitch > 90.0f) servo_pitch = 90.0f;
        if (servo_pitch < -90.0f) servo_pitch = -90.0f;
        if (servo_yaw > 90.0f) servo_yaw = 90.0f;
        if (servo_yaw < -90.0f) servo_yaw = -90.0f;


        // Drive servos with mirrored angles (all positional servos)
        // Only update if angle change is significant (> 1 degree)
        if(fabs(servo_roll - OLDROLL) > 1){
            servo_test_set_angle(SERVO_Y, servo_roll);  // Y servo mirrors roll
            OLDROLL = servo_roll;
        }
        if(fabs(servo_pitch - OLDPITCH) > 1){
            servo_test_set_angle(SERVO_X, servo_pitch); // X servo mirrors pitch
            OLDPITCH = servo_pitch;
        }
        if(fabs(servo_yaw - OLDYAW) > 1){
            servo_test_set_angle(SERVO_Z, servo_yaw); // Z servo mirrors yaw
            OLDYAW = servo_yaw;
        }

        // Get temperature from sensor
        float temp_c = mpu9250_get_temperature();

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

        if (connected) {
            // WiFi polling now handled by FreeRTOS background task
            if (unlock_count > 0) {
                break;
            }
        }

        sleep_ms(UPDATE_INTERVAL_MS); // 200ms = 5Hz update rate (smooth servo control)
    }

    // ========== FINAL ALIGNMENT COMPLETE ==========
    printf("\n=== FINAL ALIGNMENT COMPLETE ===\n");
    printf("Unlock signal received from other Pico!\n");
    
    // Display "Target Locked" message on LCD
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Target Locked:");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Initiating Key");
    sleep_ms(1500);
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("Key Sequence");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Activated...");
    
    // Flash RGB LED blue rapidly to indicate alignment completion
    printf("RGB LED: BLUE FLASHING (final alignment)\n");
    rgb_led_flash_blue(10, 100);  // Flash 10 times, 100ms on/off
    
    // Keep RGB LED blue during key sequence
    rgb_led_preset_color(&rgb_led, COLOR_BLUE);

    // ========== ACTIVATE DC MOTOR KEY SEQUENCE ==========
    printf("\n=== EXECUTING KEY SEQUENCE ===\n");

    // Step 1: Clockwise rotation at MAXIMUM speed (100% PWM) for 1.5 seconds
    printf("DC Motor: Clockwise MAX SPEED (100%% PWM) for 1.5s\n");
    dc_motor_set(&motor, 1.0f);  // 1.0 = 100% = full speed clockwise
    sleep_ms(1500);
    
    // Step 2: Stop and wait 0.5 seconds
    printf("DC Motor: Stopping, waiting 0.5s\n");
    dc_motor_set(&motor, 0.0f);
    sleep_ms(500);
    
    // Step 3: Counter-clockwise rotation at HALF speed (50% PWM) for 2.5 seconds
    printf("DC Motor: Counter-Clockwise HALF SPEED (50%% PWM) for 2.5s\n");
    dc_motor_set(&motor, -0.5f);  // -0.5 = -50% for better torque (was too weak at -0.5)
    sleep_ms(2500);
    
    // Step 4: Stop motor
    printf("DC Motor: Stopping\n");
    dc_motor_set(&motor, 0.0f);
    
    // Step 5: Pull electromagnet (turn ON to engage)
    printf("\n=== PULLING ELECTROMAGNET ===\n");
    electromagnet_open();  // GPIO 17 = HIGH = Magnet ON = Pull
    printf("Electromagnet: ON (pulling)\n");
    
    // Step 6: Continuous servo anti-clockwise for 2 seconds to open door
    // Pulse width: 1000µs = full speed anti-clockwise, 1500µs = stop, 2000µs = full speed clockwise
    printf("\n=== OPENING DOOR (CONTINUOUS SERVO) ===\n");
    printf("Door Servo: Anti-clockwise for 2 seconds...\n");
    servo_test_set_pulse_us(SERVO_LOCK, 1000);  // Full speed anti-clockwise
    sleep_ms(2000);
    servo_test_set_pulse_us(SERVO_LOCK, 1500);  // Stop
    printf("Door Servo: Stopped (door open)\n");
    
    // Update LCD
    lcd_i2c_clear();
    lcd_i2c_set_cursor(0, 0);
    lcd_i2c_print("UNLOCKED!");
    lcd_i2c_set_cursor(1, 0);
    lcd_i2c_print("Access Granted");
    
    // Change RGB LED to green (success)
    rgb_led_preset_color(&rgb_led, COLOR_GREEN);
    
    printf("*** SEQUENCE COMPLETE - ACCESS GRANTED ***\n");
    
    // ========== WAIT FOR RESET (Touch 4) ==========
    printf("\n=== Touch TOUCH 4 (GPIO %d) to restart game ===\n", TOUCH_4_PIN);
    
    while (1) {
        if (reset_touch_pressed()) {
            printf("\n*** RESET TOUCH PRESSED - RESTARTING GAME ***\n");
            
            // Play reset tone
            buzzer_beep(500, 200);
            
            // Debounce - wait for release
            sleep_ms(200);
            while (reset_touch_pressed()) {
                sleep_ms(50);
            }
            
            // Close door (continuous servo clockwise for 2 seconds)
            printf("Closing door...\n");
            servo_test_set_pulse_us(SERVO_LOCK, 2000);  // Full speed clockwise
            sleep_ms(2000);
            servo_test_set_pulse_us(SERVO_LOCK, 1500);  // Stop
            
            // Release electromagnet
            printf("Releasing electromagnet...\n");
            electromagnet_close();
            
            // Center all servos
            printf("Centering servos...\n");
            servo_test_center(SERVO_X);
            servo_test_center(SERVO_Y);
            servo_test_center(SERVO_Z);
            
            // Reset RGB to RED
            rgb_led_preset_color(&rgb_led, COLOR_RED);
            
            // Clear LCD
            lcd_i2c_clear();
            
            // Reset LEDs to locked state
            leds_locked();
            
            // Reset sequence tracking
            sequence_index = 0;
            memset(entered_sequence, -1, sizeof(entered_sequence));
            system_unlocked = false;
            unlock_count = 0;
            
            // Re-initialize MPU9250
            printf("Re-initializing MPU9250...\n");
            mpu9250_reset_orientation();
            
            printf("\n*** GAME RESET - Waiting for sequence ***\n");
            printf("Secret sequence: Touch 1-3-2-4\n\n");
            
            // Software reset - use watchdog to perform a clean reset
            watchdog_reboot(0, 0, 0);
        }
        sleep_ms(100);
    }

    return 0;
}