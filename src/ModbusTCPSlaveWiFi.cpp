#include "ModbusTCPSlaveWiFi.h"

ModbusTCPSlaveWiFi::ModbusTCPSlaveWiFi(uint16_t port) : ModbusSlave(0), _server(port) {
}

void ModbusTCPSlaveWiFi::begin() {
    _next = nullptr;
    setState(Idle);
    _server.begin();
}

void ModbusTCPSlaveWiFi::update() {
    if (getState() == Idle) {
        _currentClient = _server.available();
        if (_currentClient) {
            _next = _adu;
            setState(Receiving);
        }
    }

    if (getState() == Receiving) {
        if (receiveRequest()) {
            uint8_t err = processRequest(_adu + MODBUS_TCP_HEADER_SIZE);
            if (err) {
                _next = _adu + MODBUS_TCP_HEADER_SIZE;
                *_next++ |= 0x80;
                *_next++ = err;
            }

            sendResponse();
            setState(Idle);
        }
    }
}

bool ModbusTCPSlaveWiFi::receiveRequest() {
    if (_currentClient.available()) {
        uint16_t requestLen = _next - _adu;

        do {
            if (requestLen >= MODBUS_TCP_ADU_SIZE) {
                setState(Idle);
                break;
            }

            *_next++ = _currentClient.read();
            ++requestLen;

            if (requestLen >= MODBUS_TCP_HEADER_SIZE) {
                uint16_t packetLen = (_adu[4] << 8) + _adu[5];
                if (packetLen == requestLen - 6) {
                    if (_adu[2] != 0x00 && _adu[3] != 0x00) {
                        setState(Idle);
                        break;
                    }
                    return true;
                }
            }
        } while (_currentClient.available());

        _lastRequestTime = millis();
    } else if (millis() - _lastRequestTime > _timeout) {
        setState(Idle);
        return false;
    }

    return false;
}

bool ModbusTCPSlaveWiFi::sendResponse() {
    if (!_currentClient.connected()) {
        return false;
    }

    uint16_t tlen = _next - _adu;
    uint16_t len = tlen - 6;
    _adu[4] = len >> 8;
    _adu[5] = len;

    return _currentClient.write(_adu, tlen) == tlen;
}
