cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(micro_drive C CXX ASM)
pico_sdk_init()

add_executable(micro_drive 
                main.c
                drivers/rgb_led/rgb_led.c
                drivers/led/led.c
                drivers/omniwheel/omniwheel.c
                drivers/gyroscope/gyroscope.c
                drivers/servo/servo.c
                drivers/lcd_i2c/lcd_i2c.c
                drivers/touch_keys/touch_keys.c
)

target_include_directories(micro_drive PRIVATE
                drivers/rgb_led
                drivers/led
                drivers/omniwheel
                drivers/gyroscope
                drivers/servo
                drivers/lcd_i2c
                drivers/touch_keys
)

target_link_libraries(micro_drive 
                pico_stdlib
                hardware_i2c
                hardware_pwm
)

pico_add_extra_outputs(micro_drive)
