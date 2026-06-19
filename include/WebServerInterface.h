#ifndef WEBSERVERINTERFACE_H
#define WEBSERVERINTERFACE_H

class ConfigManager;

class WebServerInterface {
public:
  virtual ~WebServerInterface() {}
  virtual void begin(bool verbose = false) = 0;
  virtual bool connect(bool verbose = false) = 0;
  virtual bool checkConnection(bool verbose = false) = 0;
  virtual void handleClient(bool verbose = false) = 0;
  virtual void setConfigManager(ConfigManager* cm) = 0;
  virtual void setModbusPort(uint16_t port) = 0;
  virtual void setSensorData(bool* digitalInputs, int numDigitalInputs) = 0;
};

#endif
