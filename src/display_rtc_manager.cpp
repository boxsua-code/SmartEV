#include "display_rtc_manager.h"

DisplayRtcManager::DisplayRtcManager()
    : _display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, -1),
      _oledReady(false), _rtcReady(false),
      _lastRenderMs(0), _lastRtcReadMs(0),
      _currentPage(ScreenPage::PAGE_MAIN_DASHBOARD),
      _selectedQuickSetting(0), _selectedAdvSetting(0),
      _isEditMode(false), _bmsCellPageOffset(0),
      _tripDistanceKm(0.0f) {
}

void DisplayRtcManager::loadConfigFromNVS() {
    _prefs.begin("ev_dash", false);
    _cfg.wheelDiameterM   = _prefs.getFloat("wheel", DEFAULT_WHEEL_DIAMETER_M);
    _cfg.currentLimitA    = _prefs.getUShort("cur_lim", 45);
    _cfg.lowVoltageCutoff = _prefs.getFloat("v_cut", DEFAULT_LOW_VOLTAGE_CUTOFF);
    _cfg.overheatLimitC   = _prefs.getUChar("ovh_c", DEFAULT_OVERHEAT_LIMIT_C);
    _cfg.deltaCellMv      = _prefs.getUShort("d_cell", DEFAULT_DELTA_CELL_MV);
    _cfg.motorPoles       = _prefs.getUChar("poles", DEFAULT_MOTOR_POLE_PAIRS);
    _cfg.gearRatio        = _prefs.getFloat("gear", DEFAULT_GEAR_RATIO);
    _cfg.oledContrast     = _prefs.getUChar("bright", DEFAULT_OLED_CONTRAST);
    _cfg.speedWarnKmh     = _prefs.getUChar("spd_w", DEFAULT_SPEED_WARN_KMH);
    _cfg.useMph           = _prefs.getBool("mph", false);
    _prefs.end();

    Serial.println("[CONFIG] Đã tải cấu hình xe điện từ Flash NVS thành công!");
    Serial.printf("[CONFIG] Bánh xe: %.4fm | Tỉ số truyền: %.1f | Cắt áp: %.1fV | Quá nhiệt: %d°C | Cực motor: %d\n",
                  _cfg.wheelDiameterM, _cfg.gearRatio, _cfg.lowVoltageCutoff, _cfg.overheatLimitC, _cfg.motorPoles);
}

void DisplayRtcManager::saveConfigToNVS() {
    _prefs.begin("ev_dash", false);
    _prefs.putFloat("wheel", _cfg.wheelDiameterM);
    _prefs.putUShort("cur_lim", _cfg.currentLimitA);
    _prefs.putFloat("v_cut", _cfg.lowVoltageCutoff);
    _prefs.putUChar("ovh_c", _cfg.overheatLimitC);
    _prefs.putUShort("d_cell", _cfg.deltaCellMv);
    _prefs.putUChar("poles", _cfg.motorPoles);
    _prefs.putFloat("gear", _cfg.gearRatio);
    _prefs.putUChar("bright", _cfg.oledContrast);
    _prefs.putUChar("spd_w", _cfg.speedWarnKmh);
    _prefs.putBool("mph", _cfg.useMph);
    _prefs.end();

    Serial.println("[CONFIG] >>> Đã LƯU cấu hình mới vào Flash NVS thành công! <<<");
}

void DisplayRtcManager::resetToFactoryDefaults(VotolProtocolHandler &votol) {
    _cfg.wheelDiameterM   = DEFAULT_WHEEL_DIAMETER_M;
    _cfg.currentLimitA    = 45;
    _cfg.lowVoltageCutoff = DEFAULT_LOW_VOLTAGE_CUTOFF;
    _cfg.overheatLimitC   = DEFAULT_OVERHEAT_LIMIT_C;
    _cfg.deltaCellMv      = DEFAULT_DELTA_CELL_MV;
    _cfg.motorPoles       = DEFAULT_MOTOR_POLE_PAIRS;
    _cfg.gearRatio        = DEFAULT_GEAR_RATIO;
    _cfg.oledContrast     = DEFAULT_OLED_CONTRAST;
    _cfg.speedWarnKmh     = DEFAULT_SPEED_WARN_KMH;
    _cfg.useMph           = false;
    _tripDistanceKm       = 0.0f;

    saveConfigToNVS();
    applyHardwareConfig(votol);
    Serial.println("[CONFIG] Đã khôi phục toàn bộ cài đặt gốc nhà sản xuất!");
}

void DisplayRtcManager::applyHardwareConfig(VotolProtocolHandler &votol) {
    votol.setWheelParams(_cfg.wheelDiameterM, _cfg.gearRatio);
    if (_oledReady) {
        _display.ssd1306_command(SSD1306_SETCONTRAST);
        _display.ssd1306_command(_cfg.oledContrast);
    }
}

