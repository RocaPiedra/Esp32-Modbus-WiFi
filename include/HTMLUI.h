#ifndef HTMLUI_H
#define HTMLUI_H

#include <Arduino.h>

String createDashboardPage(const String& interfaceName, const String& ip, uint16_t modbusPort, bool* digitalInputs, int numDigitalInputs);
String createConfigPage(const String& eth_ip, const String& eth_gw, const String& eth_sn, const String& eth_mac,
                        const String& wifi_ip, const String& wifi_gw, const String& wifi_sn, const String& wifi_mac,
                        const String& wifi_ssid, const String& wifi_pass,
                        uint16_t modbus_port, const String& hostname);
String createOTAFormPage();
String createOTAInfoPage(const String& activeInterface);

#endif
