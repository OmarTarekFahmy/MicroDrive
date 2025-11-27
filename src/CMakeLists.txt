cmake_minimum_required(VERSION 3.13)

# Import Pico SDK (from environment variable)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(wifi_camera_project C CXX ASM)

# Initialize the SDK
pico_sdk_init()

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
    pico_cyw43_arch_lwip_poll                    # simpler poll-based LWIP (no threading)
    hardware_i2c
    hardware_pio
    hardware_pwm
    hardware_dma
)

# Enable UART stdio for debugging; disable USB stdio
pico_enable_stdio_usb(wifi_camera 0)
pico_enable_stdio_uart(wifi_camera 1)

# Create UF2 and map binaries
pico_add_extra_outputs(wifi_camera)
