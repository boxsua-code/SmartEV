#include "vehicle_signals.h"

VehicleSignals::VehicleSignals(uint8_t pinLeft, uint8_t pinRight, uint8_t pinHeadlight, bool activeLow)
    : _pinLeft(pinLeft), _pinRight(pinRight), _pinHeadlight(pinHeadlight), _activeLow(activeLow),
      _simLeft(false), _simRight(false), _simHeadlight(false), _simHazard(false) {
}

void VehicleSignals::begin() {
    pinMode(_pinLeft, INPUT_PULLUP);
    pinMode(_pinRight, INPUT_PULLUP);
    pinMode(_pinHeadlight, INPUT_PULLUP);

    Serial.printf("[SIGNALS] Khởi tạo chân tín hiệu: Xi nhan Trái (GPIO %d), Xi nhan Phải (GPIO %d), Đèn Pha (GPIO %d)\n",
                  _pinLeft, _pinRight, _pinHeadlight);
}

void VehicleSignals::update() {
    // Đọc trạng thái chân vật lý (xét mức tích cực Active Low / High)
    bool rawLeft = (digitalRead(_pinLeft) == (_activeLow ? LOW : HIGH));
    bool rawRight = (digitalRead(_pinRight) == (_activeLow ? LOW : HIGH));
    bool rawHeadlight = (digitalRead(_pinHeadlight) == (_activeLow ? LOW : HIGH));

    // Tổng hợp tín hiệu vật lý hoặc mô phỏng phần mềm
    _state.turnLeft = rawLeft || _simLeft;
    _state.turnRight = rawRight || _simRight;
    _state.headlight = rawHeadlight || _simHeadlight;
    _state.hazard = _simHazard || (rawLeft && rawRight);
}

bool VehicleSignals::isBlinkPhaseOn() const {
    return (millis() / TURN_SIGNAL_BLINK_MS) % 2 == 0;
}

bool VehicleSignals::shouldShowLeftBlink() const {
    if (_state.hazard) return isBlinkPhaseOn();
    if (_state.turnLeft) return isBlinkPhaseOn();
    return false;
}

bool VehicleSignals::shouldShowRightBlink() const {
    if (_state.hazard) return isBlinkPhaseOn();
    if (_state.turnRight) return isBlinkPhaseOn();
    return false;
}

void VehicleSignals::toggleTurnLeft() {
    _simLeft = !_simLeft;
    if (_simLeft) _simRight = false; // Tắt bên phải khi bật bên trái
    Serial.printf("[SIGNALS] Xi nhan Trái: %s\n", _simLeft ? "BẬT" : "TẮT");
}

void VehicleSignals::toggleTurnRight() {
    _simRight = !_simRight;
    if (_simRight) _simLeft = false; // Tắt bên trái khi bật bên phải
    Serial.printf("[SIGNALS] Xi nhan Phải: %s\n", _simRight ? "BẬT" : "TẮT");
}

void VehicleSignals::toggleHeadlight() {
    _simHeadlight = !_simHeadlight;
    Serial.printf("[SIGNALS] Đèn Pha (High Beam): %s\n", _simHeadlight ? "BẬT" : "TẮT");
}

void VehicleSignals::toggleHazard() {
    _simHazard = !_simHazard;
    Serial.printf("[SIGNALS] Đèn khẩn cấp Hazard: %s\n", _simHazard ? "BẬT" : "TẮT");
}

void VehicleSignals::clearAll() {
    _simLeft = false;
    _simRight = false;
    _simHeadlight = false;
    _simHazard = false;
    Serial.println("[SIGNALS] Đã tắt toàn bộ đèn và xi nhan mô phỏng.");
}

void VehicleSignals::setTurnLeft(bool on) {
    _simLeft = on;
}

void VehicleSignals::setTurnRight(bool on) {
    _simRight = on;
}

void VehicleSignals::setHeadlight(bool on) {
    _simHeadlight = on;
}

void VehicleSignals::setHazard(bool on) {
    _simHazard = on;
}
