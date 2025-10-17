cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_test C CXX ASM)
pico_sdk_init()

add_executable(pico_test main.c)
target_link_libraries(pico_test pico_stdlib)
pico_add_extra_outputs(pico_test)
