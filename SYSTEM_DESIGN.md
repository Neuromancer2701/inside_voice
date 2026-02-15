# Inside Voice — System Design

Complete system architecture for the InsideVoice wearable: firmware, mobile app, and cloud backend.

## System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Cloud (Firebase)                            │
│  ┌──────────┐ ┌──────────────┐ ┌─────────┐ ┌─────────┐ ┌────────┐ │
│  │   Auth   │ │  Firestore   │ │ Storage │ │  FCM    │ │Analytics│ │
│  │(accounts)│ │(settings,    │ │(OTA     │ │(push    │ │(usage  )│ │
│  │          │ │ history)     │ │ binaries)│ │ notify) │ │         │ │
│  └────┬─────┘ └──────┬───────┘ └────┬────┘ └────┬────┘ └────┬───┘ │
└───────┼──────────────┼──────────────┼───────────┼───────────┼──────┘
        │              │              │           │           │
        └──────────────┴──────┬───────┴───────────┴───────────┘
                              │ HTTPS / gRPC
                     ┌────────┴────────┐
                     │  Flutter App    │
                     │  (iOS/Android)  │
                     └────────┬────────┘
                              │ BLE (GATT + DFU)
                     ┌────────┴────────┐
                     │  Firmware       │
                     │  (nRF52840)     │
                     └─────────────────┘
```

---

## Phase 1: Firmware (Zephyr RTOS)

**Status:** Scaffolded

### Hardware

- **MCU:** Seeed Studio XIAO nRF52840 Sense
- **Mic:** Onboard PDM (MP34DT06JTR)
- **LED:** Onboard RGB (3× GPIO, active-low)
- **Haptic:** External coin vibration motor on D0 pad via N-FET + PWM

### Toolchain

- Zephyr v3.7 LTS (upstream, not nRF Connect SDK)
- Dockerized build: `ghcr.io/zephyrproject-rtos/zephyr-build:v0.26-branch`
- Board target: `xiao_ble/nrf52840/sense`

### Firmware Architecture

```
PDM Mic → DMIC Driver → Memory Slab → Monitor Thread → RMS/dB calc
                                            ↓
                                    Threshold comparison (3-block hysteresis)
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
| `src/audio/pdm_capture.{h,c}` | DMIC driver, 16kHz/16-bit mono, 4-block memory slab |
| `src/audio/sound_level.{h,c}` | Integer-only RMS + dB conversion (no FPU) |
| `src/feedback/led.{h,c}` | Onboard RGB LED patterns via `k_work_delayable` |
| `src/feedback/vibration.{h,c}` | PWM coin motor patterns (D0 via N-FET) |
| `src/ble/ble_manager.{h,c}` | Peripheral advertising, connection callbacks |
| `src/ble/config_service.{h,c}` | Custom GATT service (3 characteristics) |
| `src/app/config.{h,c}` | NVS-backed persistent settings |
| `src/app/monitor.{h,c}` | Core loop: audio → threshold → feedback → BLE |

### BLE GATT Service

Service UUID: `4f490000-2ff1-4a5e-a683-4de2c5a10100`

| Characteristic | UUID suffix | Properties | Type | Description |
|---------------|-------------|------------|------|-------------|
| Threshold | `0001` | Read, Write | uint8 | Loudness threshold in dB |
| Sound Level | `0002` | Read, Notify | uint8 | Current sound level in dB |
| Feedback Mode | `0003` | Read, Write | uint8 | Bitmask: bit 0 = LED, bit 1 = vibration |

### OTA / MCUboot

The firmware will use MCUboot as a secure bootloader to enable BLE DFU updates pushed from the Flutter app.

- **Bootloader:** MCUboot (Zephyr-integrated, `CONFIG_BOOTLOADER_MCUBOOT=y`)
- **Image slots:** Two-slot (primary + secondary) in internal flash
- **DFU transport:** Zephyr SMP (Simple Management Protocol) over BLE
- **Image signing:** Ed25519 key pair; public key baked into bootloader, private key used at build time
- **Rollback:** MCUboot confirms the new image on first successful boot; reverts to previous slot on failure