bool DisplayRtcManager::begin(int8_t sdaPin, int8_t sclPin, uint32_t freq) {
    Serial.printf("[INIT] Bắt đầu khởi tạo Bus I2C (SDA: GPIO %d, SCL: GPIO %d, Freq: %d Hz)...\n",
                  sdaPin, sclPin, freq);

    // 1. Tải cấu hình từ Flash NVS
    loadConfigFromNVS();

    // 2. Khởi tạo Bus I2C
    Wire.begin(sdaPin, sclPin, freq);

    // 3. Khởi tạo RTC DS3231
    if (_rtc.begin()) {
        _rtcReady = true;
        if (_rtc.lostPower()) {
            Serial.println("[RTC] ⚠️ DS3231 mất nguồn dự phòng! Đặt giờ mặc định compile time.");
            _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        _lastCachedTime = _rtc.now();
        Serial.printf("[RTC] ✅ DS3231 sẵn sàng! Giờ hiện tại: %02d:%02d:%02d\n",
                      _lastCachedTime.hour(), _lastCachedTime.minute(), _lastCachedTime.second());
    } else {
        _rtcReady = false;
        Serial.println("[RTC] ❌ Không tìm thấy module RTC DS3231 trên bus I2C!");
    }

    // 4. Khởi tạo màn hình OLED SSD1306
    if (_display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        _oledReady = true;
        _display.ssd1306_command(SSD1306_SETCONTRAST);
        _display.ssd1306_command(_cfg.oledContrast);
        Serial.printf("[OLED] ✅ Màn hình SSD1306 sẵn sàng (128x%d)!\n", OLED_SCREEN_HEIGHT);
        showSplashScreen();
    } else {
        _oledReady = false;
        Serial.printf("[OLED] ❌ Không thể khởi động SSD1306 trên địa chỉ 0x%02X!\n", OLED_I2C_ADDRESS);
    }

    return (_oledReady || _rtcReady);
}

void DisplayRtcManager::showSplashScreen() {
    if (!_oledReady) return;

    _display.clearDisplay();
    _display.setTextWrap(false);

    if (OLED_SCREEN_HEIGHT == 32) {
        _display.setTextSize(1);
        _display.setCursor(18, 4);
        _display.print("SMART DASHBOARD");
        _display.drawRect(14, 18, 100, 8, SSD1306_WHITE);
        _display.fillRect(16, 20, 60, 4, SSD1306_WHITE);
    } else {
        _display.setTextSize(2);
        _display.setCursor(8, 10);
        _display.print("SMART EV");
        _display.setTextSize(1);
        _display.setCursor(14, 32);
        _display.print("ESP32-S3 DASHBOARD");
        _display.drawRect(14, 46, 100, 8, SSD1306_WHITE);
        _display.fillRect(16, 48, 70, 4, SSD1306_WHITE);
    }

    _display.display();
    delay(1000);
}

void DisplayRtcManager::showMessage(const char* title, const char* subtitle, uint16_t durationMs) {
    if (!_oledReady) return;

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setCursor(0, 4);
    _display.println(title);

    if (subtitle) {
        _display.setCursor(0, 18);
        _display.println(subtitle);
    }

    _display.display();
    delay(durationMs);
}

DateTime DisplayRtcManager::getDateTime() {
    uint32_t now = millis();
    if (_rtcReady && (now - _lastRtcReadMs >= 500)) {
        _lastRtcReadMs = now;
        _lastCachedTime = _rtc.now();
    }
    return _lastCachedTime;
}

String DisplayRtcManager::getFormattedTime(bool showSeconds) {
    DateTime dt = getDateTime();
    char buf[12];
    if (showSeconds) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", dt.hour(), dt.minute(), dt.second());
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d", dt.hour(), dt.minute());
    }
    return String(buf);
}

String DisplayRtcManager::getFormattedDate() {
    DateTime dt = getDateTime();
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d", dt.day(), dt.month(), dt.year());
    return String(buf);
}

float DisplayRtcManager::getRtcTemperature() {
    if (_rtcReady) return _rtc.getTemperature();
    return 0.0f;
}

void DisplayRtcManager::setDateTime(const DateTime &dt) {
    if (_rtcReady) {
        _rtc.adjust(dt);
        _lastCachedTime = dt;
    }
}

void DisplayRtcManager::updateTrip(float speedKmh, uint32_t deltaMs) {
    if (speedKmh > 0.5f && deltaMs > 0) {
        _tripDistanceKm += (speedKmh * ((float)deltaMs / 3600000.0f));
    }
}

// --- CÁC HÀM VẼ ĐỒ HỌA BỔ TRỢ ---

void DisplayRtcManager::drawBatteryIcon(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t soc) {
    if (!_oledReady) return;
    if (soc > 100) soc = 100;
    _display.drawRect(x, y, w - 2, h, SSD1306_WHITE);
    _display.fillRect(x + w - 2, y + (h / 4), 2, h / 2, SSD1306_WHITE);

    int16_t innerWidth = (w - 6);
    int16_t fillW = (innerWidth * soc) / 100;
    if (fillW > 0) {
        _display.fillRect(x + 2, y + 2, fillW, h - 4, SSD1306_WHITE);
    }
}

void DisplayRtcManager::drawGearBadge(int16_t x, int16_t y, const char* gearStr) {
    if (!_oledReady) return;
    int16_t boxW = (strlen(gearStr) > 2) ? 28 : 16;
    int16_t boxH = 12;
    _display.drawRoundRect(x, y, boxW, boxH, 2, SSD1306_WHITE);
    _display.setTextSize(1);
    _display.setCursor(x + 3, y + 2);
    _display.print(gearStr);
}

void DisplayRtcManager::drawBleBadge(int16_t x, int16_t y, bool isConnected) {
    if (!_oledReady) return;
    if (isConnected) {
        _display.drawPixel(x + 1, y, SSD1306_WHITE);
        _display.drawFastVLine(x + 1, y, 7, SSD1306_WHITE);
        _display.drawLine(x + 1, y + 1, x + 3, y + 3, SSD1306_WHITE);
        _display.drawLine(x + 3, y + 3, x + 1, y + 5, SSD1306_WHITE);
        _display.drawLine(x + 1, y + 1, x - 1, y + 3, SSD1306_WHITE);
        _display.drawLine(x - 1, y + 3, x + 1, y + 5, SSD1306_WHITE);
    }
}

void DisplayRtcManager::drawTurnSignalLeft(int16_t x, int16_t y, bool active, uint8_t size) {
    if (!_oledReady) return;
    if (active) {
        _display.fillTriangle(x, y + size / 2, x + size, y, x + size, y + size, SSD1306_WHITE);
    } else {
        _display.drawTriangle(x, y + size / 2, x + size, y, x + size, y + size, SSD1306_WHITE);
    }
}

void DisplayRtcManager::drawTurnSignalRight(int16_t x, int16_t y, bool active, uint8_t size) {
    if (!_oledReady) return;
    if (active) {
        _display.fillTriangle(x + size, y + size / 2, x, y, x, y + size, SSD1306_WHITE);
    } else {
        _display.drawTriangle(x + size, y + size / 2, x, y, x, y + size, SSD1306_WHITE);
    }
}

