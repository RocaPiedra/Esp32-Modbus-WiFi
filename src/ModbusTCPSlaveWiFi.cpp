#include "ModbusTCPSlaveWiFi.h"

ModbusTCPSlaveWiFi::ModbusTCPSlaveWiFi(uint16_t port) : ModbusSlave(0), _server(port), _lastRequestTime(0) {
    setTimeout(10000);
    if (Serial) {
        Serial.printf("[MBWIFI] Constructed on port %u, timeout=%lu ms\n", port, _timeout);
    }
}

void ModbusTCPSlaveWiFi::begin() {
    _next = nullptr;
    setState(Idle);
    _server.begin();
    if (Serial) {
        Serial.printf("[MBWIFI] begin() called, timeout=%lu ms\n", _timeout);
    }
}

void ModbusTCPSlaveWiFi::update() {
    if (getState() == Idle) {
        // On ESP32, WiFiServer::available() typically returns only a *new*
        // connection. To support persistent Modbus TCP, keep using the current
        // client as long as it is connected; only accept a new client when the
        // old one is gone.
        bool isNewClient = false;
        if (!_currentClient || !_currentClient.connected()) {
            _currentClient = _server.available();
            isNewClient = (bool)_currentClient;
            if (_currentClient) {
                _currentClient.setNoDelay(true);
            }
        }

        if (_currentClient && _currentClient.connected() && _currentClient.available()) {
            _next = _adu;
            _lastRequestTime = millis();
            setState(Receiving);
            if (Serial) {
                Serial.printf("[MBWIFI] Client %s, state=Receiving, connected=%d, available=%d, nodelay=1\n",
                              isNewClient ? "accepted" : "reused",
                              (int)_currentClient.connected(), (int)_currentClient.available());
            }
        }
    }

    if (getState() == Receiving) {
        if (!_currentClient.connected()) {
            if (Serial) {
                Serial.println("[MBWIFI] Client disconnected while Receiving, returning to Idle");
            }
            setState(Idle);
            return;
        }
        if (receiveRequest()) {
            uint16_t reqLen = _next - _adu;
            if (Serial) {
                Serial.printf("[MBWIFI] Request received, len=%u, fc=%02X\n", reqLen, _adu[7]);
                Serial.print("[MBWIFI] Request bytes: ");
                for (uint16_t i = 0; i < reqLen; i++) {
                    Serial.printf("%02X ", _adu[i]);
                }
                Serial.println();
            }

            uint8_t err = processRequest(_adu + MODBUS_TCP_HEADER_SIZE);
            if (err) {
                if (Serial) {
                    Serial.printf("[MBWIFI] processRequest error=%02X\n", err);
                }
                _next = _adu + MODBUS_TCP_HEADER_SIZE;
                *_next++ |= 0x80;
                *_next++ = err;
            }

            bool sent = sendResponse();
            if (Serial) {
                Serial.printf("[MBWIFI] sendResponse returned %s, client.connected=%d\n",
                              sent ? "OK" : "FAIL", (int)_currentClient.connected());
            }
            setState(Idle);
        }
    }
}

bool ModbusTCPSlaveWiFi::receiveRequest() {
    if (_currentClient.available()) {
        uint16_t requestLen = _next - _adu;
        int bytesReadThisCall = 0;

        do {
            if (requestLen >= MODBUS_TCP_ADU_SIZE) {
                if (Serial) {
                    Serial.println("[MBWIFI] Overflow error, dropping to Idle");
                }
                setState(Idle);
                break;
            }

            int b = _currentClient.read();
            if (b < 0) {
                if (Serial) {
                    Serial.println("[MBWIFI] read() returned -1 despite available()");
                }
                break;
            }
            *_next++ = (uint8_t)b;
            ++requestLen;
            ++bytesReadThisCall;

            if (requestLen >= MODBUS_TCP_HEADER_SIZE) {
                uint16_t packetLen = (_adu[4] << 8) + _adu[5];
                if (packetLen == requestLen - 6) {
                    if (_adu[2] != 0x00 && _adu[3] != 0x00) {
                        if (Serial) {
                            Serial.printf("[MBWIFI] Invalid protocol ID %02X%02X, drop\n", _adu[2], _adu[3]);
                        }
                        setState(Idle);
                        break;
                    }
                    if (Serial) {
                        Serial.printf("[MBWIFI] Complete packet, packetLen=%u, totalRead=%u\n", packetLen, requestLen);
                    }
                    return true;
                }
            }
        } while (_currentClient.available());

        _lastRequestTime = millis();
        if (Serial && bytesReadThisCall > 0) {
            Serial.printf("[MBWIFI] Read %d bytes this call, total requestLen=%u\n", bytesReadThisCall, requestLen);
        }
    } else if (millis() - _lastRequestTime > _timeout) {
        if (Serial) {
            Serial.printf("[MBWIFI] Idle timeout after %lu ms, returning to Idle (not closing socket)\n",
                          millis() - _lastRequestTime);
        }
        setState(Idle);
        return false;
    }

    return false;
}

bool ModbusTCPSlaveWiFi::sendResponse() {
    if (!_currentClient.connected()) {
        if (Serial) {
            Serial.println("[MBWIFI] sendResponse: client not connected");
        }
        return false;
    }

    uint16_t tlen = _next - _adu;
    uint16_t len = tlen - 6;
    _adu[4] = len >> 8;
    _adu[5] = len;

    if (Serial) {
        Serial.printf("[MBWIFI] Sending response, tlen=%u, pduLen=%u\n", tlen, len);
        Serial.print("[MBWIFI] Response bytes: ");
        for (uint16_t i = 0; i < tlen; i++) {
            Serial.printf("%02X ", _adu[i]);
        }
        Serial.println();
    }

    size_t written = _currentClient.write(_adu, tlen);
    if (Serial) {
        Serial.printf("[MBWIFI] write() returned %u (expected %u)\n", (unsigned)written, (unsigned)tlen);
    }
    if (written == tlen) {
        _currentClient.flush();
        if (Serial) {
            Serial.println("[MBWIFI] flush() completed");
        }
    }
    return written == tlen;
}
