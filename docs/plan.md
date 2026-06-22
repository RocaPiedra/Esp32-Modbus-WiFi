# InputReaderModbus — Implementation Plan

## Overview
Migrate the current HTTP-based sensor reader to a **Modbus TCP Slave** with dual-network (Ethernet + WiFi) auto-fallback, while retaining a web dashboard for status, configuration, and OTA updates.

**Target board:** `14iosplc_ma` (Industrial Shields ESP32 PLC)  
**Framework:** Arduino via Industrial Shields ESP32 platform  
**Repo goal:** Must remain safe for public release (no committed credentials or static network config).

---

## Phase 1: Dependencies & Build (`platformio.ini`)
- [ ] Add `https://github.com/Industrial-Shields/arduino-Modbus.git` to `lib_deps`.
- [ ] Remove `EthernetWebServer` from `lib_deps` (no longer needed as primary protocol).
- [ ] Keep `dvarrel/ESPping@^1.0.5` for connectivity checks.
- [ ] Keep `lib_ignore = QNEthernet` and board definitions.
- [ ] Ensure `framework = arduino` and `monitor_speed = 115200` remain.

**Verification:** `pio run` compiles without errors.

---

## Phase 2: Configuration Storage (No Hardcoded Secrets)
- [ ] Create `ConfigManager` (`include/ConfigManager.h`, `src/ConfigManager.cpp`).
- [ ] Use ESP32 `Preferences` (NVS) to persist at runtime:
  - Ethernet: IP, gateway, subnet, MAC
  - WiFi: IP, gateway, subnet, MAC, SSID, password
  - Modbus: port (default 502), device hostname
- [ ] On first boot (NVS empty), load from a **gitignored** `config_defaults.json` on LittleFS/SPIFFS, or simply use safe blank defaults and require user configuration via the web UI.
- [ ] **Critical:** Remove all hardcoded IPs, MACs, SSID, and password from `src/` and `include/`.
- [ ] Provide a web form at `/config` to read/write these values.

**Verification:** After flashing, the device boots without hardcoded network values and exposes `/config`.

---

## Phase 3: Modbus TCP Core (Dual Network)
- [ ] Define full register arrays as per the sample (`sample/ModbusTCPSlave.ino`):
  - `digitalOutputsPins[]`, `digitalInputsPins[]`
  - `analogOutputsPins[]`, `analogInputsPins[]`
  - Corresponding state arrays: `digitalOutputs[]`, `digitalInputs[]`, `analogOutputs[]`, `analogInputs[]`
- [ ] For **now**, only `I0_0` and `I0_1` are physically wired, but the mapping must be generic.
- [ ] **Sensor logic:** PNP — use `digitalRead(pin)` directly (no inversion).
- [ ] Create `ModbusTCPSlaveWiFi` (`include/ModbusTCPSlaveWiFi.h`, `src/ModbusTCPSlaveWiFi.cpp`):
  - Replicates `ModbusTCPSlave` logic using `WiFiServer`/`WiFiClient`.
  - Inherits from the same `ModbusSlave` base so both slaves share register arrays.
- [ ] In `InputReaderModbus.ino`:
  - Instantiate `ModbusTCPSlave modbusEth(port)` and `ModbusTCPSlaveWiFi modbusWifi(port)`.
  - Point both to the same arrays via `setDiscreteInputs()`, `setCoils()`, etc.
  - Boot sequence: try Ethernet first → fallback to WiFi.
  - `loop()`:
    1. Read physical inputs into `digitalInputs[]`.
    2. Call `activeModbus->update()`.
    3. (Future: write `digitalOutputs[]` to physical pins after `update()`.)
  - If active connection drops, switch to the other network and update the active Modbus pointer.

**Verification:** A Modbus master (e.g., `mbtget` or a PLC) can read discrete inputs from either Ethernet or WiFi IP.

---

## Phase 4: Web Interface (Dashboard + Config)
- [ ] Refactor `WebServerInterface`, `EthernetWebServer`, `WifiWebServer`.
- [ ] Remove old sensor JSON endpoints (`/sensor/0`, `/sensores`, etc.) — Modbus replaces the data API.
- [ ] New routes (on both Ethernet and WiFi servers):
  - `/` — Status dashboard HTML:
    - Active network interface
    - Current IP address
    - Sensor states (`I0_0`, `I0_1`)
    - Modbus status (port, registers mapped)
  - `/config` — Form to edit NVS network settings (IP, MAC, SSID, password, etc.).
  - `/update` — OTA firmware upload page.