void DisplayRtcManager::drawHeadlightIcon(int16_t x, int16_t y, bool active) {
    if (!_oledReady) return;
    if (active) {
        // Vẽ chóa đèn pha hình bán nguyệt bo tròn
        _display.drawRoundRect(x, y, 7, 8, 3, SSD1306_WHITE);
        _display.fillRoundRect(x + 1, y + 1, 5, 6, 2, SSD1306_WHITE);
        // Vẽ các tia sáng ngang
        _display.drawFastHLine(x + 8, y + 1, 4, SSD1306_WHITE);
        _display.drawFastHLine(x + 8, y + 4, 5, SSD1306_WHITE);
        _display.drawFastHLine(x + 8, y + 7, 4, SSD1306_WHITE);
    }
}

void DisplayRtcManager::drawNavIcon(int16_t x, int16_t y, NavIconType icon, uint8_t size) {
    if (!_oledReady) return;
    uint8_t half = size / 2;

    switch (icon) {
        case NavIconType::STRAIGHT: // Đi thẳng ⬆
            _display.fillTriangle(x + half, y, x + 2, y + half, x + size - 2, y + half, SSD1306_WHITE);
            _display.fillRect(x + half - 2, y + half, 5, half, SSD1306_WHITE);
            break;
        case NavIconType::TURN_LEFT: // Rẽ trái ⬅
            _display.fillTriangle(x, y + half, x + half, y + 2, x + half, y + size - 2, SSD1306_WHITE);
            _display.fillRect(x + half - 1, y + half - 2, half - 1, 5, SSD1306_WHITE);
            _display.fillRect(x + size - 4, y + half - 2, 4, half + 2, SSD1306_WHITE);
            break;
        case NavIconType::TURN_RIGHT: // Rẽ phải ➡
            _display.fillTriangle(x + size, y + half, x + half, y + 2, x + half, y + size - 2, SSD1306_WHITE);
            _display.fillRect(x + 2, y + half - 2, half, 5, SSD1306_WHITE);
            _display.fillRect(x + 2, y + half - 2, 4, half + 2, SSD1306_WHITE);
            break;
        case NavIconType::SLIGHT_LEFT: // Chếch trái ↖
            _display.fillTriangle(x + 2, y + 2, x + half + 2, y, x, y + half + 2, SSD1306_WHITE);
            _display.drawLine(x + 2, y + 2, x + size - 2, y + size - 2, SSD1306_WHITE);
            break;
        case NavIconType::SLIGHT_RIGHT: // Chếch phải ↗
            _display.fillTriangle(x + size - 2, y + 2, x + size - half - 2, y, x + size, y + half + 2, SSD1306_WHITE);
            _display.drawLine(x + size - 2, y + 2, x + 2, y + size - 2, SSD1306_WHITE);
            break;
        case NavIconType::U_TURN: // Quay đầu ↩
            _display.drawCircle(x + half, y + half - 2, half - 2, SSD1306_WHITE);
            _display.fillRect(x, y + half - 2, size, half + 2, SSD1306_BLACK);
            _display.drawFastVLine(x + 2, y + half - 2, half + 2, SSD1306_WHITE);
            _display.drawFastVLine(x + size - 2, y + half - 2, half + 2, SSD1306_WHITE);
            _display.fillTriangle(x + 2, y + size, x, y + half + 2, x + 4, y + half + 2, SSD1306_WHITE);
            break;
        case NavIconType::DESTINATION: // Đích đến 🏁
            _display.drawFastVLine(x + 2, y, size, SSD1306_WHITE);
            _display.fillRect(x + 3, y + 1, size - 5, half, SSD1306_WHITE);
            break;
        default:
            _display.fillCircle(x + half, y + half, 3, SSD1306_WHITE);
            break;
    }
}

void DisplayRtcManager::drawCallOverlay(const char* caller) {
    if (!_oledReady) return;

    if (OLED_SCREEN_HEIGHT == 32) {
        _display.fillRect(2, 2, 124, 28, SSD1306_BLACK);
        _display.drawRect(2, 2, 124, 28, SSD1306_WHITE);
        _display.setTextSize(1);
        _display.setCursor(18, 5);
        _display.print("CUOC GOI DEN!");
        _display.setCursor(6, 17);
        _display.print(caller ? caller : "Khong xac dinh");
    } else {
        _display.fillRect(6, 10, 116, 44, SSD1306_BLACK);
        _display.drawRect(6, 10, 116, 44, SSD1306_WHITE);
        _display.drawRect(8, 12, 112, 40, SSD1306_WHITE);
        _display.setTextSize(1);
        _display.setCursor(20, 16);
        _display.print("CUOC GOI DEN!");
        _display.setCursor(12, 28);
        _display.print(caller ? caller : "Khong xac dinh");
        _display.setCursor(12, 40);
        _display.print("Nhan SET de tat");
    }
}

// --- XỬ LÝ NÚT BẤM VÀ ĐIỀU HƯỚNG FSM 5 TRANG ---

