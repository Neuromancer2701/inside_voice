# InsideVoice Firmware

Zephyr RTOS firmware for the InsideVoice lapel pin — monitors voice volume via PDM microphone and provides LED + haptic feedback when a configurable loudness threshold is exceeded.

**Target:** Seeed Studio XIAO nRF52840 Sense

## Prerequisites

- Docker & Docker Compose

## Build

```bash
# First time (~15-20 min — downloads Zephyr SDK + tree)
docker compose build

# Compile firmware
docker compose run --rm firmware

# Or with explicit board/options:
docker compose run --rm firmware \
  west build -p always -b xiao_ble/nrf52840/sense .
```

Output: `build/zephyr/zephyr.uf2`

## Flash

1. Double-tap the reset button on the XIAO to enter UF2 bootloader mode.
2. A USB mass storage device appears.
3. Copy `build/zephyr/zephyr.uf2` to the drive.

Or via command line (with USB passthrough enabled in `docker-compose.yml`):

```bash
docker compose run --rm firmware \
  west flash --runner uf2
```

## BLE Interface

The device advertises as **"InsideVoice"** and exposes a custom GATT service:

| Characteristic | UUID | Properties | Description |
|---------------|------|------------|-------------|
| Threshold | `4f490001-2ff1-4a5e-a683-4de2c5a10100` | Read, Write | Loudness threshold (dB, uint8) |
| Sound Level | `4f490002-2ff1-4a5e-a683-4de2c5a10100` | Read, Notify | Current sound level (dB, uint8) |
| Feedback Mode | `4f490003-2ff1-4a5e-a683-4de2c5a10100` | Read, Write | Bitmask: bit 0 = LED, bit 1 = vibration |

## Architecture

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

### Source Modules

| Module | Purpose |
|--------|---------|
| `src/main.c` | Init all subsystems, start monitor thread |
| `src/audio/pdm_capture` | PDM mic via DMIC API, 16kHz/16-bit mono |
| `src/audio/sound_level` | Integer-only RMS + dB conversion |
| `src/feedback/led` | Onboard RGB LED patterns |
| `src/feedback/vibration` | PWM coin motor patterns |
| `src/ble/ble_manager` | BLE peripheral advertising + connection mgmt |
| `src/ble/config_service` | Custom GATT service (threshold, level, mode) |
| `src/app/config` | NVS-backed persistent settings |
| `src/app/monitor` | Core loop: audio → threshold → feedback → BLE |

## License

MIT
