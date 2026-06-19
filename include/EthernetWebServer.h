#ifndef ETHERNET_WEBSERVER_H
#define ETHERNET_WEBSERVER_H

#include <Ethernet.h>
#include "WebServerInterface.h"

class ConfigManager;

class EthernetWebServer : public WebServerInterface {
public:
    EthernetWebServer(int port = 80);
    void setNetworkConfig(byte* mac, IPAddress ip, IPAddress gateway, IPAddress subnet);
    void setConfigManager(ConfigManager* cm) override;
    void setModbusPort(uint16_t port) override;
    void setSensorData(bool* digitalInputs, int numDigitalInputs) override;
    void setCoilData(bool* digitalOutputs, int numDigitalOutputs) override;

    void begin(bool verbose = true) override;
    bool connect(bool verbose = true) override;
    bool checkConnection(bool verbose = false) override;
    bool cableConnected(bool verbose = false);
    void handleClient(bool verbose = true) override;

private:
    EthernetServer server;
    EthernetClient client;
    byte* mac;
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    int port;

    ConfigManager* configManager;
    uint16_t modbusPort;
    bool* digitalInputs;
    int numDigitalInputs;
    bool* digitalOutputs;
    int numDigitalOutputs;

    void configureEthernetMAC();
    void sendResponse(int code, const String& contentType, const String& body);
    void handleRoot();
    void handleConfigGET();
    void handleConfigPOST(const String& body);
    void handleUpdateGET();
    void handleCoil(const String& query);
    void handleNotFound(const String& url);
};

#endif
