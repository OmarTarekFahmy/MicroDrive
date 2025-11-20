#include "pico/stdlib.h"
#include "drivers/servo/servo.h"  // Include your servo code file here

int main() {
    // Initialize stdio (optional, for debugging)
    stdio_init_all();

    // Initialize servo on GPIO 15
    servo_init(15);

    while (true) {
        // Spin clockwise at full speed
        servo_set_speed(100);
        sleep_ms(2000);  // Spin for 2 seconds

        // Stop
        servo_set_speed(0);
        sleep_ms(1000);

        // Spin counter-clockwise at full speed
        servo_set_speed(-100);
        sleep_ms(2000);

        // Stop
        servo_set_speed(0);
        sleep_ms(1000);
    }

    return 0;
}
