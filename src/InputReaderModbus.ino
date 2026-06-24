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

#if defined(ESP32PLC)
  // ESP32 PLC 21 IO+
  const char* BOARD_NAME = "ESP32 PLC 21 IO+";
  int digitalOutputsPins[] = { Q0_0, Q0_1, Q0_2, Q0_3, Q0_4, Q0_5, Q0_6, Q0_7 };
  int digitalInputsPins[]  = { I0_0, I0_1, I0_2, I0_3, I0_4, I0_5, I0_6, I0_7, I0_8, I0_9, I0_10, I0_11, I0_12 };
  int analogOutputsPins[]  = { A0_5, A0_6, A0_7 };
  int analogInputsPins[]   = { I0_7, I0_8, I0_9, I0_10, I0_11, I0_12 };
#elif defined(PLC14IOS)
  // ESP32 PLC 14 (4-20 mA)
  const char* BOARD_NAME = "ESP32 PLC 14 (4-20 mA)";
  int digitalOutputsPins[] = { Q0_0, Q0_1, Q0_2, Q0_3 };
  int digitalInputsPins[]  = { I0_0, I0_1, I0_2, I0_3, I0_4, I0_5, I0_6, I0_7, I0_8 };
  int analogOutputsPins[]  = {};
  int analogInputsPins[]   = { I0_7, I0_8 };
#else
  #error "Unsupported board. Please use esp32plc_21 or 14iosplc_ma."
#endif

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

void printConfigDebug();
void setup();
void loop();

void printConfigDebug() {
  Serial.println("--- Loaded Configuration ---");
  Serial.print("Ethernet IP: "); Serial.println(configManager.ipToString(config.eth_ip));
  Serial.print("Ethernet GW: "); Serial.println(configManager.ipToString(config.eth_gateway));
  Serial.print("Ethernet MAC: "); Serial.println(configManager.macToString(config.eth_mac));
  Serial.print("WiFi IP: "); Serial.println(configManager.ipToString(config.wifi_ip));
  Serial.print("WiFi GW: "); Serial.println(configManager.ipToString(config.wifi_gateway));
  Serial.print("WiFi MAC: "); Serial.println(configManager.macToString(config.wifi_mac));
  Serial.print("WiFi SSID: "); Serial.println(config.wifi_ssid);
  Serial.print("Modbus Port: "); Serial.println(config.modbus_port);
  Serial.print("Hostname: "); Serial.println(config.hostname);
  Serial.println("--------------------------");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("  InputReaderModbus - BOOT START");
  Serial.println("========================================");
  Serial.print("[INIT] Board: ");
  Serial.println(BOARD_NAME);

  Serial.println("[INIT] Starting ConfigManager...");
  configManager.begin();
  if (!configManager.load(config)) {
    Serial.println("[INIT] No NVS config found. Loading factory defaults.");
    configManager.factoryReset();
    configManager.load(config);
  } else {
    Serial.println("[INIT] Config loaded from NVS.");
  }
  printConfigDebug();

  Serial.println("[INIT] Initializing digital outputs...");
  for (int i = 0; i < numDigitalOutputs; ++i) {
    digitalOutputs[i] = false;
    digitalWrite(digitalOutputsPins[i], digitalOutputs[i]);
  }
  Serial.println("[INIT] Initializing digital inputs...");
  for (int i = 0; i < numDigitalInputs; ++i) {
    digitalInputs[i] = digitalRead(digitalInputsPins[i]);
    Serial.print("  I0_"); Serial.print(i); Serial.print(" = "); Serial.println(digitalInputs[i] ? "HIGH" : "LOW");
  }
  Serial.println("[INIT] Initializing analog outputs...");
  for (int i = 0; i < numAnalogOutputs; ++i) {
    analogOutputs[i] = 0;
    analogWrite(analogOutputsPins[i], analogOutputs[i]);
  }
  Serial.println("[INIT] Initializing analog inputs...");
  for (int i = 0; i < numAnalogInputs; ++i) {
    analogInputs[i] = analogRead(analogInputsPins[i]);
  }

  Serial.println("[INIT] Linking Modbus registers...");
  modbusEth.setDiscreteInputs(digitalInputs, numDigitalInputs);
  modbusEth.setCoils(digitalOutputs, numDigitalOutputs);
  modbusEth.setHoldingRegisters(analogOutputs, numAnalogOutputs);
  modbusEth.setInputRegisters(analogInputs, numAnalogInputs);

  modbusWifi.setDiscreteInputs(digitalInputs, numDigitalInputs);
  modbusWifi.setCoils(digitalOutputs, numDigitalOutputs);
  modbusWifi.setHoldingRegisters(analogOutputs, numAnalogOutputs);
  modbusWifi.setInputRegisters(analogInputs, numAnalogInputs);
  Serial.println("[INIT] Modbus registers linked.");

  ethWebServer.setConfigManager(&configManager);
  ethWebServer.setModbusPort(config.modbus_port);
  ethWebServer.setSensorData(digitalInputs, numDigitalInputs);
  ethWebServer.setCoilData(digitalOutputs, numDigitalOutputs);
  ethWebServer.setBoardName(BOARD_NAME);
  ethWebServer.setNetworkConfig(config.eth_mac, config.eth_ip, config.eth_gateway, config.eth_subnet);

  wifiWebServer.setConfigManager(&configManager);
  wifiWebServer.setModbusPort(config.modbus_port);
  wifiWebServer.setSensorData(digitalInputs, numDigitalInputs);
  wifiWebServer.setCoilData(digitalOutputs, numDigitalOutputs);
  wifiWebServer.setBoardName(BOARD_NAME);
  wifiWebServer.setNetworkConfig(config.wifi_mac, config.wifi_ip, config.wifi_gateway, config.wifi_subnet, config.wifi_ssid.c_str(), config.wifi_password.c_str());

  Serial.println("[INIT] Configuring WiFi stack...");
  esp_netif_init();
  esp_wifi_set_mode(WIFI_STA);
  if (config.wifi_mac[0] != 0 || config.wifi_mac[1] != 0) {
    Serial.print("[INIT] Setting custom WiFi MAC: ");
    Serial.println(configManager.macToString(config.wifi_mac));
    esp_wifi_set_mac(WIFI_IF_STA, config.wifi_mac);
  }

  Serial.println("[INIT] Starting Ethernet WebServer...");
  ethWebServer.begin(true);
  Serial.println("[INIT] Starting WiFi WebServer...");
  wifiWebServer.begin(true);

  Serial.println("[INIT] Selecting active network interface...");
  if (ethWebServer.checkConnection(true)) {
    activeWebServer = &ethWebServer;
    activeModbus = &modbusEth;
    useEthernet = true;
    modbusEth.begin();
    Serial.println("[INIT] >>> Ethernet selected as active interface <<<");
  } else if (wifiWebServer.checkConnection(true)) {
    activeWebServer = &wifiWebServer;
    activeModbus = &modbusWifi;
    useEthernet = false;
    modbusWifi.begin();
    Serial.println("[INIT] >>> WiFi selected as active interface <<<");
  } else {
    Serial.println("[INIT] >>> WARNING: No network connection available! <<<");
  }
  Serial.println("========================================");
  Serial.println("  InputReaderModbus - BOOT COMPLETE");
  Serial.println("========================================\n");
}

