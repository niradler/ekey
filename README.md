# EKey

**Universal Keyboard Bridge** for ESP32-S3

*Pronounced "hickey" - E=ESP32-S3, Key=Keyboard*

## What is EKey?

EKey is a universal keyboard bridge that routes keyboard input from different sources to different outputs. Connect any input to any output - USB keyboards, web interfaces, Bluetooth, or USB HID.

## Modes

| Mode | Input | Output | Status |
|------|-------|--------|--------|
| **OTG** | USB Keyboard | Bluetooth LE | ✅ Ready |
| **Web** | Virtual Keyboard (WiFi) | Bluetooth LE | ✅ Ready |
| **USB** | Virtual Keyboard (WiFi) | USB HID | 🔜 Coming |

### OTG Mode (USB → BLE)
Connect any USB keyboard to ESP32-S3. Keystrokes broadcast over Bluetooth to your computer, tablet, or phone. Perfect for using your mechanical keyboard with any Bluetooth device.

### Web Mode (Web → BLE)
ESP32-S3 creates a WiFi hotspot with a virtual keyboard. Type on your phone's browser, output goes to Bluetooth-connected device. Great for quick text input or when you don't have a physical keyboard.

### USB Mode (Web → USB) - Coming Soon
Virtual keyboard over WiFi, output as USB HID device. For devices that don't support Bluetooth but have USB.

## Features

- **3 Device Slots** - Pair up to 3 Bluetooth devices, hot-switch with Right Alt + 1/2/3
- **Cross-Platform** - Windows 11, macOS, iOS, Android, Linux
- **Proper BLE HID** - Battery Service + Device Info for full Windows compatibility
- **Web Interface** - Mobile-optimized virtual keyboard
- **Clean Mode Switching** - Proper cleanup prevents crashes and memory leaks

## Hardware

- ESP32-S3 DevKit (or any ESP32-S3 with USB-OTG)
- USB OTG adapter or hub
- USB keyboard (for OTG mode)

## Quick Start

```bash
# Build and flash
pio run -t upload -e ekey

# Monitor serial output
pio device monitor
```

**OTG Mode:**
1. Connect USB keyboard to ESP32-S3
2. Pair "EKey-1" in Bluetooth settings
3. Type!

**Web Mode:**
1. Hold BOOT button to switch modes
2. Connect phone to "EKey" WiFi
3. Open http://192.168.4.1
4. Pair "EKey-1" on target device
5. Type on virtual keyboard

## Controls

| Action | Method |
|--------|--------|
| Switch BLE device | Right Alt + 1/2/3 |
| Toggle mode | BOOT button or `TOGGLE_MODE` via serial |

## Configuration

Edit `src/Config.h`:

```c
// Bluetooth names
#define DEVICE_NAME_1 "EKey-1"
#define DEVICE_NAME_2 "EKey-2"
#define DEVICE_NAME_3 "EKey-3"

// WiFi
#define WIFI_AP_SSID "EKey"
#define WIFI_AP_PASSWORD ""  // Open network

// Hardware
#define LED_FEEDBACK_PIN 2   // -1 to disable
#define MODE_BUTTON_PIN 0    // BOOT button
```

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                      EKey                           │
├─────────────────────────────────────────────────────┤
│  ┌─────────────┐    ┌─────────────┐                │
│  │ USBManager  │    │ WebManager  │    Inputs      │
│  │ (USB Host)  │    │ (WiFi AP)   │                │
│  └──────┬──────┘    └──────┬──────┘                │
│         │                  │                        │
│         └────────┬─────────┘                        │
│                  ▼                                  │
│         ┌───────────────┐                          │
│         │    Bridge     │      Orchestration       │
│         │ (Mode Manager)│                          │
│         └───────┬───────┘                          │
│                 │                                   │
│         ┌───────┴───────┐                          │
│         ▼               ▼                          │
│  ┌─────────────┐ ┌─────────────┐                   │
│  │ BLEManager  │ │ USBDevice   │    Outputs       │
│  │ (BLE HID)   │ │ (USB HID)   │    (TODO)        │
│  └─────────────┘ └─────────────┘                   │
└─────────────────────────────────────────────────────┘
```

## Building

```bash
pio run              # Build
pio run -t upload    # Flash
pio device monitor   # Serial monitor
pio run -t uploadfs  # Upload web files (if using SPIFFS)
```

## Troubleshooting

**Windows can't see device:**
- Ensure Battery Service and Device Info Service are present (they are)
- Remove old pairing and re-pair

**Connection drops:**
- Check connection parameters in BLEManager
- Ensure proper cleanup on mode switch

**Boot loop:**
- Flash erase: `pio run -t erase`
- Check GPIO0 isn't held low

## License

MIT