void DisplayRtcManager::handleButtons(ButtonEvent setEvt, ButtonEvent upEvt, ButtonEvent downEvt, VotolProtocolHandler &votol) {
    // 1. Nút SET (Chuyển trang hoặc Bật/Tắt chế độ Edit)
    if (setEvt == ButtonEvent::LONG_PRESS) {
        if (_currentPage == ScreenPage::PAGE_QUICK_SETTINGS || _currentPage == ScreenPage::PAGE_ADVANCED_SETTINGS) {
            _isEditMode = !_isEditMode;
            if (!_isEditMode) {
                saveConfigToNVS();
                applyHardwareConfig(votol);
            }
            Serial.printf("[FSM] Nút SET (Giữ 1.5s): Chế độ Edit = %s\n", _isEditMode ? "BẬT" : "TẮT (ĐÃ LƯU)");
        }
    } else if (setEvt == ButtonEvent::CLICK) {
        if (_isEditMode) {
            _isEditMode = false;
            saveConfigToNVS();
            applyHardwareConfig(votol);
            Serial.println("[FSM] Nút SET: Đã lưu và thoát chế độ Edit!");
        } else {
            // Chuyển tuần hoàn qua 5 trang: Main -> BMS -> Quick -> Advanced -> Navigation -> Main
            uint8_t next = (static_cast<uint8_t>(_currentPage) + 1) % static_cast<uint8_t>(ScreenPage::PAGE_COUNT);
            _currentPage = static_cast<ScreenPage>(next);
            Serial.printf("[FSM] Nút SET: Chuyển sang Trang %d / %d\n", next + 1, static_cast<uint8_t>(ScreenPage::PAGE_COUNT));
        }
    }

    // 2. Nút UP / DOWN
    if (_currentPage == ScreenPage::PAGE_BMS_DETAIL) {
        // Cuộn xem các nhóm 8 cell
        if (downEvt == ButtonEvent::CLICK) {
            _bmsCellPageOffset = (_bmsCellPageOffset + 8) % 32;
        } else if (upEvt == ButtonEvent::CLICK) {
            _bmsCellPageOffset = (_bmsCellPageOffset >= 8) ? (_bmsCellPageOffset - 8) : 24;
        }
    } else if (_currentPage == ScreenPage::PAGE_QUICK_SETTINGS) {
        if (!_isEditMode) {
            // Cuộn chọn mục Quick Settings
            if (downEvt == ButtonEvent::CLICK) {
                _selectedQuickSetting = (_selectedQuickSetting + 1) % static_cast<uint8_t>(QuickSettingItem::QUICK_SETTING_COUNT);
            } else if (upEvt == ButtonEvent::CLICK) {
                _selectedQuickSetting = (_selectedQuickSetting == 0) ? (static_cast<uint8_t>(QuickSettingItem::QUICK_SETTING_COUNT) - 1) : (_selectedQuickSetting - 1);
            }
        } else {
            // Đang chỉnh sửa Quick Settings
            DateTime dt = getDateTime();
            switch (static_cast<QuickSettingItem>(_selectedQuickSetting)) {
                case QuickSettingItem::SETTING_WHEEL_DIAMETER:
                    if (upEvt == ButtonEvent::CLICK) _cfg.wheelDiameterM += 0.005f;
                    if (downEvt == ButtonEvent::CLICK && _cfg.wheelDiameterM > 0.1f) _cfg.wheelDiameterM -= 0.005f;
                    votol.setWheelParams(_cfg.wheelDiameterM, _cfg.gearRatio);
                    break;
                case QuickSettingItem::SETTING_CURRENT_LIMIT:
                    if (upEvt == ButtonEvent::CLICK) _cfg.currentLimitA += 5;
                    if (downEvt == ButtonEvent::CLICK && _cfg.currentLimitA >= 10) _cfg.currentLimitA -= 5;
                    break;
                case QuickSettingItem::SETTING_RTC_HOUR:
                    if (upEvt == ButtonEvent::CLICK) {
                        setDateTime(DateTime(dt.year(), dt.month(), dt.day(), (dt.hour() + 1) % 24, dt.minute(), dt.second()));
                    }
                    if (downEvt == ButtonEvent::CLICK) {
                        setDateTime(DateTime(dt.year(), dt.month(), dt.day(), (dt.hour() == 0 ? 23 : dt.hour() - 1), dt.minute(), dt.second()));
                    }
                    break;
                case QuickSettingItem::SETTING_RTC_MINUTE:
                    if (upEvt == ButtonEvent::CLICK) {
                        setDateTime(DateTime(dt.year(), dt.month(), dt.day(), dt.hour(), (dt.minute() + 1) % 60, 0));
                    }
                    if (downEvt == ButtonEvent::CLICK) {
                        setDateTime(DateTime(dt.year(), dt.month(), dt.day(), dt.hour(), (dt.minute() == 0 ? 59 : dt.minute() - 1), 0));
                    }
                    break;
                case QuickSettingItem::SETTING_RESET_TRIP:
                    if (upEvt == ButtonEvent::CLICK || downEvt == ButtonEvent::CLICK) {
                        _tripDistanceKm = 0.0f;
                        Serial.println("[SETTINGS] Đã Reset quãng đường Trip!");
                    }
                    break;
                default: break;
            }
        }
    } else if (_currentPage == ScreenPage::PAGE_ADVANCED_SETTINGS) {
        if (!_isEditMode) {
            // Cuộn chọn mục Advanced Settings
            if (downEvt == ButtonEvent::CLICK) {
                _selectedAdvSetting = (_selectedAdvSetting + 1) % static_cast<uint8_t>(AdvancedSettingItem::ADV_SETTING_COUNT);
            } else if (upEvt == ButtonEvent::CLICK) {
                _selectedAdvSetting = (_selectedAdvSetting == 0) ? (static_cast<uint8_t>(AdvancedSettingItem::ADV_SETTING_COUNT) - 1) : (_selectedAdvSetting - 1);
            }
        } else {
            // Đang chỉnh sửa Advanced Settings
            switch (static_cast<AdvancedSettingItem>(_selectedAdvSetting)) {
                case AdvancedSettingItem::ADV_LOW_VOLT_CUTOFF:
                    if (upEvt == ButtonEvent::CLICK) _cfg.lowVoltageCutoff += 0.5f;
                    if (downEvt == ButtonEvent::CLICK && _cfg.lowVoltageCutoff > 30.0f) _cfg.lowVoltageCutoff -= 0.5f;
                    break;
                case AdvancedSettingItem::ADV_OVERHEAT_LIMIT:
                    if (upEvt == ButtonEvent::CLICK && _cfg.overheatLimitC < 130) _cfg.overheatLimitC += 5;
                    if (downEvt == ButtonEvent::CLICK && _cfg.overheatLimitC > 50) _cfg.overheatLimitC -= 5;
                    break;
                case AdvancedSettingItem::ADV_DELTA_CELL_MV:
                    if (upEvt == ButtonEvent::CLICK && _cfg.deltaCellMv < 200) _cfg.deltaCellMv += 5;
                    if (downEvt == ButtonEvent::CLICK && _cfg.deltaCellMv > 5) _cfg.deltaCellMv -= 5;
                    break;
                case AdvancedSettingItem::ADV_MOTOR_POLES:
                    if (upEvt == ButtonEvent::CLICK && _cfg.motorPoles < 40) _cfg.motorPoles += 1;
                    if (downEvt == ButtonEvent::CLICK && _cfg.motorPoles > 2) _cfg.motorPoles -= 1;
                    break;
                case AdvancedSettingItem::ADV_GEAR_RATIO:
                    if (upEvt == ButtonEvent::CLICK && _cfg.gearRatio < 10.0f) _cfg.gearRatio += 0.1f;
                    if (downEvt == ButtonEvent::CLICK && _cfg.gearRatio > 0.5f) _cfg.gearRatio -= 0.1f;
                    votol.setWheelParams(_cfg.wheelDiameterM, _cfg.gearRatio);
                    break;
                case AdvancedSettingItem::ADV_OLED_CONTRAST:
                    if (upEvt == ButtonEvent::CLICK && _cfg.oledContrast <= 230) _cfg.oledContrast += 25;
                    if (downEvt == ButtonEvent::CLICK && _cfg.oledContrast >= 35) _cfg.oledContrast -= 25;
                    _display.ssd1306_command(SSD1306_SETCONTRAST);
                    _display.ssd1306_command(_cfg.oledContrast);
                    break;
                case AdvancedSettingItem::ADV_SPEED_WARN_KMH:
                    if (upEvt == ButtonEvent::CLICK && _cfg.speedWarnKmh < 150) _cfg.speedWarnKmh += 5;
                    if (downEvt == ButtonEvent::CLICK && _cfg.speedWarnKmh > 20) _cfg.speedWarnKmh -= 5;
                    break;
                case AdvancedSettingItem::ADV_SPEED_UNIT:
                    if (upEvt == ButtonEvent::CLICK || downEvt == ButtonEvent::CLICK) {
                        _cfg.useMph = !_cfg.useMph;
                    }
                    break;
                case AdvancedSettingItem::ADV_FACTORY_RESET:
                    if (upEvt == ButtonEvent::CLICK || downEvt == ButtonEvent::CLICK) {
                        resetToFactoryDefaults(votol);
                        _isEditMode = false;
                    }
                    break;
                default: break;
            }
        }
    }
}

