#include "dc_motor.h"

#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include <math.h>

#ifndef PICO_DEFAULT_PWM_FREQ_HZ
#define PICO_DEFAULT_PWM_FREQ_HZ 20000u  // 20 kHz default
#endif

static void dc_motor_configure_pwm(dc_motor_t *motor, uint32_t pwm_freq_hz) {
    if (pwm_freq_hz == 0) {
        pwm_freq_hz = PICO_DEFAULT_PWM_FREQ_HZ;
    }

    // GPIO -> PWM
    gpio_set_function(motor->pin_pwm, GPIO_FUNC_PWM);
    motor->pwm_slice   = pwm_gpio_to_slice_num(motor->pin_pwm);
    motor->pwm_channel = pwm_gpio_to_channel(motor->pin_pwm);

    // clk_sys is usually 125 MHz on RP2040
    const uint32_t sys_clk_freq = clock_get_hz(clk_sys);

    // Choose wrap = 65535 for max resolution, compute clock divider
    uint16_t wrap = 65535;
    float clkdiv = (float)sys_clk_freq / (pwm_freq_hz * (wrap + 1));

    if (clkdiv < 1.0f) clkdiv = 1.0f;
    if (clkdiv > 255.0f) clkdiv = 255.0f;

    motor->pwm_wrap = wrap;

    pwm_set_clkdiv(motor->pwm_slice, clkdiv);
    pwm_set_wrap(motor->pwm_slice, motor->pwm_wrap);
    pwm_set_chan_level(motor->pwm_slice, motor->pwm_channel, 0);
    pwm_set_enabled(motor->pwm_slice, true);
}

void dc_motor_init(dc_motor_t *motor,
                   uint pin_in1,
                   uint pin_in2,
                   uint pin_pwm,
                   uint32_t pwm_freq_hz) {
    if (!motor) return;

    motor->pin_in1 = pin_in1;
    motor->pin_in2 = pin_in2;
    motor->pin_pwm = pin_pwm;

    // Direction pins
    gpio_init(pin_in1);
    gpio_set_dir(pin_in1, GPIO_OUT);
    gpio_put(pin_in1, 0);

    gpio_init(pin_in2);
    gpio_set_dir(pin_in2, GPIO_OUT);
    gpio_put(pin_in2, 0);

    // PWM pin
    dc_motor_configure_pwm(motor, pwm_freq_hz);

    // Start in coast mode, 0% duty
    dc_motor_coast(motor);
    dc_motor_set_duty(motor, 0.0f);
}

void dc_motor_set_direction(dc_motor_t *motor, dc_motor_dir_t dir) {
    if (!motor) return;

    switch (dir) {
    case DC_MOTOR_DIR_FORWARD:
        gpio_put(motor->pin_in1, 1);
        gpio_put(motor->pin_in2, 0);
        break;
    case DC_MOTOR_DIR_REVERSE:
        gpio_put(motor->pin_in1, 0);
        gpio_put(motor->pin_in2, 1);
        break;
    case DC_MOTOR_DIR_BRAKE:
        gpio_put(motor->pin_in1, 1);
        gpio_put(motor->pin_in2, 1);
        break;
    case DC_MOTOR_DIR_COAST:
    default:
        gpio_put(motor->pin_in1, 0);
        gpio_put(motor->pin_in2, 0);
        break;
    }
}

void dc_motor_set_duty(dc_motor_t *motor, float duty) {
    if (!motor) return;

    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    uint16_t level = (uint16_t)(duty * motor->pwm_wrap);
    pwm_set_chan_level(motor->pwm_slice, motor->pwm_channel, level);
}

void dc_motor_set(dc_motor_t *motor, float value) {
    if (!motor) return;

    // Clamp to [-1, 1]
    if (value > 1.0f) value = 1.0f;
    if (value < -1.0f) value = -1.0f;

    if (value > 0.0f) {
        dc_motor_set_direction(motor, DC_MOTOR_DIR_FORWARD);
        dc_motor_set_duty(motor, fabsf(value));
    } else if (value < 0.0f) {
        dc_motor_set_direction(motor, DC_MOTOR_DIR_REVERSE);
        dc_motor_set_duty(motor, fabsf(value));
    } else {
        // value == 0 -> coast, duty = 0
        dc_motor_coast(motor);
        dc_motor_set_duty(motor, 0.0f);
    }
}

void dc_motor_brake(dc_motor_t *motor) {
    if (!motor) return;
    dc_motor_set_direction(motor, DC_MOTOR_DIR_BRAKE);
}

void dc_motor_coast(dc_motor_t *motor) {
    if (!motor) return;
    dc_motor_set_direction(motor, DC_MOTOR_DIR_COAST);
}
