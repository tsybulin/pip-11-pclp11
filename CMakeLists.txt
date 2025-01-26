cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(PCLP11 C CXX ASM)

pico_sdk_init()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(FAMILY rp2040)
set(BOARD pico_sdk)

add_executable(${PROJECT_NAME} main.c lcd.c paint.c font24.c font16.c)

add_subdirectory(lib/sdcard)
add_subdirectory(lib/fatfs)

pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_enable_stdio_usb(${PROJECT_NAME} 0)

target_link_libraries(${PROJECT_NAME} sdcard fatfs pico_stdlib hardware_i2c hardware_spi hardware_pwm pico_i2c_slave pico_multicore)
pico_add_extra_outputs(${PROJECT_NAME})