// --- VẼ GIAO DIỆN 128x32 (0.91 INCH) ---

void DisplayRtcManager::renderMainDashboard32(const VotolData &votol, const ANTBMSData &bms, const BleNavData &nav, const VehicleSignals &signals, const DateTime &time) {
    _display.clearDisplay();

    // 1. TÍN HIỆU XI NHAN VÀ ĐÈN PHA
    drawTurnSignalLeft(0, 0, signals.shouldShowLeftBlink(), 7);
    drawTurnSignalRight(120, 0, signals.shouldShowRightBlink(), 7);
    drawHeadlightIcon(42, 2, signals.isHeadlightOn());

    // 2. VẬN TỐC LỚN
    _display.setTextSize(3);
    _display.setCursor(0, 7);
    float showSpeed = _cfg.useMph ? (votol.speedKmh * 0.621371f) : votol.speedKmh;
    int spd = (int)showSpeed;
    if (spd > 199) spd = 199;
    _display.printf("%2d", spd);

    _display.setTextSize(1);
    _display.setCursor(38, 21);
    _display.print(_cfg.useMph ? "mph" : "kmh");

    // 3. KHU VỰC GIỮA: CẤP SỐ & GIỜ HOẶC ĐIỀU HƯỚNG
    if (nav.isNavActive()) {
        drawNavIcon(60, 2, nav.icon, 12);
        _display.setTextSize(1);
        _display.setCursor(76, 4);
        _display.print(nav.distance);
    } else {
        drawGearBadge(58, 2, votol.getGearString());
        _display.setTextSize(1);
        _display.setCursor(90, 4);
        _display.printf("%02d:%02d", time.hour(), time.minute());
    }

    // 4. % PIN VÀ ICON PIN
    uint8_t soc = bms.isConnected ? bms.soc : 0;
    _display.setCursor(58, 20);
    _display.printf("%2d%%", soc);
    drawBatteryIcon(80, 21, 14, 8, soc);

    // 5. CỜ KẾT NỐI VÀ BLUETOOTH
    _display.setCursor(98, 20);
    if (votol.isConnected && bms.isConnected) _display.print("VB");
    else if (votol.isConnected) _display.print("V-");
    else if (bms.isConnected) _display.print("-B");
    else _display.print("--");

    if (nav.isConnected) {
        drawBleBadge(114, 20, true);
    }

    _display.display();
}

void DisplayRtcManager::renderBmsDetail32(const ANTBMSData &bms) {
    _display.clearDisplay();
    _display.setTextSize(1);

    _display.setCursor(0, 0);
    _display.printf("BMS (P2) %4.1fV %4.1fA", bms.totalVoltage, bms.current);

    _display.setCursor(0, 11);
    _display.printf("Min:%3.2f Max:%3.2f", bms.minCellVoltage, bms.maxCellVoltage);

    _display.setCursor(0, 22);
    _display.printf("d:%2.0fmV P:%2.0fC F:%2.0fC",
                   bms.deltaCellVoltage * 1000.0f, bms.getBatteryTemp(), bms.getMosfetTemp());

    _display.display();
}

void DisplayRtcManager::renderQuickSettings32(const DateTime &time) {
    _display.clearDisplay();
    _display.setTextSize(1);

    _display.setCursor(0, 0);
    _display.printf("MENU (P3) %s", _isEditMode ? "[EDIT]" : "");

    _display.setCursor(0, 11);
    switch (static_cast<QuickSettingItem>(_selectedQuickSetting)) {
        case QuickSettingItem::SETTING_WHEEL_DIAMETER:
            _display.printf("> Lop: %.3fm", _cfg.wheelDiameterM);
            break;
        case QuickSettingItem::SETTING_CURRENT_LIMIT:
            _display.printf("> Dong max: %dA", _cfg.currentLimitA);
            break;
        case QuickSettingItem::SETTING_RTC_HOUR:
            _display.printf("> Gio RTC: %02d", time.hour());
            break;
        case QuickSettingItem::SETTING_RTC_MINUTE:
            _display.printf("> Phut RTC: %02d", time.minute());
            break;
        case QuickSettingItem::SETTING_RESET_TRIP:
            _display.printf("> Trip: %.1fkm (RST)", _tripDistanceKm);
            break;
        default: break;
    }

    _display.setCursor(0, 22);
    _display.print(_isEditMode ? "UP/DN:Chinh | SET:Luu" : "UP/DN:Chon  | SET:Giu");
    _display.display();
}