- [ ] Update `HTMLUI.cpp` to generate the new dashboard, config form, and OTA page.

**Verification:** Browser access to either IP shows the dashboard; `/config` persists changes across reboots.

---

## Phase 5: OTA Updates
- [ ] Integrate `Update.h` into the web server handlers.
- [ ] `/update` GET serves an upload form (reusing the UI from `lib/OTA_ESP32.txt`).
- [ ] `/update` POST receives the firmware `.bin` and flashes it via `Update.write()`.
- [ ] Optional: add `ESPmDNS.h` to advertise `http://<hostname>.local`.

**Verification:** Uploading a `.bin` via the web interface updates the firmware and the device reboots.

---

## Phase 6: Security & Privacy for Public Repo
- [ ] Ensure no secrets exist in committed source.
- [ ] Add `.gitignore` rules for any local config files (e.g., `data/config.json`, build artifacts).
- [ ] Document in `docs/SECURITY.md` that all sensitive config is runtime-only via NVS or the web UI.
- [ ] If a default config file is used for development, keep it out of the repo (add to `.gitignore`).

**Verification:** `grep -r "10\.92\.\|ssid\|password\|0xAA, 0xBB" src/ include/` returns no committed secrets.

---

## Phase 7: Cleanup
- [ ] Remove `lib/OTA_ESP32.txt` if fully superseded.
- [ ] Remove `lib/infoPLCs.txt` or move it to `docs/` as a reference (sanitize if needed).
- [ ] Ensure `inputReaderWebServer.ino` remains deleted.
- [ ] Delete unused OTA UI strings from `HTMLUI.cpp` if they are no longer referenced.
- [ ] Verify `pio run` succeeds and `pio check` (if available) is clean.

---

## Architecture Summary

```
InputReaderModbus.ino
├── ConfigManager (NVS read/write)
├── NetworkManager (detect & switch Ethernet/WiFi)
│   ├── EthernetLink (ModbusTCPSlave + EthernetWebServer)
│   └── WiFiLink     (ModbusTCPSlaveWiFi + WifiWebServer)
├── Shared Register Arrays
│   ├── digitalInputs[...]  <-- I0_0, I0_1, ...
│   ├── digitalOutputs[...] <-- (future)
│   ├── analogInputs[...]   <-- (future)
│   └── analogOutputs[...]  <-- (future)
└── HTMLUI (dashboard, config form, OTA page)
```

## Current Status
- **Phase 1:** Completed — `arduino-Modbus` added; `EthernetWebServer` removed from `lib_deps`.
- **Phase 2:** Completed — `ConfigManager` created with NVS persistence; hardcoded secrets removed from source; `private_config_template.h` provided for local development (gitignored).
- **Phase 3:** Completed — `ModbusTCPSlaveWiFi` created; `InputReaderModbus.ino` rewritten with dual-network Modbus core and full register mapping.
- **Phase 4:** Completed — Web interface refactored to dashboard (`/`), config form (`/config`), and OTA info (`/update`); old JSON endpoints removed.
- **Phase 5:** Completed — OTA integrated into `WifiWebServer` via `Update.h` and multipart upload.
- **Phase 6:** Completed — No secrets in `src/` or `include/`; `.gitignore` added; `lib/infoPLCs.txt` sanitized and moved to `docs/legacy_device_reference.md`; `lib/OTA_ESP32.txt` deleted.
- **Phase 7:** In Progress — Build verification pending (PlatformIO CLI not available in environment).

## Phase 9: WiFi Modbus Stability
- [x] Diagnose WiFi Modbus TCP drops (`connection reset by peer` / `errno 32`).
- [x] Fix `ModbusTCPSlaveWiFi`:
  - Initialize `_lastRequestTime` when accepting a client.
  - Increase receive timeout from 1 s to 10 s to tolerate WiFi jitter.
  - Do **not** close the client socket on idle timeout; Modbus TCP masters expect persistent connections.
  - Detect client disconnect and return to `Idle` cleanly.
- [x] Keep `ModbusTCPSlave` (Ethernet) unchanged.

**Verification:** WiFi Modbus connection remains stable under continuous HMI polling.

**Fallback noted:** If the native `ModbusTCPSlaveWiFi` still proves unstable, the `emelianov/modbus-esp8266` library (`ModbusTCP` class for ESP32) is a viable replacement candidate. It would require resolving header-name conflicts with `Industrial-Shields/arduino-Modbus` and validating against the Industrial Shields Ethernet interface.
