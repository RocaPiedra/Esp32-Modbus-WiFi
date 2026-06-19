#include <Arduino.h>
#include "HTMLUI.h"

String createDashboardPage(const String& interfaceName, const String& ip, uint16_t modbusPort, bool* digitalInputs, int numDigitalInputs) {
  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>"
    "body{font-family:Arial,sans-serif;background:#f4f6f8;margin:0;padding:16px;}"
    "h1{color:#1976d2;} .card{background:#fff;border-radius:8px;padding:16px;margin-bottom:16px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
    "table{border-collapse:collapse;width:100%;} th,td{padding:12px;text-align:left;border-bottom:1px solid #ddd;}"
    "th{background:#1976d2;color:#fff;} .on{color:#43a047;font-weight:bold;} .off{color:#e53935;font-weight:bold;}"
    "a{display:inline-block;margin-top:8px;padding:10px 16px;background:#1976d2;color:#fff;text-decoration:none;border-radius:4px;}"
    "</style></head><body>");
  html += "<h1>InputReaderModbus</h1>";
  html += "<div class='card'><h2>Status</h2>";
  html += "<p><b>Interface:</b> " + interfaceName + "</p>";
  html += "<p><b>IP:</b> " + ip + "</p>";
  html += "<p><b>Modbus Port:</b> " + String(modbusPort) + "</p>";
  html += "</div>";
  html += "<div class='card'><h2>Digital Inputs</h2><table><tr><th>Pin</th><th>State</th></tr>";
  for (int i = 0; i < numDigitalInputs; i++) {
    String stateClass = digitalInputs[i] ? "on" : "off";
    String stateText = digitalInputs[i] ? "ON" : "OFF";
    html += "<tr><td>I0_" + String(i) + "</td><td class='" + stateClass + "'>" + stateText + "</td></tr>";
  }
  html += "</table></div>";
  html += "<div class='card'><a href='/config'>Configuration</a> <a href='/update'>OTA Update</a></div>";
  html += "</body></html>";
  return html;
}

String createConfigPage(const String& eth_ip, const String& eth_gw, const String& eth_sn, const String& eth_mac,
                        const String& wifi_ip, const String& wifi_gw, const String& wifi_sn, const String& wifi_mac,
                        const String& wifi_ssid, const String& wifi_pass,
                        uint16_t modbus_port, const String& hostname) {
  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>"
    "body{font-family:Arial,sans-serif;background:#f4f6f8;margin:0;padding:16px;}"
    "h1{color:#1976d2;} .card{background:#fff;border-radius:8px;padding:16px;margin-bottom:16px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
    "label{display:block;margin-top:8px;font-weight:bold;} input{width:100%;padding:8px;margin-top:4px;box-sizing:border-box;}"
    "input[type=submit]{background:#1976d2;color:#fff;border:none;padding:12px;border-radius:4px;cursor:pointer;margin-top:16px;}"
    "</style></head><body>");
  html += "<h1>Configuration</h1>";
  html += "<form method='POST' action='/config'>";
  html += "<div class='card'><h2>Ethernet</h2>";
  html += "<label>IP</label><input name='eth_ip' value='" + eth_ip + "'>";
  html += "<label>Gateway</label><input name='eth_gw' value='" + eth_gw + "'>";
  html += "<label>Subnet</label><input name='eth_sn' value='" + eth_sn + "'>";
  html += "<label>MAC</label><input name='eth_mac' value='" + eth_mac + "'>";
  html += "</div>";
  html += "<div class='card'><h2>WiFi</h2>";
  html += "<label>IP</label><input name='wifi_ip' value='" + wifi_ip + "'>";
  html += "<label>Gateway</label><input name='wifi_gw' value='" + wifi_gw + "'>";
  html += "<label>Subnet</label><input name='wifi_sn' value='" + wifi_sn + "'>";
  html += "<label>MAC</label><input name='wifi_mac' value='" + wifi_mac + "'>";
  html += "<label>SSID</label><input name='wifi_ssid' value='" + wifi_ssid + "'>";
  html += "<label>Password</label><input type='password' name='wifi_pass' value='" + wifi_pass + "'>";
  html += "</div>";
  html += "<div class='card'><h2>Modbus / General</h2>";
  html += "<label>Modbus Port</label><input name='modbus_port' value='" + String(modbus_port) + "'>";
  html += "<label>Hostname</label><input name='hostname' value='" + hostname + "'>";
  html += "<input type='submit' value='Save & Reboot'></div>";
  html += "</form>";
  html += "</body></html>";
  return html;
}

String createOTAFormPage() {
  return F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>"
    "body{font-family:Arial,sans-serif;background:#f4f6f8;margin:0;padding:16px;}"
    "h1{color:#1976d2;} .card{background:#fff;border-radius:8px;padding:16px;margin-bottom:16px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
    "input[type=file]{margin-top:8px;} button{background:#1976d2;color:#fff;border:none;padding:12px;border-radius:4px;cursor:pointer;margin-top:16px;}"
    "</style></head><body>"
    "<h1>OTA Update</h1><div class='card'>"
    "<input type='file' id='file'><br>"
    "<button onclick='upload()'>Update</button>"
    "<p id='status'></p>"
    "</div><script>"
    "async function upload(){"
    "const f=document.getElementById('file').files[0];"
    "if(!f)return;"
    "document.getElementById('status').innerText='Uploading...';"
    "const data=new FormData();data.append('update',f);"
    "const r=await fetch('/update',{method:'POST',body:data});"
    "document.getElementById('status').innerText=r.ok?'OK, rebooting...':'FAIL';"
    "}</script></body></html>");
}

String createOTAInfoPage(const String& activeInterface) {
  String html = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>"
    "body{font-family:Arial,sans-serif;background:#f4f6f8;margin:0;padding:16px;}"
    "h1{color:#1976d2;} .card{background:#fff;border-radius:8px;padding:16px;margin-bottom:16px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
    "</style></head><body>");
  html += "<h1>OTA Update</h1><div class='card'>";
  html += "<p>OTA firmware upload is currently only supported when connected via <b>WiFi</b>.</p>";
  html += "<p>Active interface: <b>" + activeInterface + "</b></p>";
  html += "<p>If you need to update now, force a WiFi fallback by disconnecting the Ethernet cable and rebooting.</p>";
  html += "</div></body></html>";
  return html;
}
