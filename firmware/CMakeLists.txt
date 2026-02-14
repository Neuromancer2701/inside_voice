cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(inside_voice)

target_sources(app PRIVATE
    src/main.c
    src/app/config.c
    src/app/monitor.c
    src/audio/pdm_capture.c
    src/audio/sound_level.c
    src/ble/ble_manager.c
    src/ble/config_service.c
    src/feedback/led.c
    src/feedback/vibration.c
)
