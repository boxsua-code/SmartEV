#pragma once
#include <Arduino.h>

/**
 * ============================================================================
 * QUẢN LÝ NÚT BẤM (BUTTON CONTROLLER) VỚI SOFTWARE DEBOUNCE
 * Chống rung phím và phân biệt Nhấn Ngắn (Click) / Nhấn Giữ (Long Press)
 * ============================================================================
 */

enum class ButtonEvent : uint8_t {
    NONE = 0,
    CLICK,          // Nhấn nhả ngắn (< 500ms)
    LONG_PRESS,     // Nhấn giữ vượt ngưỡng (> 1500ms)
    HOLDING         // Đang giữ nút
};

class Button {
public:
    Button(uint8_t pin, bool pullup = true, uint16_t debounceMs = 40, uint16_t longPressMs = 1500);

    void begin();
    ButtonEvent update(); // Gọi liên tục trong loop() không chặn

    bool isPressed() const { return _isDown; }

private:
    uint8_t _pin;
    bool _pullup;
    uint16_t _debounceMs;
    uint16_t _longPressMs;

    bool _lastState;
    bool _isDown;
    uint32_t _stateChangeTime;
    uint32_t _pressStartTime;
    bool _longPressTriggered;
};

class ButtonController {
public:
    ButtonController(uint8_t pinSet, uint8_t pinUp, uint8_t pinDown);

    void begin();
    
    // Quét trạng thái cả 3 phím
    void update();

    ButtonEvent getSetEvent()  { ButtonEvent e = _eventSet;  _eventSet = ButtonEvent::NONE;  return e; }
    ButtonEvent getUpEvent()   { ButtonEvent e = _eventUp;   _eventUp = ButtonEvent::NONE;   return e; }
    ButtonEvent getDownEvent() { ButtonEvent e = _eventDown; _eventDown = ButtonEvent::NONE; return e; }

private:
    Button _btnSet;
    Button _btnUp;
    Button _btnDown;

    ButtonEvent _eventSet;
    ButtonEvent _eventUp;
    ButtonEvent _eventDown;
};
