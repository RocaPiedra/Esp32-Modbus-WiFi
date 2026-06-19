#ifndef MODBUS_TCP_SLAVE_WIFI_H
#define MODBUS_TCP_SLAVE_WIFI_H

#include <WiFi.h>
#include "ModbusTCP.h"
#include "ModbusSlave.h"

class ModbusTCPSlaveWiFi : public ModbusSlave {
public:
    explicit ModbusTCPSlaveWiFi(uint16_t port = 502);
    void begin();
    virtual void update();
private:
    bool receiveRequest();
    bool sendResponse();
    WiFiServer _server;
    WiFiClient _currentClient;
    uint32_t _lastRequestTime;
    uint8_t _adu[MODBUS_TCP_ADU_SIZE];
};

#endif
