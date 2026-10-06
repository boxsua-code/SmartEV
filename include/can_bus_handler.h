#pragma once
#include <Arduino.h>
#include <driver/twai.h>
#include "config.h"
#include "votol_protocol.h"
#include "ant_bms_protocol.h"

/**
 * ============================================================================
 * CAN BUS (TWAI) PROTOCOL HANDLER CHO XE ĐIỆN (CHUẨN POLYTRON / VOTOL)
 * Chạy song song với UART ở chế độ TWAI_MODE_LISTEN_ONLY (An toàn thụ động)
 * ============================================================================
 */
class CanBusHandler {
public:
    CanBusHandler();
    ~CanBusHandler();

    // Khởi tạo TWAI CAN Bus (250kbps, Listen-Only)
    bool begin(int8_t txPin = PIN_CAN_TX, int8_t rxPin = PIN_CAN_RX, uint32_t baud = CAN_BAUD_RATE);

    // Cập nhật và giải mã gói tin CAN (non-blocking)
    void update(VotolData &vd, ANTBMSData &bd);

    bool isInitialized() const { return _initialized; }
    bool isConnected() const { return _isConnected; }
    uint32_t getMessageCount() const { return _messageCount; }

private:
    bool _initialized;
    bool _isConnected;
    uint32_t _messageCount;
    uint32_t _lastMessageMs;
    int8_t _txPin;
    int8_t _rxPin;
};
