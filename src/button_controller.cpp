#include "button_controller.h"

Button::Button(uint8_t pin, bool pullup, uint16_t debounceMs, uint16_t longPressMs)
    : _pin(pin),
      _pullup(pullup),
      _debounceMs(debounceMs),
      _longPressMs(longPressMs),
      _lastState(false),
      _isDown(false),
      _stateChangeTime(0),
      _pressStartTime(0),
      _longPressTriggered(false)
{
}

void Button::begin() {
    pinMode(_pin, _pullup ? INPUT_PULLUP : INPUT);
    bool initialRaw = digitalRead(_pin);
    _lastState = _pullup ? (initialRaw == LOW) : (initialRaw == HIGH);
    _isDown = _lastState;
}

ButtonEvent Button::update() {
    bool raw = digitalRead(_pin);
    bool reading = _pullup ? (raw == LOW) : (raw == HIGH);
    uint32_t now = millis();
    ButtonEvent event = ButtonEvent::NONE;

    // 1. Kiểm tra thay đổi mức logic (Debounce filter)
    if (reading != _lastState) {
        _lastState = reading;
        _stateChangeTime = now;
    }

    // 2. Nếu trạng thái ổn định vượt quá thời gian debounce
    if ((now - _stateChangeTime) > _debounceMs) {
        if (reading != _isDown) {
            _isDown = reading;

            if (_isDown) {
                // Bắt đầu nhấn xuống
                _pressStartTime = now;
                _longPressTriggered = false;
            } else {
                // Nhả phím: nếu chưa kích hoạt nhấn giữ thì phát sự kiện CLICK
                if (!_longPressTriggered && (now - _pressStartTime < _longPressMs)) {
                    event = ButtonEvent::CLICK;
                }
            }
        } else if (_isDown) {
            // Đang giữ phím
            if (!_longPressTriggered && (now - _pressStartTime >= _longPressMs)) {
                _longPressTriggered = true;
                event = ButtonEvent::LONG_PRESS;
            }
        }
    }

    return event;
}

// --- ButtonController Implementation ---

ButtonController::ButtonController(uint8_t pinSet, uint8_t pinUp, uint8_t pinDown)
    : _btnSet(pinSet, true, 40, 1500),
      _btnUp(pinUp, true, 40, 1500),
      _btnDown(pinDown, true, 40, 1500),
      _eventSet(ButtonEvent::NONE),
      _eventUp(ButtonEvent::NONE),
      _eventDown(ButtonEvent::NONE)
{
}

void ButtonController::begin() {
    _btnSet.begin();
    _btnUp.begin();
    _btnDown.begin();
}

void ButtonController::update() {
    ButtonEvent s = _btnSet.update();
    ButtonEvent u = _btnUp.update();
    ButtonEvent d = _btnDown.update();

    if (s != ButtonEvent::NONE) _eventSet = s;
    if (u != ButtonEvent::NONE) _eventUp = u;
    if (d != ButtonEvent::NONE) _eventDown = d;
}