Additional Kconfig for MCUboot support:
```
CONFIG_BOOTLOADER_MCUBOOT=y
CONFIG_MCUBOOT_IMG_MANAGER=y
CONFIG_IMG_MANAGER=y
CONFIG_MCUMGR=y
CONFIG_MCUMGR_TRANSPORT_BT=y
CONFIG_MCUMGR_GRP_IMG=y
CONFIG_MCUMGR_GRP_OS=y
```

### Memory Budget

| Component | Estimate |
|-----------|----------|
| Audio slab (4 × 3200 B) | 12.8 KB |
| BLE stack | ~15 KB |
| MCUboot overhead | ~16 KB |
| Threads + heap | ~8 KB |
| **Total** | **~52 KB / 256 KB RAM** |

### Build & Flash

```bash
cd firmware
docker compose build
docker compose run --rm firmware
# Output: build/zephyr/zephyr.uf2

# Initial flash: USB bootloader
# Subsequent: BLE DFU via app
```

---

## Phase 2: Flutter Mobile App

### Overview

Cross-platform (iOS/Android) companion app for device configuration, real-time monitoring, session history, and OTA firmware updates.

### Tech Stack

- **Framework:** Flutter (Dart)
- **BLE:** `flutter_reactive_ble` — mature, well-maintained reactive BLE library
- **DFU:** `mcumgr_flutter` — SMP/MCUmgr client for BLE DFU with MCUboot
- **State management:** Riverpod — compile-safe, testable, good for async BLE streams
- **Local storage:** Drift (SQLite) — structured session history with queries
- **Backend SDK:** FlutterFire (Firebase Auth, Firestore, Storage, FCM, Analytics)
- **Charts:** `fl_chart` — lightweight, customizable charts for history/trends

### App Screens

```
Onboarding Flow ──→ Home (Dashboard)
                         │
              ┌──────────┼──────────────┐
              ↓          ↓              ↓
         Live Meter   Sessions     Settings
              │          │              │
              │     Session Detail   ┌──┴──┐
              │                  Profiles  Device
              │                            │
              │                        OTA Update
              │
         (overlays threshold alerts)
```

#### 1. Onboarding (first launch)
- Account creation (Firebase Auth — email/password or Google/Apple sign-in)
- BLE pairing walkthrough with device animation
- Initial threshold calibration: "Speak normally" → auto-detect baseline → set threshold 5–10 dB above
- Select default profile (meeting, restaurant, library)

#### 2. Home / Dashboard
- Real-time sound level gauge (animated arc/dial) from BLE notify stream
- Current threshold indicator on gauge
- Active profile name + quick-switch chips
- Connection status badge (connected / scanning / disconnected)
- Session timer (auto-starts when connected, configurable)

#### 3. Live Meter
- Full-screen real-time dB visualization
- Scrolling waveform or bar chart (last 30 seconds)
- Over-threshold event markers
- Mute feedback button (temporary disable without changing config)

#### 4. Sessions / History
- List of past monitoring sessions (date, duration, avg dB, peak dB, over-threshold count)
- Tapping a session shows detail view:
  - dB over time chart
  - Threshold exceedance timeline
  - Stats summary (min/avg/peak/% time over threshold)
- Filter by date range, profile
- Cloud-synced across devices via Firestore

#### 5. Settings
- **Profiles** (presets):
  - Each profile stores: name, threshold dB, feedback mode bitmask, LED pattern, vibration pattern
  - Built-in defaults: Meeting (65 dB), Restaurant (75 dB), Library (55 dB), Custom
  - User can create/edit/delete custom profiles
  - Switching profile writes new config to device via BLE GATT
- **Device:**
  - Firmware version display
  - OTA update check + install (downloads from Firebase Storage → pushes via SMP/BLE DFU)
  - Battery level (future: add BLE Battery Service to firmware)
  - Factory reset (write defaults to all GATT characteristics)
