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

// --- VotolProtocolHandler Implementation ---

VotolProtocolHandler::VotolProtocolHandler()
    : _serial(nullptr),
      _mutex(nullptr),
      _rxLen(0),
      _wheelDiameterM(DEFAULT_WHEEL_DIAMETER_M),
      _gearRatio(DEFAULT_GEAR_RATIO),
      _lastPollMs(0)
{
    memset(&_data, 0, sizeof(_data));
    _data.gear = VotolGear::GEAR_UNKNOWN;
    _data.state = VotolState::STATE_UNKNOWN;

    // Khởi tạo Mutex bảo vệ đa luồng
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
    _data.currentBaudRate = baudRate;
    _data.pinsSwapped = false;
    _serial->begin(baudRate, SERIAL_8N1, _rxPin, _txPin);
    _serial->setRxBufferSize(256);
    
    _rxLen = 0;
    _lastPollMs = millis();
    sendHandshake(); // Gửi lệnh Handshake LDGET đánh thức IC Votol ngay khi khởi động
}

void VotolProtocolHandler::setBaudRate(uint32_t baud) {
    if (!_serial) return;
    _baudRate = baud;
    _data.currentBaudRate = baud;
    _serial->begin(baud, SERIAL_8N1, _rxPin, _txPin);
    Serial.printf("[VOTOL] Đã đổi UART1 sang %d baud (RX: %d, TX: %d)\n", baud, _rxPin, _txPin);
}

void VotolProtocolHandler::swapPins() {
    if (!_serial) return;
    int8_t temp = _rxPin;
    _rxPin = _txPin;
    _txPin = temp;
    _data.pinsSwapped = !_data.pinsSwapped;
    _serial->begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);
    Serial.printf("[VOTOL] ĐÃ ĐẢO CHÂN RX/TX! RX mới: GPIO %d, TX mới: GPIO %d\n", _rxPin, _txPin);
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
    // Gói tin Handshake LDGET đánh thức cổng Votol theo VotolAIO (24 bytes)
    static const uint8_t LDGET_CMD[24] = {
        0xC9, 0x14, 0x02, 0x4C, 0x44, 0x47, 0x45, 0x54,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x0D
    };
    _serial->write(LDGET_CMD, sizeof(LDGET_CMD));
    _data.totalBytesSent += sizeof(LDGET_CMD);
    Serial.println("[VOTOL] Đã gửi lệnh Handshake LDGET đánh thức IC Votol!");
}

void VotolProtocolHandler::sendTelemetryPoll() {
    if (!_serial) return;
    
    // Gói tin truy vấn Telemetry SHOW chuẩn VotolAIO (24 bytes, local control AA, XOR=0xDC)
    static const uint8_t SHOW_CMD[24] = {
        0xC9, 0x14, 0x02, 0x53, 0x48, 0x4F, 0x57, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00,
        0x00, 0xAA, 0x00, 0x00, 0x00, 0x00, 0xDC, 0x0D
    };
    _serial->write(SHOW_CMD, sizeof(SHOW_CMD));
    _data.totalBytesSent += sizeof(SHOW_CMD);
}

void VotolProtocolHandler::sendReadConfigPoll() {
    if (!_serial) return;

    // Gói tin đọc tham số cấu hình APP-Votol (0x3A 0x01 0x52 0x00 0x53)
    static const uint8_t READ_CMD[5] = {
        0x3A, 0x01, 0x52, 0x00, 0x53
    };
    _serial->write(READ_CMD, sizeof(READ_CMD));
}

bool VotolProtocolHandler::getSnapshot(VotolData &outData, TickType_t waitTicks) {
    if (_mutex && xSemaphoreTake(_mutex, waitTicks) == pdTRUE) {
        outData = _data;
        xSemaphoreGive(_mutex);
        return true;
    }
    return false;
}

