# Universal IR Remote via ESP32

An ESP32-based universal infrared remote controller with an MIT App Inventor Android companion app. The ESP32 hosts a small HTTP server, learns raw IR signals from any remote, stores them persistently on flash, and replays them on command — letting you control TVs, ACs, and any other IR-based device from a single phone app.

## Overview

The ESP32 acts as a WiFi-connected IR hub:

- An **IR receiver** learns signals from your existing remotes and captures them as **raw pulse timings** (not decoded protocol data). This makes it brand-agnostic — it works even with AC remotes (Midea, Fresh, Tornado, etc.) whose protocols aren't recognized by standard IR libraries and decode as `UNKNOWN`.
- An **IR transmitter (LED)** replays the learned signal on demand.
- Devices and their buttons/presets are stored as JSON on the ESP32's internal flash (LittleFS), so everything survives a reboot.
- A lightweight **HTTP API** exposes endpoints to add devices, learn signals, send signals, and manage buttons — driven by the companion Android app.

## Features

- **Device types**: TV, AC, and a fully user-defined "Others" category for anything else (soundbars, fans, projectors, etc.)
- **Signal learning ("remap" mode)**: point any remote at the receiver and the button press is captured and saved
- **Raw signal capture**: uses `sendRaw`/`rawbuf` pulse timing instead of decoded hex, so unsupported/proprietary AC protocols still work
- **AC preset system**: since full AC protocol decoding is complex, ACs are controlled via 9 saved presets + OFF, each capturing Temp/Speed/Mode/Swing state
- **Custom buttons**: "Others" devices support arbitrary, user-named, extensible buttons
- **Persistent storage**: devices/buttons/presets are saved as JSON on LittleFS
- **WiFi status LED**: onboard indicator for connection state
- **Android companion app**: built with MIT App Inventor, communicates with the ESP32 over local WiFi via HTTP

## Hardware

| Component | Notes |
|---|---|
| ESP32 dev board | Main controller |
| IR receiver (e.g. TSOP38238) | Connected to `GPIO 26` |
| IR LED (+ transistor driver) | Connected to `GPIO 16` |
| Status LED | Connected to `GPIO 15` (WiFi connection indicator) |

> **Note:** If you see noisy/garbage IR captures, add a decoupling capacitor (e.g. 10–100µF) across the TSOP receiver's power pins — WiFi activity on the ESP32 can inject noise into the receiver's supply line.

## Firmware Stack

- **Framework**: Arduino (via PlatformIO)
- **Board**: `esp32dev`
- **Filesystem**: LittleFS
- **Libraries**:
  - [`IRremote`](https://github.com/Arduino-IRremote/Arduino-IRremote) `^4.7.1` — IR signal capture/transmission
  - [`ArduinoJson`](https://github.com/bblanchon/ArduinoJson) `^7.2.2` — JSON schema persistence

## JSON Data Schema

Devices are stored under `/Devices/devices.json`:

```json
{
  "Devices": [
    {
      "Name": "Living Room TV",
      "Type": "TV",
      "Buttons": [
        { "Name": "Power", "Assigned": true, "RawCode": [9000, 4500, ...] }
      ]
    },
    {
      "Name": "Bedroom AC",
      "Type": "AC",
      "Presets": [
        {
          "Name": "Preset1",
          "Assigned": true,
          "RawCode": [...],
          "Temp": "24",
          "Speed": "Auto",
          "Mode": "Cool",
          "HorSwing": "On",
          "VerSwing": "Off"
        }
      ]
    },
    {
      "Name": "Soundbar",
      "Type": "Others",
      "Buttons": [
        { "Name": "Bass Boost", "Assigned": true, "RawCode": [...] }
      ]
    }
  ]
}
```

- **TV / Others** devices store a `Buttons` array.
- **AC** devices store a `Presets` array (9 presets + `OFF`), since full protocol-level AC control is impractical within the project scope — each preset instead captures a specific combination of Temp/Speed/Mode/Swing as a raw signal.

## HTTP API

All endpoints are served on port `80`.

| Endpoint | Query Params | Description |
|---|---|---|
| `GET /getDevices` | — | Returns the full devices JSON document |
| `GET /getButtons` | `Name`, `Type` | Returns the `Buttons` (TV/Others) or `Presets` (AC) array for a device |
| `GET /addDevice` | `Name`, `Type` | Creates a new device with a pre-populated button/preset template |
| `GET /deleteDevice` | `Name`, `Type` | Removes a device |
| `GET /addCustomButton` | `Name`, `Type`, `buttonName`, `whichCustom` | Adds a user-defined button to an "Others" device |
| `GET /learnSignal` | `Name`, `Type`, `buttonName` or `Preset` (+ `Temp`/`Speed`/`Mode`/`HorSwing`/`VerSwing` for AC) | Blocks until an IR signal is received, then saves it to the selected button/preset |
| `GET /sendSignal` | `Name`, `Type`, `buttonName` or `Preset` | Transmits the previously learned raw IR signal |

## Getting Started

### Firmware

1. Install [PlatformIO](https://platformio.org/) (CLI or VS Code extension).
2. Update the WiFi credentials in `src/main.cpp`:
   ```cpp
   char *ssid = "YOUR_SSID";
   char *password = "YOUR_PASSWORD";
   ```
3. Build and upload:
   ```bash
   pio run --target upload
   ```
4. Upload the LittleFS filesystem (creates `/Devices/devices.json` automatically on first boot if missing, but you can also pre-provision it):
   ```bash
   pio run --target uploadfs
   ```
5. Open the serial monitor at `115200` baud to see the ESP32's IP address once connected to WiFi.

### Android App

The MIT App Inventor project is included as `ESP32_IRremote_10_restyled(1).aia`.

1. Go to [App Inventor](https://appinventor.mit.edu/).
2. Import the `.aia` file as a new project.
3. Set the ESP32's IP address in the app so it can reach the HTTP API on your local network.
4. Build/install the APK to your phone, and make sure your phone is on the same WiFi network as the ESP32.

## Project Structure

```
.
├── src/
│   └── main.cpp                        # ESP32 firmware (WiFi, HTTP server, IR, JSON persistence)
├── ESP32_IRremote_10_restyled(1).aia   # MIT App Inventor Android app project
├── platformio.ini                      # PlatformIO project/build configuration
├── include/                            # Project headers (empty/placeholder)
├── lib/                                # Project-specific libraries (empty/placeholder)
└── test/                               # PlatformIO unit tests (empty/placeholder)
```

## Roadmap / Known Limitations

- WiFi credentials are currently hardcoded in firmware (no WiFiManager/captive portal yet).
- AC control relies on presets rather than full protocol decoding, due to time constraints.
- No authentication on the HTTP API — intended for trusted local networks only.

## License

No license specified yet — all rights reserved by default until one is added.
