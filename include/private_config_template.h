#ifndef PRIVATE_CONFIG_TEMPLATE_H
#define PRIVATE_CONFIG_TEMPLATE_H

#include <Arduino.h>
#include "ConfigManager.h"

// Copy this file to include/private_config.h and fill in your actual values.
// include/private_config.h is gitignored and must never be committed.

const NetworkConfig PRIVATE_CONFIG_DEFAULTS = {
  IPAddress(0, 0, 0, 0),          // eth_ip
  IPAddress(0, 0, 0, 0),          // eth_gateway
  IPAddress(255, 255, 255, 0),    // eth_subnet
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // eth_mac

  IPAddress(0, 0, 0, 0),          // wifi_ip
  IPAddress(0, 0, 0, 0),          // wifi_gateway
  IPAddress(255, 255, 255, 0),    // wifi_subnet
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // wifi_mac

  "YOUR_SSID",                    // wifi_ssid
  "YOUR_PASSWORD",                // wifi_password

  502,                            // modbus_port
  "inputreader"                   // hostname
};

#endif
