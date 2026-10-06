#include "ant_bms_protocol.h"
#include <cstring>

// --- Helper Functions cho ANTBMSData ---

float ANTBMSData::getBatteryTemp() const {
    float maxT = temperatures[2];
    for (int i = 3; i < ANT_BMS_MAX_TEMPS; i++) {
        if (temperatures[i] > maxT && temperatures[i] < 120.0f) {
            maxT = temperatures[i];
        }
    }
    return maxT;
}

float ANTBMSData::getMosfetTemp() const {
    return temperatures[0];
}

// --- AntBmsProtocolHandler Implementation ---

AntBmsProtocolHandler::AntBmsProtocolHandler()
    : _serial(nullptr),
      _mutex(nullptr),
      _rxLen(0),
      _lastPollMs(0)
{
    memset(&_data, 0, sizeof(_data));
    _mutex = xSemaphoreCreateMutex();
}

AntBmsProtocolHandler::~AntBmsProtocolHandler() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

void AntBmsProtocolHandler::begin(HardwareSerial &serialPort, int8_t rxPin, int8_t txPin, uint32_t baudRate) {
    _serial = &serialPort;
    // Khởi tạo UART2 với RX buffer 512 bytes
    _serial->begin(baudRate, SERIAL_8N1, rxPin, txPin);
    _serial->setRxBufferSize(512);

    _rxLen = 0;
    _lastPollMs = millis();
}

uint16_t AntBmsProtocolHandler::calcChecksum(const uint8_t* buffer, size_t length) {
    if (length < ANT_BMS_FRAME_LEN) return 0;
    uint16_t sum = 0;
    // Tính tổng từ byte index 4 đến byte index 137 (134 bytes)
    for (size_t i = 4; i < 138; i++) {
        sum += buffer[i];
    }
    return sum;
}

void AntBmsProtocolHandler::sendPollQuery() {
    if (!_serial) return;
    // Lệnh truy vấn dữ liệu tiêu chuẩn từ BMS ANT
    static const uint8_t POLL_CMD[6] = { 0x5A, 0x5A, 0x00, 0x00, 0x00, 0x00 };
    _serial->write(POLL_CMD, sizeof(POLL_CMD));
}

bool AntBmsProtocolHandler::getSnapshot(ANTBMSData &outData, TickType_t waitTicks) {
    if (_mutex && xSemaphoreTake(_mutex, waitTicks) == pdTRUE) {
        outData = _data;
        xSemaphoreGive(_mutex);
        return true;
    }
    return false;
}

bool AntBmsProtocolHandler::parseFrame(const uint8_t* frame, size_t len) {
    if (len < ANT_BMS_FRAME_LEN) return false;

    // 1. Kiểm tra Checksum (2 bytes cuối: Big-Endian)
    uint16_t expectedCrc = ((uint16_t)frame[138] << 8) | frame[139];
    uint16_t computedCrc = calcChecksum(frame, ANT_BMS_FRAME_LEN);

    if (expectedCrc != computedCrc) {
        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            _data.checksumErrorsCount++;
            xSemaphoreGive(_mutex);
        }
        return false;
    }

    // 2. Helpers đọc số nguyên Big-Endian
    auto get16 = [&](size_t idx) -> uint16_t {
        return ((uint16_t)frame[idx] << 8) | frame[idx + 1];
    };
    auto get32 = [&](size_t idx) -> uint32_t {
        return ((uint32_t)get16(idx) << 16) | get16(idx + 2);
    };

    // 3. Trích xuất các trường dữ liệu
    float vTot = (float)get16(4) * 0.1f;
    uint8_t cells = frame[123];
    if (cells > ANT_BMS_MAX_CELLS) cells = ANT_BMS_MAX_CELLS;

    float cellV[ANT_BMS_MAX_CELLS] = {0};
    for (uint8_t i = 0; i < ANT_BMS_MAX_CELLS; i++) {
        cellV[i] = (float)get16(6 + i * 2) * 0.001f;
    }

    // Dòng điện: Byte 70..73 (int32 có dấu, đơn vị 0.1A)
    uint32_t rawCurr = get32(70);
    float curr = ((int32_t)rawCurr) * 0.1f;

    // Dung lượng % SoC: Byte 74
    uint8_t socVal = frame[74];
    if (socVal > 100) socVal = 100;

    // Dung lượng Ah
    float capTotal = (float)get32(75) * 0.000001f;
    float capRem   = (float)get32(79) * 0.000001f;
    float capCycle = (float)get32(83) * 0.001f;
    uint32_t uptime = get32(87);

    // Nhiệt độ: 6 cảm biến (Byte 91..102)
    float temps[ANT_BMS_MAX_TEMPS];
    for (uint8_t i = 0; i < ANT_BMS_MAX_TEMPS; i++) {
        temps[i] = (float)((int16_t)get16(91 + i * 2));
    }

    // Trạng thái MOSFET & Cân bằng
    bool chgMos  = (frame[103] == 0x01);
    bool disMos  = (frame[104] == 0x01);
    bool balStat = (frame[105] != 0x00);

    // Công suất (W): Byte 111..114
    float pwr = (float)((int32_t)get32(111));
    if (pwr == 0.0f && abs(curr) > 0.05f) {
        pwr = vTot * curr;
    }

    // Min/Max Cell
    uint8_t maxIdx = frame[115];
    float maxV     = (float)get16(116) * 0.001f;
    uint8_t minIdx = frame[118];
    float minV     = (float)get16(119) * 0.001f;
    float avgV     = (float)get16(121) * 0.001f;
    float deltaV   = maxV - minV;
    if (deltaV < 0.0f) deltaV = 0.0f;

    uint32_t balBitmask = get32(132);

    // Cập nhật an toàn với Mutex
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        _data.totalVoltage        = vTot;
        _data.current             = curr;
        _data.power               = pwr;
        _data.soc                 = socVal;
        _data.capacityRemaining   = capRem;
        _data.totalCapacity       = capTotal;
        _data.cycleCapacity       = capCycle;
        _data.uptimeSeconds       = uptime;
        _data.cellCount           = cells;

        for (uint8_t i = 0; i < ANT_BMS_MAX_CELLS; i++) {
            _data.cellVoltages[i] = cellV[i];
        }

        _data.maxCellVoltage      = maxV;
        _data.maxCellIndex        = maxIdx;
        _data.minCellVoltage      = minV;
        _data.minCellIndex        = minIdx;
        _data.deltaCellVoltage    = deltaV;
        _data.averageCellVoltage  = avgV;

        for (uint8_t i = 0; i < ANT_BMS_MAX_TEMPS; i++) {
            _data.temperatures[i] = temps[i];
        }

        _data.chargeMosfet        = chgMos;
        _data.dischargeMosfet     = disMos;
        _data.isBalancing         = balStat;
        _data.balancedCellBitmask = balBitmask;

        _data.isConnected         = true;
        _data.lastReceivedMs      = millis();
        _data.validPacketsCount++;

        xSemaphoreGive(_mutex);
    }

    if (_callback) {
        ANTBMSData copyData;
        if (getSnapshot(copyData)) {
            _callback(copyData);
        }
    }
    return true;
}