static unsigned long lastDebugPrint = 0;
static unsigned long lastLinkCheck = 0;
static int fallbackCounter = 0;

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

  // Periodic debug output every 5 seconds
  if (millis() - lastDebugPrint >= 5000) {
    lastDebugPrint = millis();
    Serial.print("[LOOP] Sensors -> ");
    for (int i = 0; i < numDigitalInputs; ++i) {
      Serial.print("I0_"); Serial.print(i); Serial.print(":"); Serial.print(digitalInputs[i] ? "ON" : "OFF");
      if (i < numDigitalInputs - 1) Serial.print(", ");
    }
    Serial.println();
    
    Serial.print("[LOOP] Active interface: ");
    Serial.print(useEthernet ? "Ethernet" : "WiFi");
    Serial.print(" IP: ");
    Serial.println(useEthernet ? config.eth_ip.toString() : config.wifi_ip.toString());
    Serial.print("[LOOP] Coils -> ");
    for (int i = 0; i < numDigitalOutputs; ++i) {
      Serial.print("Q0_"); Serial.print(i); Serial.print(":"); Serial.print(digitalOutputs[i] ? "ON" : "OFF");
      if (i < numDigitalOutputs - 1) Serial.print(", ");
    }
    Serial.println();
    Serial.print("[LOOP] WebServer alive: ");
    Serial.println(activeWebServer ? "YES" : "NO");
    Serial.print("[LOOP] Modbus alive: ");
    Serial.println(activeModbus ? "YES" : "NO");
  }

  if (activeWebServer) {
    activeWebServer->handleClient();

    // Only test the active link every 5 s to avoid flooding the network with pings.
    if (millis() - lastLinkCheck >= 5000) {
      lastLinkCheck = millis();
      bool linkOk = activeWebServer->checkConnection(false);
      if (!linkOk) {
        Serial.println("[NET] Active link down, attempting recovery...");
        bool connected = false;

        if (useEthernet) {
          // Currently on Ethernet: try WiFi first.
          if (wifiWebServer.checkConnection(true)) {
            activeWebServer = &wifiWebServer;
            activeModbus = &modbusWifi;
            useEthernet = false;
            modbusWifi.begin();
            Serial.println("[NET] >>> Switched to WiFi <<<");
            connected = true;
          }

          // If WiFi is not available, retry Ethernet (cable may have been reconnected).
          if (!connected && ethWebServer.checkConnection(true)) {
            modbusEth.begin();
            Serial.println("[NET] >>> Ethernet reconnected <<<");
            connected = true;
          }
        } else {
          // Currently on WiFi: only switch to Ethernet if the cable is physically
          // connected. If not, keep trying to reconnect to WiFi.
          if (ethWebServer.cableConnected(false) && ethWebServer.checkConnection(true)) {
            activeWebServer = &ethWebServer;
            activeModbus = &modbusEth;
            useEthernet = true;
            modbusEth.begin();
            Serial.println("[NET] >>> Switched to Ethernet <<<");
            connected = true;
          }

          if (!connected && wifiWebServer.checkConnection(true)) {
            modbusWifi.begin();
            Serial.println("[NET] >>> WiFi reconnected <<<");
            connected = true;
          }
        }

        if (connected) {
          fallbackCounter = 0;
        } else {
          fallbackCounter++;
          Serial.printf("[NET] Recovery attempt %d failed\n", fallbackCounter);
          if (fallbackCounter >= 10) {
            Serial.println("[NET] No connection available after 10 attempts, restarting...");
            delay(1000);
            ESP.restart();
            return;
          }
        }
      } else {
        fallbackCounter = 0;
      }
    }
  }

  // Small delay to let the network stack drain buffers and avoid tight-loop issues.
  delay(1);
}
