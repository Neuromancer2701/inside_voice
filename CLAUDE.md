# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

inside_voice — a Zephyr RTOS firmware for a lapel pin wearable (Seeed Studio XIAO nRF52840 Sense) that monitors voice volume and provides LED + haptic feedback when a loudness threshold is exceeded. MIT license.

## Toolchain

- **RTOS:** Zephyr v3.7 LTS (upstream, not nRF Connect SDK)
- **Board:** `xiao_ble/nrf52840/sense`
- **Build environment:** Dockerized (`firmware/Dockerfile` based on `ghcr.io/zephyrproject-rtos/zephyr-build`)
- **Language:** C (Zephyr kernel APIs)

## Build Commands

```bash
cd firmware

# Build Docker image (first time ~15-20 min)
docker compose build

# Compile firmware
docker compose run --rm firmware

# Explicit build
docker compose run --rm firmware west build -p always -b xiao_ble/nrf52840/sense .
```

Output: `firmware/build/zephyr/zephyr.uf2`

## Project Structure

```
firmware/
├── Dockerfile              # Zephyr SDK + workspace
├── docker-compose.yml      # Dev environment config
├── CMakeLists.txt          # Zephyr CMake project
├── prj.conf                # Kconfig (BT, DMIC, PWM, GPIO, NVS, USB, logging)
├── boards/
│   └── xiao_ble_nrf52840_sense.overlay  # Devicetree overlay (PDM, PWM, LEDs)
└── src/
    ├── main.c              # Entry: init subsystems, start monitor
    ├── app/
    │   ├── config.{h,c}    # NVS-backed settings (threshold, feedback mode)
    │   └── monitor.{h,c}   # Core loop: audio → threshold → feedback → BLE
    ├── audio/
    │   ├── pdm_capture.{h,c}   # DMIC driver, 16kHz/16-bit mono, memory slab
    │   └── sound_level.{h,c}   # Integer RMS + dB conversion (no FPU)
    ├── ble/
    │   ├── ble_manager.{h,c}   # Peripheral advertising, connection callbacks
    │   └── config_service.{h,c} # Custom GATT service (3 characteristics)
    └── feedback/
        ├── led.{h,c}       # Onboard RGB LED patterns (GPIO)
        └── vibration.{h,c} # Coin motor PWM patterns (D0 via N-FET)
```

## Architecture

Data flow: PDM mic → DMIC driver → memory slab → monitor thread → RMS/dB → threshold check (3-block hysteresis) → LED/vibration feedback + BLE notify. Config persisted via NVS settings subsystem, writable over BLE GATT.

## Key Details

- All audio math is integer-only (nRF52840 has no FPU for floats)
- Devicetree overlay maps vibration motor to PWM1 ch0 on GPIO0 pin 2 (D0 pad)
- BLE custom service UUID base: `4f490000-2ff1-4a5e-a683-4de2c5a10100`
- Host diagnostics (clang errors about missing `zephyr/` headers) are expected — Zephyr headers live in the Docker container