bool VotolProtocolHandler::parseTelemetryFrame(const uint8_t* frame, size_t len) {
    if (len < 24) return false;

    // Kiểm tra Checksum XOR từ byte 0 đến byte 21
    uint8_t expectedChecksum = calcChecksum(frame, 22);
    if (frame[22] != expectedChecksum) {
        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            _data.checksumErrorsCount++;
            xSemaphoreGive(_mutex);
        }
        return false;
    }

    // 1. Điện áp pin: Byte 5 & 6 (Đơn vị 0.1V)
    uint16_t rawVolt = ((uint16_t)frame[5] << 8) | frame[6];
    float volt = (float)rawVolt / 10.0f;

    // 2. Dòng điện pin: Byte 7 & 8 (Đơn vị 0.1A có dấu)
    int16_t rawCurr = (int16_t)(((uint16_t)frame[7] << 8) | frame[8]);
    float curr = (float)rawCurr / 10.0f;

    // 3. Mã lỗi 32-bit: Byte 10, 11, 12, 13
    uint32_t fault = ((uint32_t)frame[10] << 24) |
                     ((uint32_t)frame[11] << 16) |
                     ((uint32_t)frame[12] << 8)  |
                     ((uint32_t)frame[13]);

    // 4. Vòng tua động cơ RPM: Byte 14 & 15
    int16_t rpmVal = (int16_t)(((uint16_t)frame[14] << 8) | frame[15]);

    // Tính vận tốc km/h từ RPM: Speed = (RPM * π * d * 60) / (1000 * GearRatio)
    float speedVal = 0.0f;
    if (rpmVal > 0) {
        float calcSpeed = ((float)rpmVal * 3.14159265f * _wheelDiameterM * 60.0f) / (1000.0f * _gearRatio);
        speedVal = (calcSpeed > 0.0f) ? calcSpeed : 0.0f;
    }

    // 5. Nhiệt độ: Byte 16 (IC) và Byte 17 (Động cơ), offset +50°C
    int8_t cTemp = (int8_t)((int16_t)frame[16] - 50);
    int8_t mTemp = (int8_t)((int16_t)frame[17] - 50);

    // 6. Chế độ số & Trạng thái xe: Byte 20 (Giải mã chi tiết theo chuẩn VotolAIO)
    // Byte 20: [x0->x3: L/M/H/S] | [y+8: Parked (0x08)] | [y+4: Reverse (0x04)]
    //          [x+1: Brake (0x10)] | [x+2: Lock (0x20)] | [x+4: SideStand (0x40)] | [x+8: Regen (0x80)]
    uint8_t b20 = frame[20];
    bool isParked    = (b20 & 0x08) != 0; // Bit 3 (0x08): Số P (Parked)
    bool isReverse   = (b20 & 0x04) != 0; // Bit 2 (0x04): Số lùi R (Reverse)
    bool brk         = (b20 & 0x10) != 0; // Bit 4 (0x10): Bóp phanh điện (Brake)
    bool isLocked    = (b20 & 0x20) != 0; // Bit 5 (0x20): Khóa chống trộm (Antitheft Lock)
    bool sStand      = (b20 & 0x40) != 0; // Bit 6 (0x40): Gạt chân chống (SideStand)
    bool rgn         = (b20 & 0x80) != 0; // Bit 7 (0x80): Phanh tái tạo điện tử (Regen)

    VotolGear gVal = VotolGear::GEAR_DRIVE;
    if (isParked) {
        gVal = VotolGear::GEAR_PARK; // Ưu tiên số P khi có tín hiệu nút P
    } else if (isReverse) {
        gVal = VotolGear::GEAR_REVERSE; // Ưu tiên số R khi gạt công tắc lùi
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

    // 7. Trạng thái hoạt động: Byte 21
    VotolState sVal = (frame[21] <= 7) ? static_cast<VotolState>(frame[21]) : VotolState::STATE_UNKNOWN;

    // Cập nhật an toàn với Mutex
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        _data.voltage        = volt;
        _data.current        = curr;
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

        xSemaphoreGive(_mutex);
    }

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

    // Checksum XOR từ byte 1 đến byte cuối trước checksum
    uint8_t expectedChecksum = calcChecksum(&frame[1], totalLen - 2);
    if (frame[totalLen - 1] != expectedChecksum) {
        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            _data.checksumErrorsCount++;
            xSemaphoreGive(_mutex);
        }
        return false;
    }

    uint8_t cmd = frame[2];
    if (cmd == 0x52 && dataLen >= 20) { // Đọc dữ liệu thành công ('R')
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

            xSemaphoreGive(_mutex);
        }

        if (_callback) {
            VotolData copyData;
            if (getSnapshot(copyData)) {
                _callback(copyData);
            }
        }
        return true;
    }

    return false;
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

