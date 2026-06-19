#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

struct NetworkConfig {
  IPAddress eth_ip;
  IPAddress eth_gateway;
  IPAddress eth_subnet;
  uint8_t eth_mac[6];

  IPAddress wifi_ip;
  IPAddress wifi_gateway;
  IPAddress wifi_subnet;
  uint8_t wifi_mac[6];

  String wifi_ssid;
  String wifi_password;

  uint16_t modbus_port;
  String hostname;
};

class ConfigManager {
public:
  ConfigManager();
  bool begin(const char* namespaceName = "netconfig");
  bool load(NetworkConfig& config);
  bool save(const NetworkConfig& config);
  bool hasConfig();
  void factoryReset();

  String ipToString(IPAddress ip);
  IPAddress stringToIP(const String& s);
  String macToString(const uint8_t mac[6]);
  void stringToMac(const String& s, uint8_t mac[6]);

private:
  Preferences prefs;
  void getFactoryDefaults(NetworkConfig& config);
};

#endif
