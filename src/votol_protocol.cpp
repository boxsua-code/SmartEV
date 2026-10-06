#include "votol_protocol.h"
#include <cmath>

// --- Helper Functions cho VotolData ---

const char* VotolData::getGearString() const {
    switch (gear) {
        case VotolGear::GEAR_PARK:    return "P";
        case VotolGear::GEAR_ECO:     return "ECO";
        case VotolGear::GEAR_DRIVE:   return "D";
        case VotolGear::GEAR_SPORT:   return "SPORT";
        case VotolGear::GEAR_REVERSE: return "R";
        default:                      return "--";
    }
}

const char* VotolData::getStateString() const {
    switch (state) {
        case VotolState::STATE_IDLE:  return "IDLE";
        case VotolState::STATE_INIT:  return "INIT";
        case VotolState::STATE_START: return "START";
        case VotolState::STATE_RUN:   return "RUN";
        case VotolState::STATE_STOP:  return "STOP";
        case VotolState::STATE_BRAKE: return "BRAKE";
        case VotolState::STATE_WAIT:  return "WAIT";
        case VotolState::STATE_FAULT: return "FAULT";
        default:                      return "UNKNOWN";
    }
}

String VotolData::getFaultString() const {
    if (faultCode == 0) {
        return "Bình thường (Không có lỗi)";
    }

    String result = "";
    if (faultCode & VotolFault::EBRAKE_ON)           result += "[Phanh điện] ";
    if (faultCode & VotolFault::OVER_CURRENT_HW)     result += "[Quá dòng phần cứng] ";
    if (faultCode & VotolFault::UNDER_VOLTAGE)       result += "[Tụt áp pin] ";
    if (faultCode & VotolFault::THROTTLE_HALL_ERR)   result += "[Lỗi tay ga] ";
    if (faultCode & VotolFault::OVER_VOLTAGE)        result += "[Quá áp pin] ";
    if (faultCode & VotolFault::MCU_ERROR)           result += "[Lỗi chip điều khiển] ";
    if (faultCode & VotolFault::MOTOR_BLOCK)         result += "[Động cơ bị kẹt/bó] ";
    if (faultCode & VotolFault::FOOTPLATE_ERR)       result += "[Lỗi chân ga] ";
    if (faultCode & VotolFault::SPEED_RUNAWAY)       result += "[Mất kiểm soát ga] ";
    if (faultCode & VotolFault::EEPROM_WRITING)      result += "[Đang lưu EEPROM] ";
    if (faultCode & VotolFault::STARTUP_FAILURE)     result += "[Lỗi tự kiểm tra nguồn] ";
    if (faultCode & VotolFault::CONTROLLER_OVERHEAT) result += "[Quá nhiệt IC] ";
    if (faultCode & VotolFault::OVER_CURRENT_SW)     result += "[Quá dòng phần mềm] ";
    if (faultCode & VotolFault::THROTTLE_PEDAL_ERR)  result += "[Lỗi bướm ga] ";
    if (faultCode & VotolFault::CURRENT_SENSOR1_ERR) result += "[Lỗi cảm biến dòng 1] ";
    if (faultCode & VotolFault::CURRENT_SENSOR2_ERR) result += "[Lỗi cảm biến dòng 2] ";
    if (faultCode & VotolFault::BRAKE_FAILURE)       result += "[Hỏng cảm biến phanh] ";
    if (faultCode & VotolFault::MOTOR_HALL_ERR)      result += "[Lỗi mắt Hall động cơ] ";
    if (faultCode & VotolFault::MOSFET_DRIVER_ERR)   result += "[Lỗi kích MOSFET] ";
    if (faultCode & VotolFault::MOSFET_HIGH_SHORT)   result += "[Chập MOSFET vế cao] ";
    if (faultCode & VotolFault::PHASE_WIRE_OPEN)     result += "[Hở dây pha] ";
    if (faultCode & VotolFault::PHASE_WIRE_SHORT)    result += "[Chập dây pha] ";
    if (faultCode & VotolFault::MCU_CHIP_ERROR)      result += "[Lỗi MCU phần cứng] ";
    if (faultCode & VotolFault::PRECHARGE_ERROR)     result += "[Lỗi nạp trước Precharge] ";
    if (faultCode & VotolFault::MOTOR_OVERHEAT)      result += "[Động cơ quá nhiệt] ";
    if (faultCode & VotolFault::SOC_ZERO_ERROR)      result += "[Pin cạn 0%] ";

    return result;
}

