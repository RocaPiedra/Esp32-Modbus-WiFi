# InputReaderModbus

Industrial Shields ESP32 PLC firmware that turns digital/analog inputs into a **Modbus TCP slave** with dual-network **Ethernet-first, WiFi-fallback** operation. A web dashboard remains available on the active interface for status, configuration, and OTA updates.

A Python **Modbus TCP Master + Web HMI** tool is included for bench testing without a PLC/HMI.

---

## Features

- **Modbus TCP slave** on port 502 (configurable via NVS).
- **Dual network**: Ethernet primary, WiFi automatic fallback.
- **Web UI** on the active interface:
  - `/` — live dashboard with board, IP, network, input and coil states.
  - `/config` — edit network and Modbus settings stored in NVS.
  - `/update` — OTA firmware upload.
- **Board auto-detection** at compile time for `ESP32 PLC 21 IO+` and `PLC 14 IOS`.
- **PNP inputs**: read with `digitalRead()` directly — no inversion.
- **No committed secrets**: IPs, MACs, SSID, and password are loaded from NVS at runtime.

---

## Supported Hardware

| Board | PlatformIO `board` | Digital Inputs | Digital Outputs | Analog Inputs | Analog Outputs |
|---|---|---|---|---|---|
| ESP32 PLC 21 IO+ | `esp32plc_21` | I0_0 … I0_12 (13) | Q0_0 … Q0_7 (8) | I0_7 … I0_12 (6) | A0_5 … A0_7 (3) |
| PLC 14 IOS | `14iosplc_ma` | I0_0 … I0_8 (9) | Q0_0 … Q0_3 (4) | I0_7, I0_8 (2) | — |

Switch targets by editing `platformio.ini`:

```ini
board = esp32plc_21
; or
board = 14iosplc_ma
```

---

## Quick Start

1. **Install dependencies and build**:
   ```bash
   pio run
   ```

2. **Flash**:
   ```bash
   pio run --target upload
   ```

3. **Open the serial monitor**:
   ```bash
   pio device monitor
   ```

4. **Configure the PLC** via the web UI at `/config` (see below) or let it boot with blank defaults and connect to the WiFi AP for first setup.

---

## Configuration

All network settings are stored in ESP32 NVS via `ConfigManager`. No credentials are compiled into the firmware.

### First boot

If NVS is empty, the firmware loads factory defaults (blank/static-zero) and starts both web servers. Connect to the PLC's network and browse to:

```
http://<plc-ip>/config
```

### Settings available in `/config`

| Group | Fields |
|---|---|
| Ethernet | IP, gateway, subnet, MAC |
| WiFi | IP, gateway, subnet, MAC, SSID, password |
| Modbus | Port (default 502), hostname |

Changes are saved to NVS and take effect after a reboot.

### Local development defaults

For local builds you can copy the template and fill real values:

```bash
cp include/private_config_template.h include/private_config.h
```

`include/private_config.h` is gitignored and must **never** be committed.

---

## Web Dashboard

The dashboard is served on the active network interface only.

- **Active interface** and current IP
- **Live input states** (`I0_0`, `I0_1`, …)
- **Coil states** (`Q0_0`, `Q0_1`, …)
- **Modbus status** (port, number of mapped registers)
- **Board name**

---

## Testing with the HMI Tool

`tools/modbus_master_hmi.py` is a laptop-side Modbus TCP master with a built-in web HMI. Use it to read inputs, toggle coils, and verify dual-network behavior before connecting a real HMI/PLC.

### Requirements

```bash
cd tools
pip install -r requirements.txt   # flask, pymodbus, psutil
```

Or install manually:

```bash
pip install flask pymodbus psutil
```

### Run

```bash
python tools/modbus_master_hmi.py --web-port 8080
```

Open http://localhost:8080 in a browser.

### Usage

1. **Select a network interface** — the dropdown shows your machine's IPv4 interfaces.
2. **Discover the PLC** — click *Scan Subnet* to find devices listening on TCP/502, or type the PLC IP manually.
3. **Select board preset** — `esp32plc_21` or `14iosplc_ma` adjusts the input/coil counts automatically.
4. **Connect** — the HMI polls every second.
5. **Toggle coils** — click output buttons; the tool writes the coil and reads it back so the UI stays in sync.

### Tip

If you have both Ethernet and WiFi paths to the PLC, use the interface dropdown to test each path individually.

---

## Modbus Register Map

The slave uses unit ID `1` and maps the PLC I/O to standard Modbus tables.

| Table | Registers | PLC mapping |
|---|---|---|
| Discrete Inputs | 0 … N-1 | `I0_0` … `I0_N-1` |
| Coils | 0 … N-1 | `Q0_0` … `Q0_N-1` |
| Input Registers | 0 … N-1 | Analog inputs |
| Holding Registers | 0 … N-1 | Analog outputs |

*N depends on the detected board (see Supported Hardware).*

---

## Architecture

```
InputReaderModbus.ino
├── ConfigManager          (NVS read/write)
├── EthernetWebServer      (raw EthernetServer web UI)
├── WifiWebServer          (WebServer library web UI)
├── ModbusTCPSlave         (Industrial-Shields Ethernet Modbus)
├── ModbusTCPSlaveWiFi     (custom WiFiClient wrapper)
└── Shared register arrays (digitalInputs, digitalOutputs, analogInputs, analogOutputs)
```

At boot the firmware tries Ethernet first. If Ethernet is unreachable it falls back to WiFi. If the active link drops in operation, it switches back to the other interface and restarts the corresponding Modbus server.

---

## Security

- No IP addresses, MACs, SSIDs, or passwords are hardcoded in `src/` or `include/`.
- Sensitive defaults for development live in `include/private_config.h`, which is gitignored.
- Before contributing, verify:
  ```bash
  grep -r "10\.92\.\|ssid\|password\|0xAA, 0xBB" src/ include/
  ```
  This should return no matches.

---

## Versioning

Releases follow [Semantic Versioning](https://semver.org/).

- **v1.0.1** — Stable WiFi Modbus TCP with client-reuse fix, `TCP_NODELAY`, network fallback (cable-detection-based, not ping-based), OTA progress bar, dashboard AJAX polling, and Ethernet response chunking.
- **v1.0.0** — First production release with stable dual-network Modbus TCP slave (Ethernet + WiFi), web dashboard, configuration UI, OTA updates, and HMI test tool.

---

## License

MIT License — see `LICENSE` if present, otherwise treat as proprietary to the project owner.
