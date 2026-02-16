# Inside Voice

A lapel pin wearable based on the Seeed Studio XIAO nRF52840 Sense that monitors the wearer's voice volume and provides gentle LED + haptic feedback when they exceed a configurable loudness threshold.

## Design Decisions

- **Toolchain:** Zephyr RTOS (upstream, not nRF Connect SDK — the board is fully supported in mainline Zephyr)
- **Feedback:** Onboard RGB LED + NFP-C1034 coin vibration motor on D0 pad via IRLML6344 N-FET + PWM
- **Battery:** 3.7V 250 mAh LiPo (302530), charged via onboard BQ25101 from USB-C (~25 hrs runtime, ~5 hrs overnight charge)
- **Future app:** Flutter (not built now)
- **Docker base:** `ghcr.io/zephyrproject-rtos/zephyr-build` (Zephyr SDK + ARM GCC pre-installed)

## Phase 1: Docker Dev Environment

### `firmware/Dockerfile`
- Base: `ghcr.io/zephyrproject-rtos/zephyr-build:v0.26-branch` (includes Zephyr SDK + ARM GCC)
- `west init` + `west update --narrow -o=--depth=1` for Zephyr v3.7 LTS workspace
- Install Python requirements for Zephyr
- Project source mounted at `/workdir/project`

### `firmware/docker-compose.yml`
- Bind-mount `firmware/` → `/workdir/project` for live host editing
- Named volume `zephyr-workspace` persists the ~4GB Zephyr tree between container recreations
- Pre-set `BOARD=xiao_ble/nrf52840/sense` env var
- USB passthrough commented out but ready for flashing

### `firmware/.dockerignore`
- Exclude `build/`, object files, binaries

## Phase 2: Firmware Scaffold

### Build system files
- **`firmware/CMakeLists.txt`** — Zephyr CMake project targeting all source files
- **`firmware/prj.conf`** — Kconfig enabling: BT peripheral, DMIC/audio, PWM, GPIO, LED, NVS settings, USB CDC ACM console, logging
- **`firmware/boards/xiao_ble_nrf52840_sense.overlay`** — Devicetree overlay: PDM mic enable, vibration motor on PWM1 ch0 → D0 (GPIO0 pin 2), LED aliases

### Source modules (`firmware/src/`)

| Module | Files | Purpose |
|--------|-------|---------|
| `main.c` | Entry point | Init all subsystems in order, then exit (work done in threads) |
| `audio/pdm_capture.{h,c}` | PDM mic | DMIC driver API, 16kHz/16-bit mono, memory slab buffering (4 blocks × 1600 samples) |
| `audio/sound_level.{h,c}` | Level calc | Integer-only RMS + approximate dB conversion (no FPU dependency) |
| `feedback/led.{h,c}` | RGB LED | GPIO-based on/off with async pattern work items (pulse warm, breathe green, flash blue) |
| `feedback/vibration.{h,c}` | Haptic motor | PWM intensity control with patterns (gentle tap, double tap, soft pulse) |
| `ble/ble_manager.{h,c}` | BLE stack | Peripheral advertising as "InsideVoice", connection callbacks, auto-restart advertising on disconnect |
| `ble/config_service.{h,c}` | GATT service | Custom 128-bit UUID service with 3 characteristics: threshold (R/W), sound level (R/Notify), feedback mode (R/W bitmask) |
| `app/config.{h,c}` | Settings | NVS-backed persistent config (threshold dB, feedback mode bitmask), Zephyr settings subsystem |
| `app/monitor.{h,c}` | Core loop | Dedicated thread: read audio → compute RMS/dB → compare threshold with hysteresis (3 consecutive blocks) → trigger/stop feedback → BLE notify |

### Architecture / Data flow
```
PDM Mic → DMIC Driver → Memory Slab → Monitor Thread → RMS/dB calc
                                            ↓
                                    Threshold comparison (with hysteresis)
                                       ↙              ↘
                              LED patterns          Vibration patterns
                                       ↘              ↙
                                    BLE GATT notify (sound level)
                                            ↑
                              Config Service ← App (threshold, mode)
                                            ↓
                                    NVS persistent storage
```

## Phase 3: Supporting files

- **`firmware/README.md`** — Build/flash/BLE interface docs
- **`.gitignore`** — Includes `firmware/build/`, `firmware/.west/`
- **`CLAUDE.md`** — Project structure, build commands, architecture

## Quick Start

```bash
cd firmware
docker compose build                    # ~15-20 min first time
docker compose run --rm firmware \
  west build -p always -b xiao_ble/nrf52840/sense .
# Success: build/zephyr/zephyr.uf2 exists
```

## Flashing

1. Double-tap the reset button on the XIAO to enter UF2 bootloader mode.
2. Copy `firmware/build/zephyr/zephyr.uf2` to the USB mass storage device that appears.

## BLE Interface

The device advertises as **"InsideVoice"** and exposes a custom GATT service:

| Characteristic | UUID | Properties | Description |
|---------------|------|------------|-------------|
| Threshold | `4f490001-2ff1-4a5e-a683-4de2c5a10100` | Read, Write | Loudness threshold (dB, uint8) |
| Sound Level | `4f490002-2ff1-4a5e-a683-4de2c5a10100` | Read, Notify | Current sound level (dB, uint8) |
| Feedback Mode | `4f490003-2ff1-4a5e-a683-4de2c5a10100` | Read, Write | Bitmask: bit 0 = LED, bit 1 = vibration |

## Risks & Notes

- **Devicetree overlay**: Pin assignments and node names must match the upstream board DTS exactly. If PWM1 pinctrl or vibration motor node fails, fall back to GPIO toggling via timer.
- **DMIC API surface**: The Zephyr DMIC API may have subtle differences across versions. The `dmic_dev` nodelabel must match what `pdm0` is aliased to in the overlay.
- **Memory budget**: Audio slab (12.8 KB) + BLE stack (~15 KB) + threads is well within 256 KB but should be monitored with `CONFIG_THREAD_ANALYZER`.
- **Docker build time**: First build downloads ~5 GB total. Named volume mitigates subsequent rebuilds.

## License

MIT
