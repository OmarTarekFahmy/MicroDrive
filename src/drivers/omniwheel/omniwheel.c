#include "omniwheel.h"
#include "hardware/pwm.h"
#include <math.h>

#define M_PI 3.14159265358979323846

void omniwheel_init(Omniwheel* wheel, uint8_t directionPin, uint8_t speedPin) {
    
    gpio_set_function(speedPin, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(speedPin);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 4.0f); // Set clock divider
    pwm_init(slice_num, &cfg, true); // Initialize PWM with the config and start

    pwm_set_gpio_level(speedPin, 0);
    
    wheel->directionPin = directionPin;
    wheel->speedPin = speedPin;

    gpio_init(directionPin);
    gpio_set_dir(directionPin, true); // Set as output




}

void omniwheel_drive_quad(Omniwheel* wheelA, Omniwheel* wheelB, Omniwheel* wheelC, Omniwheel* wheelD, int speed, int angle, int rotation) {
    
    int A, B, C, D;

    A = speed * cosf((angle + 45) * (M_PI / 180.0f)) + rotation;
    B = speed * cosf((angle + 45) * (M_PI / 180.0f)) - rotation;
    C = speed * cosf((angle - 45) * (M_PI / 180.0f)) + rotation;
    D = speed * sinf((angle - 45) * (M_PI / 180.0f)) - rotation;



    if (A >= 0) {
        gpio_put(wheelA->directionPin, 1);
    } else {
        gpio_put(wheelA->directionPin, 0);
    }

    if (B >= 0) {
        gpio_put(wheelB->directionPin, 1);
    } else {
        gpio_put(wheelB->directionPin, 0);
    }

    if (C >= 0) {
        gpio_put(wheelC->directionPin, 1);
    } else {
        gpio_put(wheelC->directionPin, 0);
    }

    if (D >= 0) {
        gpio_put(wheelD->directionPin, 1);
    } else {
        gpio_put(wheelD->directionPin, 0);
    }

    A = fabs(A);
    B = fabs(B);
    C = fabs(C);
    D = fabs(D);

    //Clamp
    if (A > 255) A = 255;
    if (B > 255) B = 255;
    if (C > 255) C = 255;
    if (D > 255) D = 255;

    //Clamp negative
    if (A < 0) A = 0;
    if (B < 0) B = 0;
    if (C < 0) C = 0;
    if (D < 0) D = 0;

    int max_pwm = fmax(fmax(fabs(A), fabs(B)), fmax(fabs(C), fabs(D)));
    if (max_pwm > 255) {
        A = (A * 255) / max_pwm;
        B = (B * 255) / max_pwm;
        C = (C * 255) / max_pwm;
        D = (D * 255) / max_pwm;
    }

        // Set the PWM values for each wheel
    pwm_set_gpio_level(wheelA->speedPin, A);
    pwm_set_gpio_level(wheelB->speedPin, B);
    pwm_set_gpio_level(wheelC->speedPin, C);
    pwm_set_gpio_level(wheelD->speedPin, D);

}

void omniwheel_stop(Omniwheel* wheelA, Omniwheel* wheelB, Omniwheel* wheelC, Omniwheel* wheelD) {
    pwm_set_gpio_level(wheelA->speedPin, 0);
    pwm_set_gpio_level(wheelB->speedPin, 0);
    pwm_set_gpio_level(wheelC->speedPin, 0);
    pwm_set_gpio_level(wheelD->speedPin, 0);
}
