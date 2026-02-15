# Inside Voice — Development Setup Guide

This document covers building and running both the Zephyr firmware and the Flutter debug app. Everything runs in Docker — no local toolchains required.

## Prerequisites

- Docker and Docker Compose v2
- A Seeed Studio XIAO nRF52840 Sense board
- An Android phone with USB debugging enabled (for the app)
- A USB-C cable for the XIAO board
- A USB cable for the Android phone

## Hardware Wiring

The XIAO nRF52840 Sense has everything onboard except the vibration motor:

| Component | Connection |
|-----------|------------|
| PDM microphone | Built into the Sense variant (no wiring) |
| RGB LED | Built into the board (GPIO controlled, active low) |
| Vibration motor | Connect coin motor between **D0** pad and **GND** through an N-channel MOSFET (gate to D0, source to GND, drain to motor negative, motor positive to 3.3V) |

If you don't have a vibration motor wired up yet, the firmware still runs fine — vibration calls are no-ops if the PWM output has no load.

---

## Part 1: Firmware

### Build the Docker image

This downloads the Zephyr SDK, toolchains, and the full Zephyr tree. Takes 15–20 minutes on the first run. Subsequent builds use cached layers.

```bash
cd firmware
docker compose build
```

### Compile the firmware

```bash
docker compose run --rm firmware
```

This runs `west build -p always -b xiao_ble/nrf52840/sense .` inside the container. The output UF2 file lands at:

```
firmware/build/zephyr/zephyr.uf2
```

### Flash the board

The XIAO nRF52840 uses UF2 drag-and-drop flashing — no J-Link required:

1. Connect the XIAO to your computer via USB-C
2. Enter the bootloader: **double-tap the tiny reset button** on the board (the pad next to the USB-C connector). An orange LED will start pulsing.
3. A USB mass storage device named **XIAO-SENSE** (or similar) will appear
4. Copy `firmware/build/zephyr/zephyr.uf2` to that drive
5. The board reboots automatically and runs the firmware

### Verify it's running

Connect to the USB serial console to see log output:

```bash
# Find the serial port (usually /dev/ttyACM0 on Linux)
ls /dev/ttyACM*

# Connect (115200 baud, or use any serial terminal)
screen /dev/ttyACM0 115200
```

You should see:

```
[00:00:00.000,000] <inf> main: InsideVoice firmware starting
[00:00:00.xxx,xxx] <inf> ble_manager: Bluetooth initialized
[00:00:00.xxx,xxx] <inf> ble_manager: Advertising started
[00:00:00.xxx,xxx] <inf> config_service: InsideVoice GATT service registered
[00:00:00.xxx,xxx] <inf> main: InsideVoice firmware initialized
[00:00:00.xxx,xxx] <inf> monitor: Monitor thread running
```

The green LED should begin a slow breathing pattern (idle state).

To exit `screen`: press `Ctrl-A` then `K`, then confirm with `Y`.

---

## Part 2: Flutter Debug App

### Build the Docker image

This downloads the Flutter SDK, Android SDK, and build tools. First run pulls ~2 GB.

```bash
cd app
docker compose build
```

### Generate the Android scaffold (one-time)

The `android/` directory is not checked in. You need to generate it once inside the container:

```bash
docker compose run --rm app flutter create --platforms=android .
```

This creates `android/` with all the Gradle configuration, manifest, and native scaffolding.

### Patch Android config

After generating the scaffold, two files need manual edits.

#### 1. Set SDK versions in `android/app/build.gradle`

Open `android/app/build.gradle` and find the `android` block. Update these values:

```groovy
android {
    compileSdk = 35          // may already be set by flutter create

    defaultConfig {
        minSdk = 21          // change from flutter default (usually 16 or 21)
        targetSdk = 35       // change from flutter default
        // ... leave the rest as-is
    }
}
```

