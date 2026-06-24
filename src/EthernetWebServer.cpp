#include "EthernetWebServer.h"
#include <ESPping.h>
#include "HTMLUI.h"
#include "ConfigManager.h"

static String urlDecode(const String& s) {
  String r;
  for (int i = 0; i < s.length(); i++) {
    if (s[i] == '+') {
      r += ' ';
    } else if (s[i] == '%' && i + 2 < s.length()) {
      char hex[3] = {s[i+1], s[i+2], 0};
      r += (char)strtol(hex, nullptr, 16);
      i += 2;
    } else {
      r += s[i];
    }
  }
  return r;
}

static String getFormValue(const String& body, const String& key) {
  String prefix = key + "=";
  int start = body.indexOf(prefix);
  if (start < 0) return "";
  start += prefix.length();
  int end = body.indexOf("&", start);
  if (end < 0) end = body.length();
  return urlDecode(body.substring(start, end));
}

EthernetWebServer::EthernetWebServer(int port)
    : server(port), mac(nullptr), ip(IPAddress(0,0,0,0)), gateway(IPAddress(0,0,0,0)), subnet(IPAddress(255,255,255,0)), port(port),
      configManager(nullptr), modbusPort(502), digitalInputs(nullptr), numDigitalInputs(0), digitalOutputs(nullptr), numDigitalOutputs(0), boardName("Unknown") {}

void EthernetWebServer::setNetworkConfig(byte* mac, IPAddress ip, IPAddress gateway, IPAddress subnet) {
    this->mac = mac;
    this->ip = ip;
    this->gateway = gateway;
    this->subnet = subnet;
}

void EthernetWebServer::setConfigManager(ConfigManager* cm) {
    this->configManager = cm;
}

void EthernetWebServer::setModbusPort(uint16_t port) {
    this->modbusPort = port;
}

void EthernetWebServer::setSensorData(bool* digitalInputs, int numDigitalInputs) {
    this->digitalInputs = digitalInputs;
    this->numDigitalInputs = numDigitalInputs;
}

void EthernetWebServer::setCoilData(bool* digitalOutputs, int numDigitalOutputs) {
    this->digitalOutputs = digitalOutputs;
    this->numDigitalOutputs = numDigitalOutputs;
}

void EthernetWebServer::setBoardName(const char* name) {
    this->boardName = name;
}

void EthernetWebServer::begin(bool verbose) {
    connect(verbose);
    server.begin();
    if (Serial && verbose) {
        Serial.print("Ethernet HTTP server at ");
        Serial.println(Ethernet.localIP());
    }
}

bool EthernetWebServer::connect(bool verbose) {
    if (mac) {
        configureEthernetMAC();
    }
    delay(2000);
    return checkConnection(verbose);
}

bool EthernetWebServer::cableConnected(bool verbose) {
    bool disconnected = (Ethernet.linkStatus() == LinkOFF);
    if (verbose && Serial) {
        Serial.println(disconnected ? "Ethernet cable disconnected" : "Ethernet cable connected");
    }
    return !disconnected;
}

bool EthernetWebServer::checkConnection(bool verbose) {
    IPAddress local_ip = Ethernet.localIP();
    if (local_ip[0] == 0 || local_ip[0] == 255) {
        if (verbose && Serial) Serial.println("Invalid Ethernet IP");
        return false;
    }
    if (!cableConnected(verbose)) return false;
    // Note: on ESP32 with both WiFi and Ethernet up, Ping may be routed through
    // the default interface (often WiFi) rather than Ethernet. Therefore the
    // gateway ping is treated as advisory; a valid IP + physical link is enough
    // to consider Ethernet available.
    bool ping_ok = Ping.ping(gateway, 2);
    if (verbose && Serial) {
        Serial.println(String("Gateway ping (advisory): ") + (ping_ok ? "Success" : "Failed"));
    }
    return true;
}

