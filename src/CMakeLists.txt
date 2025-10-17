cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(micro_drive C CXX ASM)
pico_sdk_init()

add_executable(micro_drive 
                main.c
                drivers/RGB_LED.c
)

target_link_libraries(micro_drive pico_stdlib)
pico_add_extra_outputs(micro_drive)