- **Account:** Sign out, delete account, export data (CSV)
- **Notifications:** Toggle push notifications for firmware updates

### App Architecture

```
┌──────────────────────────────────────────────────────┐
│                    Presentation                       │
│  Screens / Widgets ← Riverpod Providers (ViewModel)  │
├──────────────────────────────────────────────────────┤
│                    Domain                             │
│  Use Cases: MonitorSession, ManageProfiles,           │
│             SyncSettings, PerformDFU                  │
├──────────────────────────────────────────────────────┤
│                    Data                               │
│  ┌─────────────┐  ┌──────────┐  ┌──────────────────┐ │
│  │ BLE Service │  │ Firebase │  │ Local DB (Drift) │ │
│  │ (reactive_  │  │ (Auth,   │  │ (sessions,       │ │
│  │  ble)       │  │ Firestore│  │  cached profiles)│ │
│  └──────┬──────┘  │ Storage, │  └────────┬─────────┘ │
│         │         │ FCM)     │           │           │
│         │         └─────┬────┘           │           │
└─────────┼───────────────┼───────────────┼───────────┘
          │ BLE           │ HTTPS          │ SQLite
     ┌────┴────┐    ┌─────┴─────┐    local filesystem
     │ Device  │    │  Firebase │
     └─────────┘    │  Cloud    │
                    └───────────┘
```

### Project Structure

```
app/
├── lib/
│   ├── main.dart
│   ├── app.dart                    # MaterialApp, routing, theme
│   ├── core/
│   │   ├── ble/
│   │   │   ├── ble_service.dart         # BLE scan, connect, disconnect
│   │   │   ├── gatt_client.dart         # Read/write/subscribe GATT chars
│   │   │   └── dfu_service.dart         # MCUmgr DFU orchestration
│   │   ├── firebase/
│   │   │   ├── auth_service.dart        # Firebase Auth wrapper
│   │   │   ├── firestore_service.dart   # Cloud sync for settings/history
│   │   │   ├── storage_service.dart     # OTA binary downloads
│   │   │   └── fcm_service.dart         # Push notification handling
│   │   └── database/
│   │       ├── app_database.dart        # Drift database definition
│   │       └── tables.dart              # Sessions, profiles tables
│   ├── features/
│   │   ├── onboarding/
│   │   │   ├── onboarding_screen.dart
│   │   │   └── calibration_screen.dart
│   │   ├── home/
│   │   │   ├── home_screen.dart
│   │   │   └── providers.dart
│   │   ├── meter/
│   │   │   ├── live_meter_screen.dart
│   │   │   └── providers.dart
│   │   ├── sessions/
│   │   │   ├── sessions_list_screen.dart
│   │   │   ├── session_detail_screen.dart
│   │   │   └── providers.dart
│   │   ├── settings/
│   │   │   ├── settings_screen.dart
│   │   │   ├── profiles_screen.dart
│   │   │   ├── device_screen.dart
│   │   │   └── providers.dart
│   │   └── dfu/
│   │       ├── dfu_screen.dart
│   │       └── providers.dart
│   ├── models/
│   │   ├── device_config.dart           # threshold, feedback_mode
│   │   ├── profile.dart                 # name, threshold, mode
│   │   ├── session.dart                 # start, end, samples, stats
│   │   └── firmware_info.dart           # version, update availability
│   └── shared/
│       ├── theme.dart
│       ├── widgets/                     # Reusable components
│       └── constants.dart               # UUIDs, defaults
├── test/
├── pubspec.yaml
└── firebase.json
```

### BLE Data Flow (App Side)