void EthernetWebServer::configureEthernetMAC() {
    if (!mac) return;
    bool allZero = true;
    for (int i = 0; i < 6; i++) if (mac[i] != 0) allZero = false;
    if (allZero) return;
    
    Ethernet.begin(mac, ip, ip, gateway, subnet);
    if (Serial) {
        Serial.print("Ethernet MAC: ");
        uint8_t myMAC[6];
        Ethernet.MACAddress(myMAC);
        for (int i = 0; i < 6; ++i) {
            if (i > 0) Serial.print(':');
            Serial.print(myMAC[i], HEX);
        }
        Serial.println();
    }
}

static void writeAll(EthernetClient& client, const uint8_t* data, size_t len) {
    const size_t MAX_CHUNK = 512;
    size_t sent = 0;
    while (sent < len && client.connected()) {
        size_t toSend = min(len - sent, MAX_CHUNK);
        size_t chunk = client.write(data + sent, toSend);
        if (chunk == 0) {
            delay(1);
            continue;
        }
        sent += chunk;
    }
}

void EthernetWebServer::sendResponse(int code, const String& contentType, const String& body) {
    if (!client.connected()) return;
    String header = "HTTP/1.1 " + String(code) + " OK\r\n" +
                    "Content-Type: " + contentType + "\r\n" +
                    "Content-Length: " + String(body.length()) + "\r\n" +
                    "Connection: close\r\n\r\n";
    writeAll(client, (const uint8_t*)header.c_str(), header.length());
    if (body.length() > 0) {
        writeAll(client, (const uint8_t*)body.c_str(), body.length());
    }
    client.flush();
}

void EthernetWebServer::handleRoot() {
    String ipStr = configManager ? configManager->ipToString(Ethernet.localIP()) : "";
    String html = createDashboardPage(boardName, "Ethernet", ipStr, modbusPort, digitalInputs, numDigitalInputs, digitalOutputs, numDigitalOutputs);
    sendResponse(200, "text/html", html);
}

