#ifndef WIFIWEBSERVER_H
#define WIFIWEBSERVER_H

#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#include "WebServerInterface.h"

class ConfigManager;

class WifiWebServer : public WebServerInterface {
public:
  WifiWebServer(int port = 80);
  void setNetworkConfig(byte* mac, IPAddress ip, IPAddress gateway, IPAddress subnet, const char* ssid, const char* password);
  void setConfigManager(ConfigManager* cm) override;
  void setModbusPort(uint16_t port) override;
  void setSensorData(bool* digitalInputs, int numDigitalInputs) override;

  void begin(bool verbose = true) override;
  bool connect(bool verbose = true) override;
  bool checkConnection(bool verbose = false) override;
  void handleClient(bool verbose = false) override;

private:
  WebServer server;
  byte* mac;
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  const char* ssid;
  const char* password;
  int port;

  ConfigManager* configManager;
  uint16_t modbusPort;
  bool* digitalInputs;
  int numDigitalInputs;

  void setupRoutes();
  void handleRoot();
  void handleConfigGET();
  void handleConfigPOST();
  void handleUpdateGET();
  void handleUpdatePOST();
  void handleUpdateUpload();
};

#endif