// ============================================================================
// 3 GÓI TIN THĂM DÒ TELEMETRY XOAY VÒNG (24 BYTES)
// ============================================================================
static const uint8_t VOTOL_TELEMETRY_POLLS[3][24] = {
    {0xC9, 0x14, 0x02, 0x53, 0x48, 0x4F, 0x57, 0x00, 0x00, 0x00, 0x00, 0x00,
     0xAA, 0x10, 0x41, 0x00, 0x03, 0xAA, 0x05, 0xA7, 0x00, 0x7A, 0x56, 0x0D},
    {0xC9, 0x14, 0x02, 0x53, 0x48, 0x4F, 0x57, 0x00, 0x00, 0x00, 0x00, 0x00,
     0xAA, 0x10, 0x41, 0x10, 0x03, 0xAA, 0x05, 0xA7, 0x00, 0x7A, 0x46, 0x0D},
    {0xC9, 0x14, 0x02, 0x53, 0x48, 0x4F, 0x57, 0x00, 0x00, 0x00, 0x00, 0x00,
     0xAA, 0x10, 0x41, 0x08, 0x03, 0xAA, 0x05, 0xA7, 0x00, 0x7A, 0x5E, 0x0D}
};

// --- VotolProtocolHandler Implementation ---

VotolProtocolHandler::VotolProtocolHandler()
    : _serial(nullptr),
      _rxPin(PIN_VOTOL_RX),
      _txPin(PIN_VOTOL_TX),
      _baudRate(VOTOL_BAUD_RATE),
      _mutex(nullptr),
      _uartFrameLength(0),
      _uartRxSampleLength(0),
      _uartRxIntervalBytes(0),
      _uartRxTotalBytes(0),
      _uartRxLastByteTime(0),
      _uartValidFrameCount(0),
      _uartLastValidFrameTime(0),
      _uartPollingEnabled(true),
      _uartPollStep(0),
      _uartNextPollStepAt(0),
      _uartTxRequestCount(0),
      _wheelDiameterM(DEFAULT_WHEEL_DIAMETER_M),
      _gearRatio(DEFAULT_GEAR_RATIO),
      _lastReportMs(0),
      _autoProbeEnabled(false),
      _probeIndex(0),
      _lastProbeSwitchMs(0)
{
    memset(&_data, 0, sizeof(_data));
    memset(_uartFrameBuffer, 0, sizeof(_uartFrameBuffer));
    memset(_uartRxSample, 0, sizeof(_uartRxSample));
    memset(_uartLastFrameHex, 0, sizeof(_uartLastFrameHex));

    _data.gear = VotolGear::GEAR_UNKNOWN;
    _data.state = VotolState::STATE_UNKNOWN;
    _data.currentBaudRate = VOTOL_BAUD_RATE;

    _mutex = xSemaphoreCreateMutex();
}

VotolProtocolHandler::~VotolProtocolHandler() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

void VotolProtocolHandler::begin(HardwareSerial &serialPort, int8_t rxPin, int8_t txPin, uint32_t baudRate) {
    _serial = &serialPort;
    _rxPin = rxPin;
    _txPin = txPin;
    _baudRate = baudRate;
    _probeIndex = 0;
    _lastProbeSwitchMs = millis();
    _lastReportMs = millis();

    applyUartConfig(_rxPin, _txPin, _baudRate);
}

bool VotolProtocolHandler::applyUartConfig(int8_t rxPin, int8_t txPin, uint32_t baudRate) {
    if (!_serial || rxPin == txPin || baudRate == 0) {
        return false;
    }

    _serial->end();
    _serial->setRxBufferSize(256);
    _serial->begin(baudRate, SERIAL_8N1, rxPin, txPin);

    _rxPin = rxPin;
    _txPin = txPin;
    _baudRate = baudRate;
    _data.currentBaudRate = baudRate;
    _data.pinsSwapped = (rxPin != PIN_VOTOL_RX);

    _uartFrameLength = 0;
    _uartRxIntervalBytes = 0;
    _uartRxSampleLength = 0;
    _uartNextPollStepAt = millis();

    Serial.printf("[VOTOL UART] Cấu hình: RX=GPIO%d, TX=GPIO%d, Baud=%lu 8N1\n",
                  _rxPin, _txPin, (unsigned long)_baudRate);

    sendHandshake(); // Gửi lệnh LDGET đánh thức IC Votol
    return true;
}