```
flutter_reactive_ble
    │
    ├── scanForDevices(name: "InsideVoice")
    │       → DeviceDiscovered stream
    │
    ├── connectToDevice(deviceId)
    │       → ConnectionState stream
    │
    ├── subscribeToCharacteristic(soundLevel: 0x0002)
    │       → Stream<uint8> dB values (every 100ms)
    │       → Feed to live meter UI + session recorder
    │
    ├── readCharacteristic(threshold: 0x0001)
    │       → Current device threshold
    │
    ├── writeCharacteristic(threshold: 0x0001, value)
    │       → Push new threshold from profile switch
    │
    └── writeCharacteristic(feedbackMode: 0x0003, value)
            → Push feedback mode bitmask
```

### OTA Update Flow

```
1. App checks Firebase Storage for latest firmware manifest
   (version, binary URL, checksum, release notes)

2. If newer than device version → show update badge in Settings

3. User taps "Install Update":
   a. Download signed binary from Firebase Storage
   b. Verify SHA-256 checksum
   c. Switch to DFU mode via mcumgr_flutter
   d. Stream binary to device over BLE SMP
   e. Device reboots into MCUboot → validates signature → swaps slots
   f. App reconnects, reads new version, confirms success

4. On failure: MCUboot reverts to previous slot automatically
```

---

## Phase 3: Cloud Backend (Firebase)

### Services Used

| Firebase Service | Purpose | Billing Tier |
|-----------------|---------|-------------|
| **Authentication** | User accounts (email, Google, Apple sign-in) | Free (unlimited) |
| **Cloud Firestore** | Settings sync, session history, profiles | Free up to 1 GB |
| **Cloud Storage** | OTA firmware binaries + manifests | Free up to 5 GB |
| **Cloud Functions** | Firmware upload processing, push triggers | Free up to 2M invocations/mo |
| **FCM** | Push notifications for firmware updates | Free (unlimited) |
| **Google Analytics** | Usage analytics, session tracking | Free |
| **Crashlytics** | App crash reporting | Free |

### Firestore Data Model

```
users/{uid}/
├── profile                          # User account doc
│   ├── displayName: string
│   ├── email: string
│   ├── createdAt: timestamp
│   └── lastSyncAt: timestamp
│
├── devices/{deviceId}/              # Paired devices
│   ├── name: string
│   ├── firmwareVersion: string
│   ├── lastConnected: timestamp
│   └── macAddress: string
│
├── profiles/{profileId}/            # Sound profiles (presets)
│   ├── name: string                 # "Meeting", "Restaurant", etc.
│   ├── thresholdDb: number          # 55–90
│   ├── feedbackMode: number         # Bitmask: LED | vibration
│   ├── isDefault: boolean
│   ├── createdAt: timestamp
│   └── updatedAt: timestamp
│
└── sessions/{sessionId}/            # Monitoring sessions
    ├── startedAt: timestamp
    ├── endedAt: timestamp
    ├── profileId: string
    ├── deviceId: string
    ├── durationSec: number
    ├── avgDb: number
    ├── peakDb: number
    ├── overThresholdCount: number
    ├── overThresholdPct: number     # % time over threshold
    └── samples: subcollection       # (optional, for detail view)
        └── {sampleId}/
            ├── timestamp: timestamp
            ├── db: number
            └── overThreshold: boolean
```

### Cloud Storage Structure

```
firmware/
├── manifests/
│   └── latest.json                  # Current release manifest
│       {
│         "version": "1.2.0",
│         "binaryPath": "firmware/binaries/inside_voice_1.2.0.bin",
│         "checksum": "sha256:abc123...",
│         "releaseNotes": "Improved threshold accuracy",
│         "minAppVersion": "1.0.0",
│         "createdAt": "2026-02-14T..."
│       }
└── binaries/
    ├── inside_voice_1.0.0.bin
    ├── inside_voice_1.1.0.bin
    └── inside_voice_1.2.0.bin
```

### Cloud Functions

| Function | Trigger | Purpose |
|----------|---------|---------|
| `onFirmwareUpload` | Storage upload to `firmware/binaries/` | Validate binary, compute checksum, update `latest.json` manifest, trigger push notification |
| `notifyFirmwareUpdate` | Firestore write to manifest | Send FCM push to all users with paired devices |
| `cleanupOldSessions` | Scheduled (weekly) | Delete session sample data older than 90 days to manage storage |
| `exportUserData` | HTTPS callable | Generate CSV export of user's session history |

