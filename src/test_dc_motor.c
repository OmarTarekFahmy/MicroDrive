#include "pico/stdlib.h"
#include <stdio.h>
#include "drivers/dc_motor/dc_motor.h"

#define PIN_IN1 18
#define PIN_IN2 19
#define PIN_PWM 20
#define PWM_FREQ_HZ 20000u

int main(void) {
    stdio_init_all();
    sleep_ms(200);
    printf("test_dc_motor: starting\n");

    dc_motor_t motor;
    dc_motor_init(&motor, PIN_IN1, PIN_IN2, PIN_PWM, PWM_FREQ_HZ);

    printf("Coast 1s\n");
    dc_motor_coast(&motor);
    sleep_ms(1000);

    printf("Forward 50%% for 10s\n");
    dc_motor_set(&motor, 1.0f);
    sleep_ms(10000);

    printf("Reverse 75%% for 10s\n");
    dc_motor_set(&motor, -1.0f);
    sleep_ms(10000);

    printf("Brake 1s\n");
    dc_motor_brake(&motor);
    sleep_ms(1000);

    printf("Ramp forward 0->1\n");
    for (int i = 0; i <= 10; ++i) {
        float d = i / 10.0f;
        dc_motor_set(&motor, d);
        sleep_ms(200);
    }

    printf("Stop (coast)\n");
    dc_motor_set(&motor, 0.0f);
    sleep_ms(500);

    printf("Done — idling.\n");
    while (1) {
        tight_loop_contents();
    }

    return 0;
}
