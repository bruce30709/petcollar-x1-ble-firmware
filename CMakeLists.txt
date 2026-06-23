cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(evm_bt40_firmware)

target_sources(app PRIVATE
    src/main.c
    src/ble.c
    src/ble_pet_collar_service.c
    src/sensors.c
    src/led.c
    src/power.c
)

target_include_directories(app PRIVATE include)