### Security Rules (Firestore)

```
rules_version = '2';
service cloud.firestore {
  match /databases/{database}/documents {
    // Users can only access their own data
    match /users/{uid}/{document=**} {
      allow read, write: if request.auth != null && request.auth.uid == uid;
    }

    // Firmware manifests are readable by any authenticated user
    match /firmware/{document=**} {
      allow read: if request.auth != null;
      allow write: if false;  // Only Cloud Functions can write
    }
  }
}
```

### Analytics Events

| Event | Params | Purpose |
|-------|--------|---------|
| `session_start` | profile, threshold | Track usage patterns |
| `session_end` | duration, avg_db, peak_db, over_count | Session quality metrics |
| `threshold_exceeded` | db_value, profile | Frequency of feedback triggers |
| `profile_switched` | from, to | Profile popularity |
| `dfu_started` | from_version, to_version | OTA adoption tracking |
| `dfu_completed` | success, duration | OTA reliability |
| `device_paired` | firmware_version | Device fleet tracking |

---

## Phase Summary & Implementation Order

### Phase 1: Firmware — DONE
- [x] Docker dev environment
- [x] Zephyr project scaffold (CMake, Kconfig, devicetree overlay)
- [x] All source modules (audio, feedback, BLE, config, monitor)
- [ ] MCUboot integration + SMP transport
- [ ] BLE DFU testing

### Phase 2: Flutter App
1. Project scaffold (Flutter create, dependencies, Firebase init)
2. Core BLE service (scan, connect, GATT read/write/subscribe)
3. Home screen with live dB gauge
4. Settings screen with threshold/mode controls
5. Profiles (CRUD, sync to device on switch)
6. Onboarding flow with calibration
7. Session recording + local Drift database
8. Session history list + detail charts
9. Cloud sync (Firestore read/write for profiles + sessions)
10. OTA DFU screen (mcumgr_flutter integration)
11. Push notifications (FCM for firmware updates)
12. Polish: theming, animations, error handling, offline mode

### Phase 3: Cloud Backend
1. Firebase project setup (Auth, Firestore, Storage, Functions, FCM)
2. Firestore security rules + data model
3. Cloud Function: firmware upload processing + manifest
4. Cloud Function: push notification on new firmware
5. Cloud Function: session data cleanup
6. Cloud Function: CSV export
7. Analytics event definitions
8. Crashlytics integration

### Future Considerations
- **Battery service:** Add standard BLE Battery Service (0x180F) to firmware for battery level reporting in app
- **Watch companion:** WearOS/watchOS widget showing current dB level
- **Multi-device:** Support pairing multiple InsideVoice devices per account
- **Social/sharing:** Share session summaries or progress with a coach/therapist
- **ML-based threshold:** Auto-adjust threshold based on ambient noise classification
- **Accessibility:** VoiceOver/TalkBack support, high-contrast mode, haptic-only feedback option

---

## Repository Structure (Target)

```
inside_voice/
├── SYSTEM_DESIGN.md          # This document
├── CLAUDE.md                 # Claude Code project guidance
├── README.md                 # Project overview
├── LICENSE                   # MIT
├── .gitignore
│
├── firmware/                 # Zephyr RTOS (C)
│   ├── Dockerfile
│   ├── docker-compose.yml
│   ├── CMakeLists.txt
│   ├── prj.conf
│   ├── boards/
│   └── src/
│
├── app/                      # Flutter (Dart)
│   ├── lib/
│   ├── test/
│   ├── pubspec.yaml
│   ├── firebase.json
│   ├── android/
│   └── ios/
│
└── backend/                  # Firebase (Cloud Functions, rules)
    ├── functions/
    │   ├── src/
    │   └── package.json
    ├── firestore.rules
    ├── storage.rules
    └── firebase.json
```
