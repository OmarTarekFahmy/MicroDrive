cmake_minimum_required(VERSION 3.13)

# Set board to Pico W
set(PICO_BOARD pico_w)

# Import Pico SDK (from environment variable)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_projects C CXX ASM)

# Initialize the SDK
pico_sdk_init()

#--------------------------------------------------------------------
# Celestial Lock Project (Main project with gyro, servo, touch, buzzer, LEDs)
#--------------------------------------------------------------------
add_executable(servo_test
    ServoGyroWorking_needsTweaks.c
    drivers/servo/servo.c
    drivers/Gyro/mpu6050.c
    drivers/lcd/lcd_i2c.c
    drivers/touch/touch_sensor.c
    drivers/buzzer/buzzer.c
)

# Include the driver directories
target_include_directories(servo_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/drivers/servo
    ${CMAKE_CURRENT_LIST_DIR}/drivers/Gyro
    ${CMAKE_CURRENT_LIST_DIR}/drivers/lcd
    ${CMAKE_CURRENT_LIST_DIR}/drivers/touch
    ${CMAKE_CURRENT_LIST_DIR}/drivers/buzzer
)

# Link required Pico libraries
target_link_libraries(servo_test
    pico_stdlib
    hardware_pwm
    hardware_i2c
)

# Enable UART stdio for debugging (USB has issues with TinyUSB)
pico_enable_stdio_usb(servo_test 0)
pico_enable_stdio_uart(servo_test 1)

# Create UF2 and map binaries
pico_add_extra_outputs(servo_test)

#--------------------------------------------------------------------
# WiFi Camera Project (Pico W only)
#--------------------------------------------------------------------
add_executable(wifi_camera
    main_wifi_camera.c
    drivers/camera/ov7670.c
    drivers/wifi/wifi_camera.c
)

# Generate PIO header for image capture
pico_generate_pio_header(wifi_camera ${CMAKE_CURRENT_LIST_DIR}/drivers/camera/image.pio)

# Include project directories
target_include_directories(wifi_camera PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/drivers/camera
    ${CMAKE_CURRENT_LIST_DIR}/drivers/wifi
)

# Link required Pico libraries including WiFi + LWIP
target_link_libraries(wifi_camera
    pico_stdlib
    pico_cyw43_arch_lwip_poll
    hardware_i2c
    hardware_pio
    hardware_pwm
    hardware_dma
)

# Enable USB stdio for debugging via USB serial (no UART adapter needed!)
pico_enable_stdio_usb(wifi_camera 1)
pico_enable_stdio_uart(wifi_camera 0)

# Create UF2 and map binaries
pico_add_extra_outputs(wifi_camera)

#--------------------------------------------------------------------
# WiFi Camera Project - SIMPLE VERSION (using working USB camera code)
#--------------------------------------------------------------------
add_executable(wifi_camera_simple
    main_wifi_camera_simple.c
    drivers/camera/ov7670.c
    drivers/dc_motor/dc_motor.c
)

# Generate PIO header for image capture
pico_generate_pio_header(wifi_camera_simple ${CMAKE_CURRENT_LIST_DIR}/drivers/camera/image.pio)

# Include project directories
target_include_directories(wifi_camera_simple PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/drivers/camera
)

# Link required Pico libraries including WiFi + LWIP
target_link_libraries(wifi_camera_simple
    pico_stdlib
    pico_cyw43_arch_lwip_poll
    hardware_i2c
    hardware_pio
    hardware_pwm
    hardware_dma
)

# Enable USB stdio for debugging
pico_enable_stdio_usb(wifi_camera_simple 1)
pico_enable_stdio_uart(wifi_camera_simple 0)

# Create UF2 and map binaries
pico_add_extra_outputs(wifi_camera_simple)

#--------------------------------------------------------------------
# Serial Test (Simple USB Serial Test)
#--------------------------------------------------------------------
add_executable(
    test_serial
    test_serial.c
)

target_link_libraries(test_serial
    pico_stdlib
)

pico_enable_stdio_usb(test_serial 1)
pico_enable_stdio_uart(test_serial 0)

pico_add_extra_outputs(test_serial)

#--------------------------------------------------------------------
# DC Motor Test
#--------------------------------------------------------------------
add_executable(test_dc_motor
    test_dc_motor.c
    drivers/dc_motor/dc_motor.c    

)

target_include_directories(test_dc_motor PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/drivers/dc_motor   
)

target_link_libraries(test_dc_motor
    pico_stdlib
    hardware_pwm
    hardware_clocks     
)

pico_enable_stdio_usb(test_dc_motor 1)
pico_enable_stdio_uart(test_dc_motor 0)

pico_add_extra_outputs(test_dc_motor)

#--------------------------------------------------------------------
# WiFi Simple Test (WiFi connection test without camera)
#--------------------------------------------------------------------
add_executable(test_wifi_simple
    test_wifi_simple.c
)

target_include_directories(test_wifi_simple PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(test_wifi_simple
    pico_stdlib
    pico_cyw43_arch_lwip_poll
)

pico_enable_stdio_usb(test_wifi_simple 1)
pico_enable_stdio_uart(test_wifi_simple 0)

pico_add_extra_outputs(test_wifi_simple)

#--------------------------------------------------------------------
# Network Only Test (WiFi + TCP without camera)
#--------------------------------------------------------------------
add_executable(test_network_only
    test_network_only.c
)

target_include_directories(test_network_only PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(test_network_only
    pico_stdlib
    pico_cyw43_arch_lwip_poll
)

pico_enable_stdio_usb(test_network_only 1)
pico_enable_stdio_uart(test_network_only 0)

pico_add_extra_outputs(test_network_only)

# #--------------------------------------------------------------------
# # USB Camera Project (OV7670 with TinyUSB Video Class)
# #--------------------------------------------------------------------
# add_executable(usb_camera
#     main.c
#     usb_descriptors.c
#     drivers/camera/ov7670.c
# )

# # Generate PIO header for image capture
# pico_generate_pio_header(usb_camera ${CMAKE_CURRENT_LIST_DIR}/drivers/camera/image.pio)

# # Include project directories
# target_include_directories(usb_camera PRIVATE
#     ${CMAKE_CURRENT_LIST_DIR}
#     ${CMAKE_CURRENT_LIST_DIR}/drivers/camera
# )

# # Link required Pico libraries including TinyUSB
# target_link_libraries(usb_camera
#     pico_stdlib
#     hardware_i2c
#     hardware_pio
#     hardware_pwm
#     hardware_dma
#     tinyusb_device
#     tinyusb_board
# )

# # Enable UART stdio for debugging; USB is used for video
# pico_enable_stdio_usb(usb_camera 0)
# pico_enable_stdio_uart(usb_camera 1)

# # Create UF2 and map binaries
# pico_add_extra_outputs(usb_camera)