void VotolProtocolHandler::update() {
    if (!_serial) return;

    // 1. Đọc không chặn các byte có sẵn từ UART
    while (_serial->available() > 0 && _rxLen < RX_BUFFER_SIZE) {
        uint8_t b = static_cast<uint8_t>(_serial->read());
        _rxBuffer[_rxLen++] = b;
        _data.totalBytesReceived++;

        // Cập nhật chuỗi Hex thô dạng trượt (lưu 6 byte gần nhất để app hiển thị liên tục)
        static uint8_t s_recent[6];
        static uint8_t s_count = 0;
        if (s_count < 6) {
            s_recent[s_count++] = b;
        } else {
            memmove(&s_recent[0], &s_recent[1], 5);
            s_recent[5] = b;
        }
        char hexStr[32] = "";
        for (uint8_t k = 0; k < s_count; k++) {
            char tmp[4];
            snprintf(tmp, sizeof(tmp), "%02X ", s_recent[k]);
            strncat(hexStr, tmp, sizeof(hexStr) - strlen(hexStr) - 1);
        }
        strncpy(_data.lastRawHex, hexStr, sizeof(_data.lastRawHex) - 1);
    }

    // 2. Duyệt qua buffer trượt tìm frame hợp lệ
    size_t i = 0;
    while (i < _rxLen) {
        // Gói tin Telemetry: Bắt đầu bằng 0xC0 0x14, 0xC9 0x14, hoặc 0x09 0x55 (dài 24 bytes)
        if (((_rxBuffer[i] == 0xC0 || _rxBuffer[i] == 0xC9) && i + 1 < _rxLen && _rxBuffer[i + 1] == 0x14) ||
            (_rxBuffer[i] == 0x09 && i + 1 < _rxLen && _rxBuffer[i + 1] == 0x55)) {
            
            if (_rxLen - i < 24) {
                // Chưa đủ 24 bytes, giữ lại chờ lần đọc tiếp theo
                break;
            }
            if (parseTelemetryFrame(&_rxBuffer[i], 24)) {
                i += 24;
                continue;
            } else {
                // Checksum sai hoặc frame hỏng, trượt 1 byte
                i++;
                continue;
            }
        }
        // Gói tin cấu hình APP-Votol: Bắt đầu bằng 0x3A 0x01
        else if (_rxBuffer[i] == 0x3A && i + 1 < _rxLen && _rxBuffer[i + 1] == 0x01) {
            if (_rxLen - i < 4) {
                break;
            }
            uint8_t dataLen = _rxBuffer[i + 3];
            size_t totalLen = 4 + dataLen + 1;
            if (_rxLen - i < totalLen) {
                break;
            }
            if (parseAppVotolFrame(&_rxBuffer[i], totalLen)) {
                i += totalLen;
                continue;
            } else {
                i++;
                continue;
            }
        }
        else {
            // Không khớp header nào, bỏ 1 byte rác
            i++;
        }
    }

    // 3. Dịch các byte còn lại về đầu buffer
    if (i > 0) {
        if (i < _rxLen) {
            memmove(_rxBuffer, &_rxBuffer[i], _rxLen - i);
            _rxLen -= i;
        } else {
            _rxLen = 0;
        }
    }

    // 4. Gửi lệnh truy vấn (Chỉ gửi nếu KHÔNG bật chế độ Passive Listen an toàn)
    uint32_t now = millis();
#if !VOTOL_PASSIVE_MODE
    if (now - _lastPollMs >= VOTOL_POLL_INTERVAL_MS) {
        _lastPollMs = now;
        sendTelemetryPoll();
    }
#endif

    // 5. Kiểm tra timeout mất tín hiệu
    checkConnectionTimeout();
}