void DisplayRtcManager::renderAdvancedSettings32() {
    _display.clearDisplay();
    _display.setTextSize(1);

    _display.setCursor(0, 0);
    _display.printf("CAI DAT NANG CAO (P4) %s", _isEditMode ? "[*]" : "");

    _display.setCursor(0, 11);
    switch (static_cast<AdvancedSettingItem>(_selectedAdvSetting)) {
        case AdvancedSettingItem::ADV_LOW_VOLT_CUTOFF:
            _display.printf("> Cat ap yeu: %.1fV", _cfg.lowVoltageCutoff);
            break;
        case AdvancedSettingItem::ADV_OVERHEAT_LIMIT:
            _display.printf("> Canh bao t: %dC", _cfg.overheatLimitC);
            break;
        case AdvancedSettingItem::ADV_DELTA_CELL_MV:
            _display.printf("> Lech cell: %dmV", _cfg.deltaCellMv);
            break;
        case AdvancedSettingItem::ADV_MOTOR_POLES:
            _display.printf("> Cuc motor: %d", _cfg.motorPoles);
            break;
        case AdvancedSettingItem::ADV_GEAR_RATIO:
            _display.printf("> Ti so: %.1f", _cfg.gearRatio);
            break;
        case AdvancedSettingItem::ADV_OLED_CONTRAST:
            _display.printf("> Sang OLED: %d", _cfg.oledContrast);
            break;
        case AdvancedSettingItem::ADV_SPEED_WARN_KMH:
            _display.printf("> Canh bao spd: %d", _cfg.speedWarnKmh);
            break;
        case AdvancedSettingItem::ADV_SPEED_UNIT:
            _display.printf("> Don vi: %s", _cfg.useMph ? "mph" : "km/h");
            break;
        case AdvancedSettingItem::ADV_FACTORY_RESET:
            _display.print("> Reset Mac Dinh");
            break;
        default: break;
    }

    _display.setCursor(0, 22);
    _display.print(_isEditMode ? "UP/DN:Chinh | SET:Luu" : "UP/DN:Chon  | SET:Giu");
    _display.display();
}

void DisplayRtcManager::renderNavigation32(const BleNavData &nav, const DateTime &time) {
    _display.clearDisplay();

    if (nav.isNavActive()) {
        drawNavIcon(0, 6, nav.icon, 20);

        _display.setTextSize(2);
        _display.setCursor(24, 1);
        _display.print(nav.distance);

        _display.setTextSize(1);
        _display.setCursor(24, 19);
        _display.print(nav.instruction);
    } else if (nav.mediaTitle[0] != '\0') {
        _display.setTextSize(1);
        _display.setCursor(0, 0);
        _display.printf("MUSIC %s", nav.isPlaying ? "[>]" : "[||]");
        _display.setCursor(72, 0);
        _display.printf("%02d:%02d", time.hour(), time.minute());

        _display.setCursor(0, 11);
        _display.print(nav.mediaTitle);

        _display.setCursor(0, 22);
        _display.print(nav.mediaArtist);
    } else {
        _display.setTextSize(1);
        _display.setCursor(4, 4);
        _display.print("ANDROID AUTO BLE (P5)");

        _display.setCursor(4, 18);
        _display.print(nav.isConnected ? "Phone: DA KET NOI" : "Phone: CHO BLUETOOTH");
    }

    _display.display();
}

// --- VẼ GIAO DIỆN 128x64 (0.96 INCH) ---

void DisplayRtcManager::renderMainDashboard64(const VotolData &votol, const ANTBMSData &bms, const BleNavData &nav, const VehicleSignals &signals, const DateTime &time) {
    _display.clearDisplay();

    // 1. THANH TRẠNG THÁI TRÊN CÙNG: XI NHAN TRÁI, ĐÈN PHA, GIỜ, PIN, BLE, XI NHAN PHẢI
    drawTurnSignalLeft(1, 0, signals.shouldShowLeftBlink(), 9);
    drawHeadlightIcon(20, 1, signals.isHeadlightOn());

    _display.setTextSize(1);
    _display.setCursor(38, 1);
    _display.printf("%02d:%02d", time.hour(), time.minute());

    _display.setCursor(74, 1);
    _display.printf("[%s%s]", votol.isConnected ? "V" : "-", bms.isConnected ? "B" : "-");

    if (nav.isConnected) {
        drawBleBadge(94, 2, true);
    }

    uint8_t soc = bms.isConnected ? bms.soc : 0;
    drawBatteryIcon(100, 1, 14, 8, soc);

    drawTurnSignalRight(118, 0, signals.shouldShowRightBlink(), 9);

    _display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

    // 2. VẬN TỐC CHÍNH VÀ CẤP SỐ
    float showSpeed = _cfg.useMph ? (votol.speedKmh * 0.621371f) : votol.speedKmh;
    int spd = (int)showSpeed;

    _display.setTextSize(4);
    _display.setCursor(8, 16);
    _display.printf("%2d", spd);

    _display.setTextSize(1);
    _display.setCursor(62, 20);
    _display.print(_cfg.useMph ? "MPH" : "KM/H");
    drawGearBadge(92, 16, votol.getGearString());

    // Cảnh báo quá tốc độ hoặc chỉ đường phụ
    if (spd > _cfg.speedWarnKmh) {
        _display.setCursor(62, 36);
        _display.print("QUÁ TỐC ĐỘ!");
    } else if (nav.isNavActive()) {
        drawNavIcon(62, 34, nav.icon, 14);
        _display.setCursor(80, 36);
        _display.print(nav.distance);
    } else {
        _display.setCursor(62, 36);
        _display.printf("Trip: %4.1f km", _tripDistanceKm);
    }

    _display.drawLine(0, 50, 127, 50, SSD1306_WHITE);

    // 3. THÔNG SỐ ĐIỆN ÁP, DÒNG ĐIỆN, CÔNG SUẤT, NHIỆT ĐỘ
    float showVolt = bms.isConnected ? bms.totalVoltage : votol.voltage;
    float showCurr = bms.isConnected ? bms.current : votol.current;
    _display.setCursor(0, 54);
    _display.printf("%4.1fV", showVolt);
    _display.setCursor(44, 54);
    _display.printf("%4.1fA", showCurr);
    _display.setCursor(86, 54);
    _display.printf("%2dC", votol.controllerTemp);

    _display.display();
}