void VotolProtocolHandler::setBaudRate(uint32_t baud) {
    if (baud == 0) return;
    _data.autoProbeLocked = true; // Khóa cấu hình do người dùng chỉ định
    applyUartConfig(_rxPin, _txPin, baud);
}

void VotolProtocolHandler::swapPins() {
    _data.autoProbeLocked = true; // Khóa cấu hình do người dùng chỉ định
    int8_t temp = _rxPin;
    applyUartConfig(_txPin, temp, _baudRate);
}

void VotolProtocolHandler::setWheelParams(float wheelDiameterM, float gearRatio) {
    if (wheelDiameterM > 0.05f) _wheelDiameterM = wheelDiameterM;
    if (gearRatio > 0.01f) _gearRatio = gearRatio;
}

uint8_t VotolProtocolHandler::calcChecksum(const uint8_t* buffer, size_t length) {
    uint8_t cs = 0;
    for (size_t i = 0; i < length; i++) {
        cs ^= buffer[i];
    }
    return cs;
}

void VotolProtocolHandler::sendHandshake() {
    if (!_serial) return;
    // Gói tin Handshake LDGET đánh thức cổng Votol (24 bytes)
    static const uint8_t LDGET_CMD[24] = {
        0xC9, 0x14, 0x02, 0x4C, 0x44, 0x47, 0x45, 0x54,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x0D
    };
    _serial->write(LDGET_CMD, sizeof(LDGET_CMD));
    _serial->flush();
    _data.totalBytesSent += sizeof(LDGET_CMD);
    Serial.println("[VOTOL] Đã gửi lệnh Handshake LDGET đánh thức IC Votol!");
}

void VotolProtocolHandler::sendTelemetryPoll() {
    if (!_serial || !_uartPollingEnabled) return;
    if ((int32_t)(millis() - _uartNextPollStepAt) < 0) return;

    // Gửi frame thăm dò hiện tại (xoay vòng 3 gói)
    const uint8_t* cmd = VOTOL_TELEMETRY_POLLS[_uartPollStep];
    _serial->write(cmd, 24);
    _serial->flush();
    ++_uartTxRequestCount;
    _data.totalBytesSent += 24;

    // Quản lý timing gửi: 50ms -> 50ms -> 350ms
    if (_uartPollStep < 2) {
        ++_uartPollStep;
        _uartNextPollStepAt = millis() + 50;
    } else {
        _uartPollStep = 0;
        _uartNextPollStepAt = millis() + 350;
    }
}

void VotolProtocolHandler::sendReadConfigPoll() {
    if (!_serial) return;
    // Gói tin đọc tham số cấu hình APP-Votol (0x3A 0x01 0x52 0x00 0x53)
    static const uint8_t READ_CMD[5] = {
        0x3A, 0x01, 0x52, 0x00, 0x53
    };
    _serial->write(READ_CMD, sizeof(READ_CMD));
    _serial->flush();
}

bool VotolProtocolHandler::getSnapshot(VotolData &outData, TickType_t waitTicks) {
    if (_mutex && xSemaphoreTake(_mutex, waitTicks) == pdTRUE) {
        outData = _data;
        xSemaphoreGive(_mutex);
        return true;
    }
    return false;
}