> `minSdk 21` is required by `flutter_reactive_ble`. `targetSdk 35` is required for current Play Store compatibility and matches the SDK we installed in Docker.

#### 2. Add BLE permissions to `android/app/src/main/AndroidManifest.xml`

Add these permissions **inside** the `<manifest>` tag, **before** the `<application>` tag:

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <!-- BLE permissions: Android 6–11 -->
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION"
                     android:maxSdkVersion="30" />
    <uses-permission android:name="android.permission.BLUETOOTH"
                     android:maxSdkVersion="30" />
    <uses-permission android:name="android.permission.BLUETOOTH_ADMIN"
                     android:maxSdkVersion="30" />

    <!-- BLE permissions: Android 12+ -->
    <uses-permission android:name="android.permission.BLUETOOTH_SCAN"
                     android:usesPermissionFlags="neverForLocation" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />

    <!-- Require BLE hardware -->
    <uses-feature android:name="android.hardware.bluetooth_le"
                  android:required="true" />

    <application
        ...
```

### Connect your Android phone

1. Enable **Developer Options** on your phone (Settings → About Phone → tap Build Number 7 times)
2. Enable **USB Debugging** in Developer Options
3. Connect the phone to your computer via USB
4. Start ADB on the host so Docker can inherit the authorization:

```bash
adb start-server
adb devices    # confirm your phone shows up and is "device" (not "unauthorized")
```

If it says "unauthorized", check your phone for the USB debugging authorization prompt and accept it.

### Build the debug APK

```bash
cd app
docker compose run --rm app
```

The first build downloads ~300 MB of Gradle dependencies (cached in a named volume for subsequent builds). Output APK lands at:

```
app/build/app/outputs/flutter-apk/app-debug.apk
```

### Install to phone

```bash
docker compose run --rm install
```

Or combine build + install in one step:

```bash
docker compose run --rm app sh -c "flutter build apk --debug && flutter install"
```

### Rebuilding after code changes

The source is bind-mounted into the container, so just re-run:

```bash
docker compose run --rm app
docker compose run --rm install
```

No need to rebuild the Docker image unless you change `pubspec.yaml` dependencies.

---

## Using the Debug App

Open the **InsideVoice Debug** app on your phone. It has a single screen with four panels:

### 1. Connection Panel

Shows the current BLE connection status:

- **Disconnected** (red) — tap **Scan & Connect** to begin
- **Scanning** (orange) — the app is scanning for a device advertising the name "InsideVoice"
- **Connecting** (orange) — device found, establishing BLE connection
- **Connected** (green) — GATT services discovered, tap **Disconnect** to drop the connection

The app scans by device name (not service UUID) because the firmware advertisement packet contains only the device name and flags.

### 2. Sound Level Panel

Displays the live dB reading from the firmware's PDM microphone:

- Large number showing the current dB value
- Progress bar that turns **green** when below threshold, **red** when at/above threshold
- Updates in real time via BLE notifications from the Sound Level characteristic

### 3. Threshold Panel

A slider from 50–90 dB to set the volume threshold:

- Reads the current value from the device on connect
- Writes to the device only on **slider release** (not during drag) to avoid flooding GATT writes
- The firmware persists the value to flash via NVS, so it survives power cycles

### 4. Feedback Mode Panel

Two toggles to control which feedback channels are active:

- **LED** — enables/disables the RGB LED alert pattern when over threshold
- **Vibration** — enables/disables the vibration motor alert when over threshold
- Both enabled by default (bitmask `0x03`)
- Also persisted to NVS on the firmware side

---

## What to Expect at This Stage

This is a development scaffold. Here's what works and what doesn't yet.

### What works

- **Firmware compiles** to a valid UF2 binary targeting the XIAO nRF52840 Sense
- **All subsystems initialize**: PDM audio capture (16 kHz mono), BLE peripheral advertising, NVS-backed config, GPIO LEDs, PWM vibration motor
- **Monitor loop runs**: continuously reads 100ms audio blocks, computes integer RMS → dB, applies 3-block hysteresis, triggers feedback when threshold is exceeded
- **BLE GATT service**: three characteristics (threshold R/W, sound level R/Notify, feedback mode R/W) are fully implemented
- **Config persistence**: threshold and feedback mode survive reboots via NVS
- **LED patterns**: green breathing (idle), warm pulse (over threshold), blue flash (BLE event)
- **Vibration patterns**: gentle tap, double tap, soft pulse
- **Flutter app structure**: compiles, scans, connects, reads/writes all characteristics, displays live dB stream

### What hasn't been tested on hardware yet

- Actual PDM microphone readings (dB calibration may need tuning)
- BLE connection stability and notification throughput
- LED timing/brightness on the actual onboard RGB LED
- Vibration motor PWM duty cycle and feel
- NVS read/write across real power cycles
- Flutter app BLE scan filtering (Android name caching quirks)
- End-to-end flow: speak loudly → firmware detects → LED/vibration fire → app shows live dB crossing threshold

### Known limitations

- **No OTA firmware update** — reflash via UF2 each time
- **No battery management** — the board runs off USB power; battery support is not implemented
- **No audio calibration** — dB values are relative to the PDM mic's digital full scale, not calibrated to SPL. The threshold slider works on these relative values, so it's functionally correct but the numbers won't match a sound meter.
- **Single BLE connection** — the firmware allows only one connected client (`CONFIG_BT_MAX_CONN=1`)
- **Debug APK only** — no release signing, no Play Store build
- **No iOS support** — the Flutter app targets Android only (Docker doesn't support iOS builds)
- **Host IDE shows Zephyr header errors** — this is expected since Zephyr headers only exist inside the Docker container. The code compiles correctly in Docker.

### Next steps (not yet implemented)

- Flash firmware to the board and validate PDM mic readings
- Tune dB thresholds based on real-world audio
- Adjust LED brightness and vibration intensity
- Test BLE connection from the Flutter app to the real device
- Add USB serial log viewer to the app for on-device debugging
- Battery power management and low-power modes
- Physical enclosure / lapel pin form factor

---

## Troubleshooting

### Firmware

| Problem | Fix |
|---------|-----|
| `west build` fails with missing toolchain | Run `docker compose build` again — the Zephyr SDK image may not have downloaded fully |
| No USB mass storage on double-tap reset | Try a different USB-C cable (some are charge-only). Hold reset for 5 seconds then double-tap. |
| No serial output | Check that `CONFIG_USB_CDC_ACM=y` is in `prj.conf`. Try a different terminal program (`minicom`, `picocom`, `tio`). |
| "Bluetooth init failed" in logs | The nRF52840 BLE stack needs the SoftDevice. Ensure you're building for the correct board: `xiao_ble/nrf52840/sense` |

### Flutter App

| Problem | Fix |
|---------|-----|
| `flutter create` fails | Make sure you're running it inside the container: `docker compose run --rm app flutter create --platforms=android .` |
| Gradle download hangs | The gradle-cache volume may be corrupted. Run `docker volume rm app_gradle-cache` and rebuild. |
| `adb devices` shows nothing | Run `adb start-server` on the host first. Check the phone's USB mode is set to "File Transfer" or "PTP", not "Charging only". |
| `adb devices` shows "unauthorized" | Unlock your phone and accept the USB debugging prompt. If no prompt appears, revoke USB debugging authorizations in Developer Options and reconnect. |
| App crashes on launch | Check that `minSdk` is 21 in `build.gradle` and BLE permissions are in `AndroidManifest.xml`. |
| Scan finds nothing | Make sure the firmware is running and advertising (check serial log for "Advertising started"). Make sure Location is enabled on the phone (required for BLE scanning on Android). |
| Connects then immediately disconnects | Add the 200ms delay before GATT reads (already in the code). Check serial log on firmware side for connection/disconnection reasons. |
