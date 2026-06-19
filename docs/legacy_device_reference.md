# Legacy Device Reference (Sanitized)

This file preserves the **structure** of the old per-device network assignments.
**Do not commit real IPs or MACs.** All values below are placeholders.

## Example Format

```cpp
// ---------- CONFIG Ethernet ----------
IPAddress eth_ip(xxx, xxx, xxx, xxx);
IPAddress eth_gateway(xxx, xxx, xxx, 1);
IPAddress eth_subnet(255, 255, 255, 0);
byte mac_eth[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xNN};
// ---------- CONFIG WiFi ----------
IPAddress wifi_ip(xxx, xxx, xxx, xxx);
IPAddress wifi_gateway(xxx, xxx, xxx, 1);
IPAddress wifi_subnet(255, 255, 255, 0);
byte mac_wifi[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEF, 0xNN};
```

## Devices

- CD impar, CD par, P01, P02, P03, E02

All real values have been removed. Use the web UI (`/config`) or NVS to configure each device at runtime.
