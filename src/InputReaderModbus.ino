#include <SPI.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <Ethernet.h>
#include <ModbusTCPSlave.h>
#include "ModbusTCPSlaveWiFi.h"
#include "ConfigManager.h"
#include "WebServerInterface.h"
#include "WifiWebServer.h"
#include "EthernetWebServer.h"

int digitalOutputsPins[] = {
#if defined(PIN_Q0_4)
  Q0_0, Q0_1, Q0_2, Q0_3, Q0_4,
#endif
};
int digitalInputsPins[] = {
#if defined(PIN_I0_6)
  I0_0, I0_1, I0_2, I0_3, I0_4, I0_5, I0_6,
#endif
};
int analogOutputsPins[] = {
#if defined(PIN_Q0_7)
  A0_5, A0_6, A0_7,
#endif
};
int analogInputsPins[] = {
#if defined(PIN_I0_12)
  I0_7, I0_8, I0_9, I0_10, I0_11, I0_12,
#endif
};

#define numDigitalOutputs int(sizeof(digitalOutputsPins) / sizeof(int))
#define numDigitalInputs int(sizeof(digitalInputsPins) / sizeof(int))
#define numAnalogOutputs int(sizeof(analogOutputsPins) / sizeof(int))
#define numAnalogInputs int(sizeof(analogInputsPins) / sizeof(int))

bool digitalOutputs[numDigitalOutputs];
bool digitalInputs[numDigitalInputs];
uint16_t analogOutputs[numAnalogOutputs];
uint16_t analogInputs[numAnalogInputs];

ConfigManager configManager;
NetworkConfig config;

ModbusTCPSlave modbusEth(502);
ModbusTCPSlaveWiFi modbusWifi(502);

EthernetWebServer ethWebServer(80);
WifiWebServer wifiWebServer(80);

WebServerInterface* activeWebServer = nullptr;
ModbusSlave* activeModbus = nullptr;

bool useEthernet = true;

void setup() {
  Serial.begin(115200);
  delay(1000);

  configManager.begin();
  if (!configManager.load(config)) {
    Serial.println("No config found, using factory defaults");
    configManager.factoryReset();
    configManager.load(config);
  }

  for (int i = 0; i < numDigitalOutputs; ++i) {
    digitalOutputs[i] = false;
    digitalWrite(digitalOutputsPins[i], digitalOutputs[i]);
  }
  for (int i = 0; i < numDigitalInputs; ++i) {
    digitalInputs[i] = digitalRead(digitalInputsPins[i]);
  }
  for (int i = 0; i < numAnalogOutputs; ++i) {
    analogOutputs[i] = 0;
    analogWrite(analogOutputsPins[i], analogOutputs[i]);
  }
  for (int i = 0; i < numAnalogInputs; ++i) {
    analogInputs[i] = analogRead(analogInputsPins[i]);
  }

  modbusEth.setDiscreteInputs(digitalInputs, numDigitalInputs);
  modbusEth.setCoils(digitalOutputs, numDigitalOutputs);
  modbusEth.setHoldingRegisters(analogOutputs, numAnalogOutputs);
  modbusEth.setInputRegisters(analogInputs, numAnalogInputs);

  modbusWifi.setDiscreteInputs(digitalInputs, numDigitalInputs);
  modbusWifi.setCoils(digitalOutputs, numDigitalOutputs);
  modbusWifi.setHoldingRegisters(analogOutputs, numAnalogOutputs);
  modbusWifi.setInputRegisters(analogInputs, numAnalogInputs);

  ethWebServer.setConfigManager(&configManager);
  ethWebServer.setModbusPort(config.modbus_port);
  ethWebServer.setSensorData(digitalInputs, numDigitalInputs);
  ethWebServer.setNetworkConfig(config.eth_mac, config.eth_ip, config.eth_gateway, config.eth_subnet);

  wifiWebServer.setConfigManager(&configManager);
  wifiWebServer.setModbusPort(config.modbus_port);
  wifiWebServer.setSensorData(digitalInputs, numDigitalInputs);
  wifiWebServer.setNetworkConfig(config.wifi_mac, config.wifi_ip, config.wifi_gateway, config.wifi_subnet, config.wifi_ssid.c_str(), config.wifi_password.c_str());

  esp_netif_init();
  esp_wifi_set_mode(WIFI_STA);
  if (config.wifi_mac[0] != 0 || config.wifi_mac[1] != 0) {
    esp_wifi_set_mac(WIFI_IF_STA, config.wifi_mac);
  }

  ethWebServer.begin(false);
  wifiWebServer.begin(false);

  if (ethWebServer.checkConnection(true)) {
    activeWebServer = &ethWebServer;
    activeModbus = &modbusEth;
    useEthernet = true;
    modbusEth.begin();
    Serial.println("Using Ethernet");
  } else if (wifiWebServer.checkConnection(true)) {
    activeWebServer = &wifiWebServer;
    activeModbus = &modbusWifi;
    useEthernet = false;
    modbusWifi.begin();
    Serial.println("Using WiFi");
  } else {
    Serial.println("No connection available");
  }
}

void loop() {
  for (int i = 0; i < numDigitalInputs; ++i) {
    digitalInputs[i] = digitalRead(digitalInputsPins[i]);
  }
  for (int i = 0; i < numAnalogInputs; ++i) {
    analogInputs[i] = analogRead(analogInputsPins[i]);
  }

  if (activeModbus) {
    activeModbus->update();
  }

  for (int i = 0; i < numDigitalOutputs; ++i) {
    digitalWrite(digitalOutputsPins[i], digitalOutputs[i]);
  }
  for (int i = 0; i < numAnalogOutputs; ++i) {
    analogWrite(analogOutputsPins[i], analogOutputs[i]);
  }

  if (activeWebServer) {
    if (!activeWebServer->checkConnection(false)) {
      Serial.println("Connection lost, attempting fallback...");
      bool connected = false;
      if (useEthernet) {
        if (wifiWebServer.checkConnection(true)) {
          activeWebServer = &wifiWebServer;
          activeModbus = &modbusWifi;
          useEthernet = false;
          modbusWifi.begin();
          Serial.println("Switched to WiFi");
          connected = true;
        }
      } else {
        if (ethWebServer.checkConnection(true)) {
          activeWebServer = &ethWebServer;
          activeModbus = &modbusEth;
          useEthernet = true;
          modbusEth.begin();
          Serial.println("Switched to Ethernet");
          connected = true;
        }
      }
      if (!connected) {
        Serial.println("No connection available, restarting...");
        delay(1000);
        ESP.restart();
        return;
      }
    }
    activeWebServer->handleClient();
  }
}
