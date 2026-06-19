# AGENTS.md — InputReaderModbus

## Project Type & Stack
- **PlatformIO** Arduino project for an Industrial Shields ESP32 PLC (`14iosplc_ma`).
- Framework: `arduino` via a custom Industrial Shields ESP32 platform/package.
- Language: C++ (Arduino `.ino` + `.cpp`/`.h`).

## Build & Upload
- `pio run` — build
- `pio run --target upload` — flash to board
- `pio device monitor` — serial monitor at 115200 baud
- Active environment is `[env:board]` in `platformio.ini`.

## Architecture
- **Entry point**: `src/InputReaderModbus.ino`.
- **Active plan**: `docs/plan.md`. The agent must track plan progress and document all core architectural decisions, changes, and status updates in the `docs/` folder.
- **Interface**: `WebServerInterface` abstracts the dual-network HTTP server (dashboard + config + OTA).
- **Implementations**:
  - `EthernetWebServer` — raw `EthernetServer`/`EthernetClient`.
  - `WifiWebServer` — `WebServer` library.
- **Modbus**: Uses `Industrial-Shields/arduino-Modbus` (`ModbusTCPSlave`). A local `ModbusTCPSlaveWiFi` mirrors the logic for WiFi because the library is Ethernet-only.
- **Configuration**: `ConfigManager` uses ESP32 `Preferences` (NVS). All network settings are runtime-only; **never hardcode IPs, MACs, SSID, or passwords** in committed source.
- **Sensor logic**: PNP — read with `digitalRead(pin)` directly. **Do not invert** (`!digitalRead` is incorrect here).

## Key Code Conventions & Constraints
- Digital inputs use Industrial Shields pin aliases: `I0_0`, `I0_1`, etc.
- Only **2 sensors** are physically wired (`I0_0`, `I0_1`), but the Modbus register mapping must be generic to support future expansion.

## Dependencies (`platformio.ini`)
- `https://github.com/Industrial-Shields/arduino-Modbus.git`
- `dvarrel/ESPping@^1.0.5`
- `QNEthernet` is explicitly ignored (`lib_ignore`).

## Security & Privacy
- **No committed secrets.** All network configuration (IPs, MACs, SSID, password) is loaded from NVS at runtime.
- If a local defaults file is needed for development, copy `include/private_config_template.h` to `include/private_config.h` (gitignored) and fill in real values.
- Before concluding work, verify with: `grep -r "10\.92\.\|ssid\|password\|0xAA, 0xBB" src/ include/` returns no hardcoded secrets.