bool VotolProtocolHandler::parseVotolTelemetryFrame(const uint8_t* frame, size_t length) {
    if (length < 24) return false;

    // Kiểm tra header: C0 14 0D 59 42 (hoặc C0 14)
    if (frame[0] != 0xC0 || frame[1] != 0x14) {
        return false;
    }

    // Kiểm tra Checksum XOR từ byte 0 đến byte 21
    uint8_t checksum = 0;
    for (size_t i = 0; i < 22; ++i) {
        checksum ^= frame[i];
    }

    if (checksum != frame[22]) {
        Serial.printf("[VOTOL] checksum sai: calc=%02X packet=%02X\n", checksum, frame[22]);
        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            _data.checksumErrorsCount++;
            xSemaphoreGive(_mutex);
        }
        return false;
    }

    if (frame[23] != 0x0D) {
        Serial.printf("[VOTOL] byte kết thúc sai: %02X\n", frame[23]);
        return false;
    }

    // 1. Điện áp pin: Byte 5 & 6 (Đơn vị 0.1V)
    const uint16_t batteryRaw = ((uint16_t)frame[5] << 8) | uint16_t(frame[6]);
    const uint16_t currentRaw = ((uint16_t)frame[7] << 8) | uint16_t(frame[8]);
    const uint16_t rpmRaw = ((uint16_t)frame[14] << 8) | uint16_t(frame[15]);

    const float decodedVoltage = batteryRaw / 10.0f;
    const float decodedCurrent = (int16_t)currentRaw / 10.0f;

    // 2. Mã lỗi 32-bit: Byte 10, 11, 12, 13
    uint32_t fault = ((uint32_t)frame[10] << 24) |
                     ((uint32_t)frame[11] << 16) |
                     ((uint32_t)frame[12] << 8)  |
                     ((uint32_t)frame[13]);

    // 3. Vòng tua động cơ RPM & Tốc độ di chuyển km/h
    int16_t rpmVal = (int16_t)rpmRaw;
    float speedVal = 0.0f;
    if (rpmVal > 0) {
        speedVal = ((float)rpmVal * 3.14159265f * _wheelDiameterM * 60.0f) / (1000.0f * _gearRatio);
        if (speedVal < 0.0f) speedVal = 0.0f;
    }

    // 4. Nhiệt độ: Byte 16 (IC) và Byte 17 (Động cơ), offset +50°C
    int8_t cTemp = (int8_t)((int16_t)frame[16] - 50);
    int8_t mTemp = (int8_t)((int16_t)frame[17] - 50);

    // 5. Giải mã Byte 20: Tín hiệu xe & Cấp số (Chuẩn VotolAIO)
    uint8_t b20 = frame[20];
    bool isParked  = (b20 & 0x08) != 0 || (b20 & 0x20) != 0; // P
    bool isReverse = (b20 & 0x04) != 0 || (b20 & 0x10) != 0; // R
    bool brk       = (b20 & 0x10) != 0 || (b20 & 0x08) != 0; // Brake
    bool isLocked  = (b20 & 0x20) != 0;                      // Lock
    bool sStand    = (b20 & 0x40) != 0;                      // SideStand
    bool rgn       = (b20 & 0x80) != 0;                      // Regen

    VotolGear gVal = VotolGear::GEAR_DRIVE;
    if (isParked) {
        gVal = VotolGear::GEAR_PARK;
    } else if (isReverse) {
        gVal = VotolGear::GEAR_REVERSE;
    } else {
        uint8_t subGear = b20 & 0x03;
        switch (subGear) {
            case 0x00: gVal = VotolGear::GEAR_ECO; break;   // L: Low / Eco
            case 0x01: gVal = VotolGear::GEAR_DRIVE; break; // M: Mid / Drive
            case 0x02: gVal = VotolGear::GEAR_SPORT; break; // H: High / Sport
            case 0x03: gVal = VotolGear::GEAR_SPORT; break; // S: Super Sport
            default:   gVal = VotolGear::GEAR_DRIVE; break;
        }
    }

    // 6. Trạng thái hoạt động: Byte 21
    VotolState sVal = (frame[21] <= 7) ? static_cast<VotolState>(frame[21]) : VotolState::STATE_UNKNOWN;

    // Cập nhật an toàn với Mutex
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        _data.voltage        = decodedVoltage;
        _data.current        = decodedCurrent;
        _data.faultCode      = fault;
        _data.rpm            = rpmVal;
        _data.speedKmh       = speedVal;
        _data.controllerTemp = cTemp;
        _data.motorTemp      = mTemp;
        _data.gear           = gVal;
        _data.state          = sVal;
        _data.sideStand      = sStand;
        _data.brake          = brk;
        _data.regen          = rgn;
        _data.parked         = isParked;
        _data.reverse        = isReverse;
        _data.locked         = isLocked;

        _data.isConnected    = true;
        _data.lastReceivedMs = millis();
        _data.validPacketsCount++;
        _data.autoProbeLocked = true; // Khóa cấu hình chuẩn

        xSemaphoreGive(_mutex);
    }

    Serial.printf("[VOTOL] accepted: V=%.1fV, I=%.1fA; RPM=%d, Spd=%.1fkm/h, Gear=%s, Tctrl=%d, Tmotor=%d, status=%u\n",
                  decodedVoltage, decodedCurrent, rpmVal, speedVal,
                  _data.getGearString(), cTemp, mTemp, frame[21]);

    if (_callback) {
        VotolData copyData;
        if (getSnapshot(copyData)) {
            _callback(copyData);
        }
    }
    return true;
}

