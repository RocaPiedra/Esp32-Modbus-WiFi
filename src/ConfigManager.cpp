#include "ConfigManager.h"

#if __has_include("private_config.h")
  #include "private_config.h"
  #define HAS_PRIVATE_CONFIG 1
#else
  #define HAS_PRIVATE_CONFIG 0
#endif

ConfigManager::ConfigManager() {}

bool ConfigManager::begin(const char* namespaceName) {
  return prefs.begin(namespaceName, false);
}

bool ConfigManager::load(NetworkConfig& config) {
  if (!prefs.isKey("eth_ip")) {
    return false;
  }
  config.eth_ip = stringToIP(prefs.getString("eth_ip", "0.0.0.0"));
  config.eth_gateway = stringToIP(prefs.getString("eth_gw", "0.0.0.0"));
  config.eth_subnet = stringToIP(prefs.getString("eth_sn", "255.255.255.0"));
  stringToMac(prefs.getString("eth_mac", "00:00:00:00:00:00"), config.eth_mac);

  config.wifi_ip = stringToIP(prefs.getString("wifi_ip", "0.0.0.0"));
  config.wifi_gateway = stringToIP(prefs.getString("wifi_gw", "0.0.0.0"));
  config.wifi_subnet = stringToIP(prefs.getString("wifi_sn", "255.255.255.0"));
  stringToMac(prefs.getString("wifi_mac", "00:00:00:00:00:00"), config.wifi_mac);

  config.wifi_ssid = prefs.getString("wifi_ssid", "");
  config.wifi_password = prefs.getString("wifi_pass", "");

  config.modbus_port = (uint16_t)prefs.getUInt("modbus_port", 502);
  config.hostname = prefs.getString("hostname", "inputreader");

  return true;
}

bool ConfigManager::save(const NetworkConfig& config) {
  prefs.putString("eth_ip", ipToString(config.eth_ip));
  prefs.putString("eth_gw", ipToString(config.eth_gateway));
  prefs.putString("eth_sn", ipToString(config.eth_subnet));
  prefs.putString("eth_mac", macToString(config.eth_mac));

  prefs.putString("wifi_ip", ipToString(config.wifi_ip));
  prefs.putString("wifi_gw", ipToString(config.wifi_gateway));
  prefs.putString("wifi_sn", ipToString(config.wifi_subnet));
  prefs.putString("wifi_mac", macToString(config.wifi_mac));

  prefs.putString("wifi_ssid", config.wifi_ssid);
  prefs.putString("wifi_pass", config.wifi_password);

  prefs.putUInt("modbus_port", config.modbus_port);
  prefs.putString("hostname", config.hostname);

  return true;
}

bool ConfigManager::hasConfig() {
  return prefs.isKey("eth_ip");
}

void ConfigManager::factoryReset() {
  NetworkConfig defaults;
  getFactoryDefaults(defaults);
  save(defaults);
}

void ConfigManager::getFactoryDefaults(NetworkConfig& config) {
#if HAS_PRIVATE_CONFIG
  config = PRIVATE_CONFIG_DEFAULTS;
#else
  config.eth_ip = IPAddress(0, 0, 0, 0);
  config.eth_gateway = IPAddress(0, 0, 0, 0);
  config.eth_subnet = IPAddress(255, 255, 255, 0);
  memset(config.eth_mac, 0, 6);

  config.wifi_ip = IPAddress(0, 0, 0, 0);
  config.wifi_gateway = IPAddress(0, 0, 0, 0);
  config.wifi_subnet = IPAddress(255, 255, 255, 0);
  memset(config.wifi_mac, 0, 6);

  config.wifi_ssid = "";
  config.wifi_password = "";

  config.modbus_port = 502;
  config.hostname = "inputreader";
#endif
}

String ConfigManager::ipToString(IPAddress ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

IPAddress ConfigManager::stringToIP(const String& s) {
  int parts[4] = {0, 0, 0, 0};
  int part = 0;
  for (int i = 0; i < s.length() && part < 4; i++) {
    char c = s[i];
    if (c == '.') {
      part++;
    } else if (c >= '0' && c <= '9') {
      parts[part] = parts[part] * 10 + (c - '0');
    }
  }
  return IPAddress(parts[0], parts[1], parts[2], parts[3]);
}

String ConfigManager::macToString(const uint8_t mac[6]) {
  String s;
  for (int i = 0; i < 6; i++) {
    if (i > 0) s += ":";
    if (mac[i] < 16) s += "0";
    s += String(mac[i], HEX);
  }
  return s;
}

void ConfigManager::stringToMac(const String& s, uint8_t mac[6]) {
  memset(mac, 0, 6);
  int byteIndex = 0;
  for (int i = 0; i < s.length() && byteIndex < 6; i++) {
    char c = s[i];
    if (c == ':') {
      byteIndex++;
    } else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
      int val = (c >= '0' && c <= '9') ? (c - '0') :
                (c >= 'a' && c <= 'f') ? (c - 'a' + 10) : (c - 'A' + 10);
      mac[byteIndex] = mac[byteIndex] * 16 + val;
    }
  }
}