void EthernetWebServer::handleConfigGET() {
    if (!configManager) {
        sendResponse(500, "text/plain", "ConfigManager not set");
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
    sendResponse(200, "text/html", html);
}

void EthernetWebServer::handleConfigPOST(const String& body) {
    if (!configManager) {
        sendResponse(500, "text/plain", "ConfigManager not set");
        return;
    }
    NetworkConfig cfg;
    configManager->load(cfg);
    
    String val;
    val = getFormValue(body, "eth_ip");
    if (val.length() > 0) cfg.eth_ip = configManager->stringToIP(val);
    val = getFormValue(body, "eth_gw");
    if (val.length() > 0) cfg.eth_gateway = configManager->stringToIP(val);
    val = getFormValue(body, "eth_sn");
    if (val.length() > 0) cfg.eth_subnet = configManager->stringToIP(val);
    val = getFormValue(body, "eth_mac");
    if (val.length() > 0) configManager->stringToMac(val, cfg.eth_mac);
    
    val = getFormValue(body, "wifi_ip");
    if (val.length() > 0) cfg.wifi_ip = configManager->stringToIP(val);
    val = getFormValue(body, "wifi_gw");
    if (val.length() > 0) cfg.wifi_gateway = configManager->stringToIP(val);
    val = getFormValue(body, "wifi_sn");
    if (val.length() > 0) cfg.wifi_subnet = configManager->stringToIP(val);
    val = getFormValue(body, "wifi_mac");
    if (val.length() > 0) configManager->stringToMac(val, cfg.wifi_mac);
    
    val = getFormValue(body, "wifi_ssid");
    if (val.length() > 0) cfg.wifi_ssid = val;
    val = getFormValue(body, "wifi_pass");
    if (val.length() > 0) cfg.wifi_password = val;
    
    val = getFormValue(body, "modbus_port");
    if (val.length() > 0) cfg.modbus_port = (uint16_t)val.toInt();
    val = getFormValue(body, "hostname");
    if (val.length() > 0) cfg.hostname = val;
    
    configManager->save(cfg);
    sendResponse(200, "text/html", "<html><body><h1>Saved</h1><p>Rebooting...</p></body></html>");
    delay(1000);
    ESP.restart();
}

void EthernetWebServer::handleUpdateGET() {
    String html = createOTAInfoPage("Ethernet");
    sendResponse(200, "text/html", html);
}

void EthernetWebServer::handleCoil(const String& query) {
    int idx = -1;
    int val = -1;
    int i1 = query.indexOf("idx=");
    int i2 = query.indexOf("val=");
    if (i1 >= 0) idx = query.substring(i1 + 4).toInt();
    if (i2 >= 0) val = query.substring(i2 + 4).toInt();
    bool ok = false;
    if (idx >= 0 && idx < numDigitalOutputs && val >= 0 && val <= 1) {
        digitalOutputs[idx] = (val == 1);
        ok = true;
        if (Serial) {
            Serial.print("[WEB] Coil Q0_"); Serial.print(idx);
            Serial.print(" set to "); Serial.println(val ? "ON" : "OFF");
        }
    }
    String json = "{\"ok\":" + String(ok ? "true" : "false") + ",\"state\":" + String(digitalOutputs[idx] ? "true" : "false") + "}";
    sendResponse(200, "application/json", json);
}

void EthernetWebServer::handleApiState() {
    String json = "{\"inputs\":[";
    for (int i = 0; i < numDigitalInputs; i++) {
        json += String(digitalInputs[i] ? "true" : "false");
        if (i < numDigitalInputs - 1) json += ",";
    }
    json += "],\"outputs\":[";
    for (int i = 0; i < numDigitalOutputs; i++) {
        json += String(digitalOutputs[i] ? "true" : "false");
        if (i < numDigitalOutputs - 1) json += ",";
    }
    json += "]}";
    sendResponse(200, "application/json", json);
}

void EthernetWebServer::handleNotFound(const String& url) {
    sendResponse(404, "text/html", "<html><body><h1>404 Not Found</h1><p>" + url + "</p></body></html>");
}

void EthernetWebServer::handleClient(bool verbose) {
    client = server.available();
    if (!client) return;
    
    String method = "";
    String path = "";
    String currentLine = "";
    int contentLength = 0;
    boolean currentLineIsBlank = true;
    int lineNumber = 0;
    bool headersDone = false;
    String body = "";
    
    while (client.connected()) {
        if (client.available()) {
            char c = client.read();
            if (!headersDone) {
                if (c == '\n' && currentLineIsBlank) {
                    headersDone = true;
                    if (method != "POST" || contentLength == 0) break;
                } else if (c == '\n') {
                    if (lineNumber == 0) {
                        int sp1 = currentLine.indexOf(' ');
                        int sp2 = currentLine.indexOf(' ', sp1 + 1);
                        if (sp1 > 0) {
                            method = currentLine.substring(0, sp1);
                            if (sp2 > sp1) path = currentLine.substring(sp1 + 1, sp2);
                            else path = currentLine.substring(sp1 + 1);
                        }
                    } else {
                        if (currentLine.startsWith("Content-Length:")) {
                            contentLength = currentLine.substring(15).toInt();
                        }
                    }
                    currentLine = "";
                    lineNumber++;
                    currentLineIsBlank = true;
                } else if (c != '\r') {
                    currentLine += c;
                    currentLineIsBlank = false;
                }
            } else {
                body += c;
                if (body.length() >= contentLength) break;
            }
        }
    }
    
    if (method == "GET" && path == "/") handleRoot();
    else if (method == "GET" && path == "/config") handleConfigGET();
    else if (method == "POST" && path == "/config") handleConfigPOST(body);
    else if (method == "GET" && path == "/update") handleUpdateGET();
    else if (method == "GET" && path == "/api/state") handleApiState();
    else if (method == "GET" && path.startsWith("/coil")) handleCoil(path.substring(5));
    else handleNotFound(path);
    
    client.stop();
}
