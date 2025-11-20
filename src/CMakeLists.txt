cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(MicroDrive C CXX ASM)

# Initialize the SDK
pico_sdk_init()

add_executable(microdrive
    main.c
    drivers/Gyro/mpu6050.c
    drivers/gyroservo/gyro_servo.c
    drivers/servo/servo.c
    drivers/lcd/lcd_i2c.c
    drivers/touch/touch_sensor.c
)


# Enable USB stdio
pico_enable_stdio_usb(microdrive 1)
pico_enable_stdio_uart(microdrive 0)

# Include the folder with drivers
target_include_directories(microdrive PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/drivers/Gyro
    ${CMAKE_CURRENT_LIST_DIR}/drivers/gyroservo
    ${CMAKE_CURRENT_LIST_DIR}/drivers/servo
    ${CMAKE_CURRENT_LIST_DIR}/drivers/lcd
    ${CMAKE_CURRENT_LIST_DIR}/drivers/touch
)


target_link_libraries(microdrive
    pico_stdlib
    hardware_i2c
    hardware_pwm
)

pico_add_extra_outputs(microdrive)