bool VotolProtocolHandler::parseAppVotolFrame(const uint8_t* frame, size_t len) {
    if (len < 5) return false;
    uint8_t dataLen = frame[3];
    size_t totalLen = 4 + dataLen + 1;
    if (len < totalLen) return false;

    uint8_t expectedChecksum = calcChecksum(&frame[1], totalLen - 2);
    if (frame[totalLen - 1] != expectedChecksum) {
        return false;
    }

    uint8_t cmd = frame[2];
    if (cmd == 0x52 && dataLen >= 20) { // Read Response ('R')
        const uint8_t* data = &frame[4];
        uint16_t swVer  = ((uint16_t)data[0] << 8) | data[1];
        uint16_t hwVer  = ((uint16_t)data[2] << 8) | data[3];
        uint8_t br      = data[4];
        uint8_t md      = data[5];
        float vVolt     = (float)(((uint16_t)data[6] << 8) | data[7]);
        uint16_t busCur = ((uint16_t)data[16] << 8) | data[17];
        uint16_t maxRpm = (dataLen >= 50) ? (((uint16_t)data[48] << 8) | data[49]) : 0;

        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            _data.softwareVersion  = swVer;
            _data.hardwareVersion  = hwVer;
            _data.brand            = br;
            _data.model            = md;
            if (!_data.isConnected) {
                _data.voltage      = vVolt;
            }
            _data.busCurrentLimit  = busCur;
            _data.maxRpmLimit      = maxRpm;
            _data.isConnected      = true;
            _data.lastReceivedMs   = millis();
            _data.validPacketsCount++;
            _data.autoProbeLocked  = true;
            xSemaphoreGive(_mutex);
        }
        return true;
    }
    return false;
}

void VotolProtocolHandler::processVotolUartData() {
    if (!_serial) return;

    // Header chuẩn Votol: 5 bytes
    static const uint8_t frameHeader[] = {0xC0, 0x14, 0x0D, 0x59, 0x42};

    while (_serial->available() > 0) {
        const int value = _serial->read();
        if (value < 0) break;

        const uint8_t byteValue = (uint8_t)value;
        ++_uartRxIntervalBytes;
        ++_uartRxTotalBytes;
        _data.totalBytesReceived++;
        _uartRxLastByteTime = millis();

        if (_uartRxSampleLength < sizeof(_uartRxSample)) {
            _uartRxSample[_uartRxSampleLength++] = byteValue;
        }

        // Cập nhật chuỗi Hex thô dạng trượt
        static uint8_t s_recent[8];
        static uint8_t s_count = 0;
        if (s_count < 8) {
            s_recent[s_count++] = byteValue;
        } else {
            memmove(&s_recent[0], &s_recent[1], 7);
            s_recent[7] = byteValue;
        }
        char hexStr[32] = "";
        for (uint8_t k = 0; k < s_count; k++) {
            char tmp[4];
            snprintf(tmp, sizeof(tmp), "%02X ", s_recent[k]);
            strncat(hexStr, tmp, sizeof(hexStr) - strlen(hexStr) - 1);
        }
        strncpy(_data.lastRawHex, hexStr, sizeof(_data.lastRawHex) - 1);

        // Khớp Header
        if (_uartFrameLength < sizeof(frameHeader)) {
            if (byteValue == frameHeader[_uartFrameLength]) {
                _uartFrameBuffer[_uartFrameLength++] = byteValue;
            } else {
                _uartFrameLength = (byteValue == frameHeader[0]) ? 1 : 0;
                if (_uartFrameLength == 1) {
                    _uartFrameBuffer[0] = byteValue;
                }
            }
            continue;
        }

        // Nhận phần thân frame
        _uartFrameBuffer[_uartFrameLength++] = byteValue;
        if (_uartFrameLength == sizeof(_uartFrameBuffer)) {
            if (parseVotolTelemetryFrame(_uartFrameBuffer, sizeof(_uartFrameBuffer))) {
                ++_uartValidFrameCount;
                _uartLastValidFrameTime = millis();

                size_t offset = 0;
                for (size_t i = 0; i < sizeof(_uartFrameBuffer); ++i) {
                    const int written = snprintf(_uartLastFrameHex + offset,
                                                 sizeof(_uartLastFrameHex) - offset,
                                                 i == 0 ? "%02X" : " %02X",
                                                 _uartFrameBuffer[i]);
                    if (written <= 0 || (size_t)written >= sizeof(_uartLastFrameHex) - offset) {
                        _uartLastFrameHex[0] = '\0';
                        break;
                    }
                    offset += (size_t)written;
                }
                strncpy(_data.lastFrameHex, _uartLastFrameHex, sizeof(_data.lastFrameHex) - 1);
            }
            _uartFrameLength = 0;
        }
    }
}