void DisplayRtcManager::renderBmsDetail64(const ANTBMSData &bms) {
    _display.clearDisplay();
    _display.setTextSize(1);

    _display.setCursor(0, 0);
    _display.printf("BMS DETAIL (P2) %4.1fV", bms.totalVoltage);
    _display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    // Grid 8 Cell theo trang offset
    uint8_t start = _bmsCellPageOffset;
    for (int row = 0; row < 4; row++) {
        int idx1 = start + row;
        int idx2 = start + row + 4;
        _display.setCursor(0, 12 + row * 9);
        if (idx1 < bms.cellCount) {
            _display.printf("C%02d:%3.3f", idx1 + 1, bms.cellVoltages[idx1]);
        }
        _display.setCursor(64, 12 + row * 9);
        if (idx2 < bms.cellCount) {
            _display.printf("C%02d:%3.3f", idx2 + 1, bms.cellVoltages[idx2]);
        }
    }

    _display.drawLine(0, 50, 127, 50, SSD1306_WHITE);
    _display.setCursor(0, 54);
    _display.printf("d:%2.0fmV Pin:%2.0fC FET:%2.0fC",
                   bms.deltaCellVoltage * 1000.0f, bms.getBatteryTemp(), bms.getMosfetTemp());

    _display.display();
}

void DisplayRtcManager::renderQuickSettings64(const DateTime &time) {
    _display.clearDisplay();
    _display.setTextSize(1);

    _display.setCursor(0, 0);
    _display.printf("CAI DAT CO BAN (P3) %s", _isEditMode ? "[SUA]" : "");
    _display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    const char* items[] = {
        "1. Duong kinh banh",
        "2. Gioi han dong xa",
        "3. Chinh gio RTC",
        "4. Chinh phut RTC",
        "5. Reset Trip km"
    };

    for (int i = 0; i < 5; i++) {
        _display.setCursor(0, 12 + i * 8);
        _display.print((_selectedQuickSetting == i) ? ">" : " ");
        _display.print(items[i]);
    }

    _display.drawLine(0, 53, 127, 53, SSD1306_WHITE);
    _display.setCursor(0, 56);
    _display.print(_isEditMode ? "UP/DN:Chinh | SET:Luu" : "UP/DN:Chon | Giu SET:Sua");

    _display.display();
}

void DisplayRtcManager::renderAdvancedSettings64() {
    _display.clearDisplay();
    _display.setTextSize(1);

    _display.setCursor(0, 0);
    _display.printf("CAI DAT CHUYEN SAU (P4) %s", _isEditMode ? "[SUA]" : "");
    _display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    // Hiển thị danh sách cuộn 5 mục tại một thời điểm
    uint8_t startIdx = 0;
    if (_selectedAdvSetting >= 4) {
        startIdx = _selectedAdvSetting - 3;
    }
    if (startIdx + 4 >= static_cast<uint8_t>(AdvancedSettingItem::ADV_SETTING_COUNT)) {
        startIdx = static_cast<uint8_t>(AdvancedSettingItem::ADV_SETTING_COUNT) - 5;
    }

    for (int row = 0; row < 5; row++) {
        uint8_t idx = startIdx + row;
        _display.setCursor(0, 12 + row * 8);
        _display.print((_selectedAdvSetting == idx) ? ">" : " ");

        switch (static_cast<AdvancedSettingItem>(idx)) {
            case AdvancedSettingItem::ADV_LOW_VOLT_CUTOFF:
                _display.printf("Cat ap: %.1fV", _cfg.lowVoltageCutoff);
                break;
            case AdvancedSettingItem::ADV_OVERHEAT_LIMIT:
                _display.printf("Bao qua nhiet: %dC", _cfg.overheatLimitC);
                break;
            case AdvancedSettingItem::ADV_DELTA_CELL_MV:
                _display.printf("Lech cell max: %dmV", _cfg.deltaCellMv);
                break;
            case AdvancedSettingItem::ADV_MOTOR_POLES:
                _display.printf("Cap cuc motor: %d", _cfg.motorPoles);
                break;
            case AdvancedSettingItem::ADV_GEAR_RATIO:
                _display.printf("Ti so truyen: %.1f", _cfg.gearRatio);
                break;
            case AdvancedSettingItem::ADV_OLED_CONTRAST:
                _display.printf("Do sang OLED: %d", _cfg.oledContrast);
                break;
            case AdvancedSettingItem::ADV_SPEED_WARN_KMH:
                _display.printf("Canh bao spd: %d", _cfg.speedWarnKmh);
                break;
            case AdvancedSettingItem::ADV_SPEED_UNIT:
                _display.printf("Don vi: %s", _cfg.useMph ? "MPH" : "KM/H");
                break;
            case AdvancedSettingItem::ADV_FACTORY_RESET:
                _display.print("Reset Cai Dat Goc");
                break;
            default: break;
        }
    }

    _display.drawLine(0, 53, 127, 53, SSD1306_WHITE);
    _display.setCursor(0, 56);
    _display.print(_isEditMode ? "UP/DN:Chinh | SET:Luu" : "UP/DN:Chon | Giu SET:Sua");

    _display.display();
}

