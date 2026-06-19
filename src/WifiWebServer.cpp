#include "WifiWebServer.h"
#include <ESPping.h>
#include "HTMLUI.h"
#include "ConfigManager.h"

WifiWebServer::WifiWebServer(int port)
    : server(port), mac(nullptr), ip(IPAddress(0,0,0,0)), gateway(IPAddress(0,0,0,0)), subnet(IPAddress(255,255,255,0)), ssid(""), password(""), port(port),
      configManager(nullptr), modbusPort(502), digitalInputs(nullptr), numDigitalInputs(0) {}

void WifiWebServer::setNetworkConfig(byte* mac, IPAddress ip, IPAddress gateway, IPAddress subnet, const char* ssid, const char* password) {
    this->mac = mac;
    this->ip = ip;
    this->gateway = gateway;
    this->subnet = subnet;
    this->ssid = ssid;
    this->password = password;
}

void WifiWebServer::setConfigManager(ConfigManager* cm) {
    this->configManager = cm;
}

void WifiWebServer::setModbusPort(uint16_t port) {
    this->modbusPort = port;
}

void WifiWebServer::setSensorData(bool* digitalInputs, int numDigitalInputs) {
    this->digitalInputs = digitalInputs;
    this->numDigitalInputs = numDigitalInputs;
}

void WifiWebServer::begin(bool verbose) {
    connect(verbose);
    server.begin();
    setupRoutes();
    if (Serial && verbose) {
        Serial.print("WiFi HTTP server at ");
        Serial.println(WiFi.localIP());
    }
}

bool WifiWebServer::connect(bool verbose) {
    if (!ssid || strlen(ssid) == 0) {
        if (verbose && Serial) Serial.println("No WiFi SSID configured");
        return false;
    }
    WiFi.mode(WIFI_STA);
    if (mac && (mac[0] != 0 || mac[1] != 0)) {
        esp_wifi_set_mac(WIFI_IF_STA, mac);
    }
    WiFi.config(ip, gateway, subnet);
    WiFi.begin(ssid, password);
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 40) {
        delay(500);
        if (verbose && Serial) Serial.print(".");
        timeout++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        if (verbose && Serial) {
            Serial.print("\nWiFi connected, IP: ");
            Serial.println(WiFi.localIP());
        }
        return Ping.ping(gateway, 2);
    }
    return false;
}

bool WifiWebServer::checkConnection(bool verbose) {
    if (WiFi.status() != WL_CONNECTED) {
        return connect(verbose);
    }
    return Ping.ping(gateway, 2);
}

void WifiWebServer::handleClient(bool verbose) {
    server.handleClient();
}

void WifiWebServer::setupRoutes() {
    server.on("/", std::bind(&WifiWebServer::handleRoot, this));
    server.on("/config", HTTP_GET, std::bind(&WifiWebServer::handleConfigGET, this));
    server.on("/config", HTTP_POST, std::bind(&WifiWebServer::handleConfigPOST, this));
    server.on("/update", HTTP_GET, std::bind(&WifiWebServer::handleUpdateGET, this));
    server.on("/update", HTTP_POST, std::bind(&WifiWebServer::handleUpdatePOST, this), std::bind(&WifiWebServer::handleUpdateUpload, this));
}

void WifiWebServer::handleRoot() {
    String ipStr = configManager ? configManager->ipToString(WiFi.localIP()) : "";
    String html = createDashboardPage("WiFi", ipStr, modbusPort, digitalInputs, numDigitalInputs);
    server.send(200, "text/html", html);
}

void WifiWebServer::handleConfigGET() {
    if (!configManager) {
        server.send(500, "text/plain", "ConfigManager not set");
        return;
    }
    NetworkConfig cfg;
    configManager->load(cfg);
    String html = createConfigPage(
        configManager->ipToString(cfg.eth_ip), configManager->ipToString(cfg.eth_gateway), configManager->ipToString(cfg.eth_subnet), configManager->macToString(cfg.eth_mac),
        configManager->ipToString(cfg.wifi_ip), configManager->ipToString(cfg.wifi_gateway), configManager->ipToString(cfg.wifi_subnet), configManager->macToString(cfg.wifi_mac),
        cfg.wifi_ssid, cfg.wifi_password,
        cfg.modbus_port, cfg.hostname
    );
    server.send(200, "text/html", html);
}

void WifiWebServer::handleConfigPOST() {
    if (!configManager) {
        server.send(500, "text/plain", "ConfigManager not set");
        return;
    }
    NetworkConfig cfg;
    configManager->load(cfg);
    
    if (server.hasArg("eth_ip")) cfg.eth_ip = configManager->stringToIP(server.arg("eth_ip"));
    if (server.hasArg("eth_gw")) cfg.eth_gateway = configManager->stringToIP(server.arg("eth_gw"));
    if (server.hasArg("eth_sn")) cfg.eth_subnet = configManager->stringToIP(server.arg("eth_sn"));
    if (server.hasArg("eth_mac")) configManager->stringToMac(server.arg("eth_mac"), cfg.eth_mac);
    
    if (server.hasArg("wifi_ip")) cfg.wifi_ip = configManager->stringToIP(server.arg("wifi_ip"));
    if (server.hasArg("wifi_gw")) cfg.wifi_gateway = configManager->stringToIP(server.arg("wifi_gw"));
    if (server.hasArg("wifi_sn")) cfg.wifi_subnet = configManager->stringToIP(server.arg("wifi_sn"));
    if (server.hasArg("wifi_mac")) configManager->stringToMac(server.arg("wifi_mac"), cfg.wifi_mac);
    
    if (server.hasArg("wifi_ssid")) cfg.wifi_ssid = server.arg("wifi_ssid");
    if (server.hasArg("wifi_pass")) cfg.wifi_password = server.arg("wifi_pass");
    
    if (server.hasArg("modbus_port")) cfg.modbus_port = (uint16_t)server.arg("modbus_port").toInt();
    if (server.hasArg("hostname")) cfg.hostname = server.arg("hostname");
    
    configManager->save(cfg);
    server.send(200, "text/html", "<html><body><h1>Saved</h1><p>Rebooting...</p></body></html>");
    delay(1000);
    ESP.restart();
}

void WifiWebServer::handleUpdateGET() {
    server.send(200, "text/html", createOTAFormPage());
}

void WifiWebServer::handleUpdatePOST() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
}

void WifiWebServer::handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("Update Success: %u\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}
