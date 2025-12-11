cmake_minimum_required(VERSION 3.13)

# Set board to Pico W
set(PICO_BOARD pico_w)

# Import Pico SDK (from environment variable)
include(/home/yassin/pico/pico-sdk/external/pico_sdk_import.cmake)

project(pico_projects C CXX ASM)

# Initialize the SDK
pico_sdk_init()

#--------------------------------------------------------------------
# Celestial Lock Project (Main project with gyro, servo, touch, buzzer, LEDs)
#--------------------------------------------------------------------
add_executable(servo_test
    main.c
    drivers/servo_test/servo_test.c
    drivers/MyMPUTest/yassinMpu.c
    drivers/lcd/lcd_i2c.c
    drivers/touch/touch_sensor.c
    drivers/buzzer/buzzer.c
    drivers/dc_motor/dc_motor.c
)

# Include the driver directories
target_include_directories(servo_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/drivers/servo
    ${CMAKE_CURRENT_LIST_DIR}/drivers/MyMPUTest
    ${CMAKE_CURRENT_LIST_DIR}/drivers/lcd
    ${CMAKE_CURRENT_LIST_DIR}/drivers/touch
    ${CMAKE_CURRENT_LIST_DIR}/drivers/buzzer
)

# Link required Pico libraries
target_link_libraries(servo_test
    pico_stdlib
    hardware_pwm
    hardware_i2c
    pico_cyw43_arch_lwip_poll
)

# Enable UART stdio for debugging
pico_enable_stdio_usb(servo_test 1)
pico_enable_stdio_uart(servo_test 1)

# Create UF2 and map binaries
pico_add_extra_outputs(servo_test)

#--------------------------------------------------------------------
# Servo Motor Pulse Width Test Program
# Use this to test if your servo is positional or continuous
#--------------------------------------------------------------------
add_executable(servo_pulse_test
    main_servo_test.c
    drivers/servo_test/servo_test.c
)

# Include the driver directories
target_include_directories(servo_pulse_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/drivers/servo_test
)

# Link required Pico libraries
target_link_libraries(servo_pulse_test
    pico_stdlib
    hardware_pwm
)

# Enable USB stdio for serial commands
pico_enable_stdio_usb(servo_pulse_test 1)
pico_enable_stdio_uart(servo_pulse_test 0)

# Create UF2 and map binaries
pico_add_extra_outputs(servo_pulse_test)

#--------------------------------------------------------------------
# WiFi Camera Project (Pico W only)
# TEMPORARILY DISABLED - lwIP socket configuration needed
#--------------------------------------------------------------------
# add_executable(wifi_camera
#     main_wifi_camera.c
#     drivers/camera/ov7670.c
#     drivers/wifi/wifi_camera.c
# )

# # Generate PIO header for image capture
# pico_generate_pio_header(wifi_camera ${CMAKE_CURRENT_LIST_DIR}/drivers/camera/image.pio)

# # Include project directories
# target_include_directories(wifi_camera PRIVATE
#     ${CMAKE_CURRENT_LIST_DIR}
#     ${CMAKE_CURRENT_LIST_DIR}/drivers/camera
#     ${CMAKE_CURRENT_LIST_DIR}/drivers/wifi
# )

# # Link required Pico libraries including WiFi + LWIP
# target_link_libraries(wifi_camera
#     pico_stdlib
#     pico_cyw43_arch_lwip_poll
#     pico_cyw43_arch_lwip_threadsafe_background
#     hardware_i2c
#     hardware_pio
#     hardware_pwm
#     hardware_dma
# )

# # Enable UART stdio for debugging only (disable USB to avoid TinyUSB conflicts)
# pico_enable_stdio_usb(wifi_camera 0)
# pico_enable_stdio_uart(wifi_camera 1)

# # Create UF2 and map binaries
# pico_add_extra_outputs(wifi_camera)

#--------------------------------------------------------------------
# USB Camera Project (OV7670 with TinyUSB Video Class)
# COMMENTED OUT: Old drivers not available
#--------------------------------------------------------------------
# add_executable(usb_camera
#     main.c
#     usb_descriptors.c
#     drivers/camera/ov7670.c
#     drivers/servo/servo.c
#     drivers/Gyro/mpu6050.c
#     drivers/lcd/lcd_i2c.c
#     drivers/touch/touch_sensor.c
#     drivers/buzzer/buzzer.c
# )

# # Generate PIO header for image capture
# pico_generate_pio_header(usb_camera ${CMAKE_CURRENT_LIST_DIR}/drivers/camera/image.pio)
# 
# # Include project directories
# target_include_directories(usb_camera PRIVATE
#     ${CMAKE_CURRENT_LIST_DIR}
#     ${CMAKE_CURRENT_LIST_DIR}/drivers/camera
#     ${CMAKE_CURRENT_LIST_DIR}/drivers/servo
#     ${CMAKE_CURRENT_LIST_DIR}/drivers/Gyro
#     ${CMAKE_CURRENT_LIST_DIR}/drivers/lcd
#     ${CMAKE_CURRENT_LIST_DIR}/drivers/touch
#     ${CMAKE_CURRENT_LIST_DIR}/drivers/buzzer
# )
# 
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
# 
# # Enable UART stdio for debugging; USB is used for video
# pico_enable_stdio_usb(usb_camera 0)
# pico_enable_stdio_uart(usb_camera 1)
# 
# # Create UF2 and map binaries
# pico_add_extra_outputs(usb_camera)