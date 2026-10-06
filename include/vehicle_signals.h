#pragma once
#include <Arduino.h>
#include "config.h"

/**
 * ============================================================================
 * QUẢN LÝ TÍN HIỆU ĐÈN VÀ XI NHAN XE ĐIỆN (VEHICLE SIGNALS)
 * Hỗ trợ đọc GPIO phần cứng (Optocoupler/Trở kéo) & Giả lập Serial
 * ============================================================================
 */

struct SignalState {
    bool turnLeft = false;      // Xi nhan trái đang bật
    bool turnRight = false;     // Xi nhan phải đang bật
    bool headlight = false;     // Đèn pha đang bật (High Beam / Pha)
    bool hazard = false;        // Đèn khẩn cấp Hazard (cả 2 xi nhan cùng nháy)
};

class VehicleSignals {
public:
    VehicleSignals(uint8_t pinLeft = PIN_SIGNAL_LEFT,
                   uint8_t pinRight = PIN_SIGNAL_RIGHT,
                   uint8_t pinHeadlight = PIN_SIGNAL_HEADLIGHT,
                   bool activeLow = SIGNAL_INPUT_ACTIVE_LOW);

    void begin();

    // Cập nhật trạng thái tín hiệu từ GPIO (non-blocking)
    void update();

    // Lấy trạng thái tín hiệu
    const SignalState& getState() const { return _state; }

    // Kiểm tra pha nhấp nháy xi nhan (sáng 400ms / tắt 400ms)
    bool isBlinkPhaseOn() const;

    // Các hàm kiểm tra thuận tiện cho Render UI
    bool shouldShowLeftBlink() const;
    bool shouldShowRightBlink() const;
    bool isHeadlightOn() const { return _state.headlight; }

    // Các hàm mô phỏng / điều khiển bằng phần mềm hoặc qua Serial
    void toggleTurnLeft();
    void toggleTurnRight();
    void toggleHeadlight();
    void toggleHazard();
    void clearAll();

    void setTurnLeft(bool on);
    void setTurnRight(bool on);
    void setHeadlight(bool on);
    void setHazard(bool on);

private:
    uint8_t _pinLeft;
    uint8_t _pinRight;
    uint8_t _pinHeadlight;
    bool _activeLow;

    SignalState _state;

    // Giữ cờ mô phỏng phần mềm nếu không có kết nối vật lý
    bool _simLeft;
    bool _simRight;
    bool _simHeadlight;
    bool _simHazard;
};
