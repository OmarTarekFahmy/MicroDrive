cmake_minimum_required(VERSION 3.13)

# Import Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_projects C CXX ASM)

# Initialize the SDK
pico_sdk_init()

#--------------------------------------------------------------------
# Servo Test Project
#--------------------------------------------------------------------
add_executable(servo_test
    main.c
    drivers/servo/servo.c
)

# Include the servo driver directory
target_include_directories(servo_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/drivers/servo
)

# Link required Pico libraries
target_link_libraries(servo_test
    pico_stdlib
    hardware_pwm
)

# Enable USB stdio (optional, for debugging)
pico_enable_stdio_usb(servo_test 1)
pico_enable_stdio_uart(servo_test 0)

# Create UF2 and map binaries
pico_add_extra_outputs(servo_test)

#--------------------------------------------------------------------
# Camera Test Project (OV7670 USB Camera)
#--------------------------------------------------------------------
add_executable(camera_test
    main_camera.c
    usb_descriptors.c
    drivers/camera/ov7670.c
)

# Generate PIO header for image capture
pico_generate_pio_header(camera_test ${CMAKE_CURRENT_LIST_DIR}/drivers/camera/image.pio)

# Include directories
target_include_directories(camera_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/drivers/camera
)

# Link required Pico libraries including TinyUSB
target_link_libraries(camera_test
    pico_stdlib
    hardware_dma
    hardware_i2c
    hardware_pio
    hardware_pwm
    tinyusb_device
    tinyusb_board
)

# Disable USB stdio, enable UART stdio for camera (USB is used for video)
pico_enable_stdio_usb(camera_test 0)
pico_enable_stdio_uart(camera_test 1)

# Create UF2 and map binaries
pico_add_extra_outputs(camera_test)