void DisplayRtcManager::renderNavigation64(const BleNavData &nav, const DateTime &time) {
    _display.clearDisplay();

    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.printf("HUD NAV (P5) | %02d:%02d", time.hour(), time.minute());

    _display.setCursor(96, 0);
    _display.print(nav.isConnected ? "[BT]" : "[--]");
    _display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    if (nav.isNavActive()) {
        drawNavIcon(4, 14, nav.icon, 28);

        _display.setTextSize(2);
        _display.setCursor(40, 14);
        _display.print(nav.distance);

        _display.setTextSize(1);
        _display.setCursor(40, 34);
        _display.print(nav.instruction);

        _display.drawLine(0, 48, 127, 48, SSD1306_WHITE);
        _display.setCursor(0, 53);
        if (nav.mediaTitle[0] != '\0') {
            _display.printf(">> %s", nav.mediaTitle);
        } else {
            _display.printf("HUONG DAN: %s", nav.getIconName());
        }
    } else if (nav.mediaTitle[0] != '\0') {
        _display.setTextSize(1);
        _display.setCursor(0, 14);
        _display.printf("TRINH PHAT NHAC [%s]", nav.isPlaying ? "PLAY" : "PAUSE");

        _display.setTextSize(2);
        _display.setCursor(0, 26);
        _display.print(nav.mediaTitle);

        _display.setTextSize(1);
        _display.setCursor(0, 44);
        _display.print(nav.mediaArtist);

        _display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
        _display.setCursor(0, 56);
        _display.print("Bluetooth Audio Sync");
    } else {
        _display.setTextSize(1);
        _display.setCursor(14, 16);
        _display.print("ANDROID AUTO HUD");

        _display.setCursor(6, 30);
        _display.print(nav.isConnected ? "Phone: DA KET NOI!" : "Phone: CHO BLUETOOTH");
        _display.setCursor(6, 42);
        _display.print(nav.isConnected ? "Mo Google Maps / Navi" : "Quet BLE: ESP32-Dash");

        _display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
        _display.setCursor(0, 56);
        _display.print("Nhan SET: Chuyen trang");
    }

    _display.display();
}

void DisplayRtcManager::renderWelcomeScreen() {
    _display.clearDisplay();
    _display.drawRect(0, 0, 128, OLED_SCREEN_HEIGHT, SSD1306_WHITE);
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    if (OLED_SCREEN_HEIGHT == 32) {
        _display.setCursor(24, 6);
        _display.print("* SMART EV *");
        _display.setCursor(28, 18);
        _display.print("-- WELCOME --");
    } else {
        _display.setTextSize(2);
        _display.setCursor(14, 14);
        _display.print("SMART EV");
        _display.setTextSize(1);
        _display.setCursor(26, 40);
        _display.print("-- WELCOME --");
    }
    _display.display();
}

void DisplayRtcManager::renderWaitingBleScreen(const DateTime &time) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    // Tiêu đề
    _display.setCursor(16, 2);
    _display.print("ESP32-SmartDash");
    _display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

    // Biểu tượng BLE chớp nháy
    bool blink = (millis() / 500) % 2;
    if (blink) {
        drawBleBadge(8, 18, true);
    }

    _display.setCursor(20, 18);
    _display.print("CHO KET NOI BLE..");

    _display.display();
}

void DisplayRtcManager::renderClockScreen(const DateTime &time, bool bleConnected, uint8_t soc, float speedKmh) {
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);

    // 1. Giờ : Phút : Giây số to rõ nét (Size 2)
    _display.setTextSize(2);
    _display.setCursor(16, 4);
    _display.printf("%02d:%02d:%02d", time.hour(), time.minute(), time.second());

    // 2. Dòng trạng thái dưới cùng: Ngày/Tháng + Biểu tượng BLE OK + Pin / Vận tốc
    _display.setTextSize(1);
    _display.setCursor(4, 23);
    _display.printf("%02d/%02d", time.day(), time.month());

    // Icon BLE
    drawBleBadge(44, 23, bleConnected);
    _display.setCursor(52, 23);
    _display.print("BLE OK");

    // % Pin nếu có
    if (soc > 0) {
        _display.setCursor(96, 23);
        _display.printf("%d%%", soc);
    } else if (speedKmh > 0.5f) {
        _display.setCursor(90, 23);
        _display.printf("%dkm", (int)speedKmh);
    }

    _display.display();
}

void DisplayRtcManager::update(const VotolData &votol, const ANTBMSData &bms, const BleNavData &nav, const VehicleSignals &signals) {
    if (!_oledReady) return;

    uint32_t now = millis();
    if (now - _lastRenderMs < OLED_REFRESH_INTERVAL_MS) {
        return;
    }
    uint32_t deltaMs = now - _lastRenderMs;
    _lastRenderMs = now;

    // Cập nhật quãng đường Trip
    updateTrip(votol.speedKmh, deltaMs);
    DateTime curTime = getDateTime();

    // 1. Giai đoạn khởi động: Hiện màn hình WELCOME trong 2.5s đầu
    if (now < 2500) {
        renderWelcomeScreen();
        return;
    }

    // 2. Nếu CHƯA KẾT NỐI BLE -> Chế độ chờ kết nối
    if (!nav.isConnected) {
        renderWaitingBleScreen(curTime);
        return;
    }

    // 3. Nếu ĐÃ KẾT NỐI BLE -> Tự động chuyển qua HIỂN THỊ ĐỒNG HỒ RTC!
    uint8_t soc = bms.isConnected ? bms.soc : 0;
    renderClockScreen(curTime, nav.isConnected, soc, votol.speedKmh);

    // Popup cuộc gọi đến nếu có
    if (nav.hasIncomingCall && (now - nav.callPopupStartTime < 5000)) {
        drawCallOverlay(nav.incomingCall);
        _display.display();
    }
}

void DisplayRtcManager::update(const VotolData &votol, const ANTBMSData &bms, const BleNavData &nav) {
    VehicleSignals emptySignals;
    update(votol, bms, nav, emptySignals);
}

void DisplayRtcManager::update(const VotolData &votol, const ANTBMSData &bms) {
    BleNavData emptyNav;
    VehicleSignals emptySignals;
    update(votol, bms, emptyNav, emptySignals);
}
