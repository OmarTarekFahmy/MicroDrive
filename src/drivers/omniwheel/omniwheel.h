#include "pico/stdlib.h"

#ifndef OMNIWHEEL_H
#define OMNIWHEEL_H

typedef struct {
    uint8_t directionPin;
    uint8_t speedPin;

} Omniwheel;

// Function prototypes for omniwheel control
void omniwheel_init(Omniwheel* wheel, uint8_t directionPin, uint8_t speedPin);
void omniwheel_set_speed(Omniwheel* wheel, int speed);
void omniwheel_drive_quad(Omniwheel* wheelA, Omniwheel* wheelB, Omniwheel* wheelC, Omniwheel* wheelD, int speed, int angle, int rotation);
void omniwheel_stop(Omniwheel* wheel);

#endif // OMNIWHEEL_H