void VotolProtocolHandler::reportUartRx() {
    uint32_t now = millis();
    if (now - _lastReportMs < 1000) return;

    _data.bytesPerSecond = _uartRxIntervalBytes;

    char sampleHex[49] = "";
    size_t sampleHexLength = 0;
    for (size_t index = 0; index < _uartRxSampleLength; ++index) {
        const int written = snprintf(sampleHex + sampleHexLength,
                                     sizeof(sampleHex) - sampleHexLength,
                                     index == 0 ? "%02X" : " %02X", _uartRxSample[index]);
        if (written <= 0 || (size_t)written >= sizeof(sampleHex) - sampleHexLength) break;
        sampleHexLength += (size_t)written;
    }

    Serial.printf("[UART RX GPIO%d/TX%d @ %lu] %lu B/s (Tổng: %luB, Valid: %lu), sample: %s\n",
                  _rxPin, _txPin, (unsigned long)_baudRate,
                  (unsigned long)_uartRxIntervalBytes, (unsigned long)_uartRxTotalBytes,
                  (unsigned long)_uartValidFrameCount,
                  sampleHex);

    _uartRxIntervalBytes = 0;
    _uartRxSampleLength = 0;
    _lastReportMs = now;
}

void VotolProtocolHandler::checkConnectionTimeout() {
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        if (_data.isConnected && (millis() - _data.lastReceivedMs > VOTOL_TIMEOUT_MS)) {
            _data.isConnected = false;
            _data.speedKmh = 0.0f;
            _data.rpm = 0;
            _data.current = 0.0f;
        }
        xSemaphoreGive(_mutex);
    }
}

void VotolProtocolHandler::checkAutoProbe() {
    if (!_autoProbeEnabled || _data.autoProbeLocked) return;
    if (_data.isConnected) {
        _data.autoProbeLocked = true;
        Serial.printf("\n[VOTOL SCAN] >>> KHÓA CẤU HÌNH THÀNH CÔNG: RX=GPIO%d, TX=GPIO%d @ %lu baud! <<<\n\n",
                      _rxPin, _txPin, (unsigned long)_baudRate);
        return;
    }

    uint32_t now = millis();
    // Thử mỗi cấu hình trong 6 giây ở đúng 9600 baud chuẩn của xe
    if (now - _lastProbeSwitchMs >= 6000) {
        _lastProbeSwitchMs = now;
        _probeIndex = (_probeIndex + 1) % 2; // Chỉ đảo 2 chiều chân ở 9600 baud

        int8_t nextRx = (_probeIndex == 0) ? 16 : 17;
        int8_t nextTx = (_probeIndex == 0) ? 17 : 16;
        uint32_t nextBaud = 9600; // Bắt buộc cố định 9600 baud theo yêu cầu xe

        Serial.printf("\n[VOTOL SCAN] Đang thử cấu hình #%u: RX=GPIO%d, TX=GPIO%d @ %lu baud...\n",
                      _probeIndex + 1, nextRx, nextTx, (unsigned long)nextBaud);
        applyUartConfig(nextRx, nextTx, nextBaud);
    }
}

void VotolProtocolHandler::update() {
    if (!_serial) return;

    // 1. Gửi lệnh thăm dò Telemetry xoay vòng 3 gói
#if !VOTOL_PASSIVE_MODE
    sendTelemetryPoll();
#endif

    // 2. Nhận và gom frame UART
    processVotolUartData();

    // 3. In log báo cáo UART RX ra Serial Monitor (1 lần / giây)
    reportUartRx();

    // 4. Tự động quét dò cổng nếu chưa bắt được tín hiệu
    checkAutoProbe();

    // 5. Kiểm tra timeout mất kết nối
    checkConnectionTimeout();
}