void AntBmsProtocolHandler::checkConnectionTimeout() {
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        if (_data.isConnected && (millis() - _data.lastReceivedMs > BMS_TIMEOUT_MS)) {
            _data.isConnected = false;
            _data.current = 0.0f;
            _data.power = 0.0f;
        }
        xSemaphoreGive(_mutex);
    }
}

void AntBmsProtocolHandler::update() {
    if (!_serial) return;

    // 1. Đọc dữ liệu từ UART không chặn
    while (_serial->available() > 0 && _rxLen < RX_BUFFER_SIZE) {
        _rxBuffer[_rxLen++] = static_cast<uint8_t>(_serial->read());
    }

    // 2. Tìm kiếm Header 0xAA 0x55 0xAA 0xFF trong buffer trượt
    size_t i = 0;
    while (i < _rxLen) {
        if (_rxBuffer[i] == 0xAA &&
            i + 1 < _rxLen && _rxBuffer[i + 1] == 0x55 &&
            i + 2 < _rxLen && _rxBuffer[i + 2] == 0xAA &&
            i + 3 < _rxLen && _rxBuffer[i + 3] == 0xFF) {

            // Đã tìm thấy Header, kiểm tra độ dài frame
            if (_rxLen - i < ANT_BMS_FRAME_LEN) {
                // Chưa đủ 140 bytes, giữ lại chờ đợt đọc sau
                break;
            }

            // Đã đủ 140 bytes, tiến hành kiểm tra checksum & giải mã
            if (parseFrame(&_rxBuffer[i], ANT_BMS_FRAME_LEN)) {
                i += ANT_BMS_FRAME_LEN;
                continue;
            } else {
                // Checksum sai, bỏ qua 1 byte để trượt tiếp
                i++;
                continue;
            }
        } else {
            i++;
        }
    }

    // 3. Dịch phần dữ liệu dư về đầu buffer
    if (i > 0) {
        if (i < _rxLen) {
            memmove(_rxBuffer, &_rxBuffer[i], _rxLen - i);
            _rxLen -= i;
        } else {
            _rxLen = 0;
        }
    }

    // 4. Gửi truy vấn định kỳ (Chỉ gửi nếu có lệnh yêu cầu hoặc không bật chế độ chân chờ thụ động)
    uint32_t now = millis();
#if !BMS_UART_PASSIVE_WAIT
    if (now - _lastPollMs >= BMS_POLL_INTERVAL_MS) {
        _lastPollMs = now;
        sendPollQuery();
    }
#endif

    // 5. Kiểm tra timeout mất kết nối
    checkConnectionTimeout();
}
