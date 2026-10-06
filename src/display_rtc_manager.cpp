#include "display_rtc_manager.h"

DisplayRtcManager::DisplayRtcManager()
    : _display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, -1),
      _oledReady(false), _rtcReady(false),
      _lastRenderMs(0), _lastRtcReadMs(0),
      _currentPage(ScreenPage::PAGE_CLOCK),
      _currentSetupPage(SetupSubPage::SETUP_PAGE_POWER),
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

// --- XỬ LÝ NÚT BẤM (CHUYỂN 4 TRANG CHÍNH & 4 TRANG CÀI ĐẶT NÂNG CAO) ---

void DisplayRtcManager::handleButtons(ButtonEvent setEvt, ButtonEvent upEvt, ButtonEvent downEvt, VotolProtocolHandler &votol) {
    // 1. Nhấn giữ nút GPIO 4 (>800ms): Chuyển vào / Thoát khỏi Menu Cài Đặt Nâng Cao
    if (setEvt == ButtonEvent::LONG_PRESS) {
        if (_currentPage != ScreenPage::PAGE_SETUP) {
            _currentPage = ScreenPage::PAGE_SETUP;
            _currentSetupPage = SetupSubPage::SETUP_PAGE_POWER;
            Serial.println("[FSM] Đã vào MENU CÀI ĐẶT NÂNG CAO (Gồm 4 trang chuẩn VotolAIO)");
        } else {
            saveConfigToNVS();
            applyHardwareConfig(votol);
            _currentPage = ScreenPage::PAGE_CLOCK;
            Serial.println("[FSM] Đã LƯU cấu hình và quay lại Màn hình chính");
        }
    } 
    // 2. Nhấn 1 lần nút GPIO 4: Chuyển tiếp tuần tự trang
    else if (setEvt == ButtonEvent::CLICK) {
        if (_currentPage != ScreenPage::PAGE_SETUP) {
            // Chuyển 4 trang chính: Trang 1 (Clock) -> Trang 2 (Temps) -> Trang 3 (BMS) -> Trang 4 (Power) -> Trang 1
            uint8_t next = (static_cast<uint8_t>(_currentPage) + 1) % 4;
            _currentPage = static_cast<ScreenPage>(next);
            Serial.printf("[FSM] Nút GPIO 4: Chuyển sang Trang Vận Hành %d / 4\n", next + 1);
        } else {
            // Chuyển 4 trang cài đặt: Trang 1 (Power) -> Trang 2 (Safety) -> Trang 3 (Motor) -> Trang 4 (System) -> Trang 1
            uint8_t nextSetup = (static_cast<uint8_t>(_currentSetupPage) + 1) % 4;
            _currentSetupPage = static_cast<SetupSubPage>(nextSetup);
            Serial.printf("[FSM] Nút GPIO 4: Chuyển sang Trang Cài Đặt %d / 4\n", nextSetup + 1);
        }
    }
}

bool DisplayRtcManager::begin(int8_t sdaPin, int8_t sclPin, uint32_t freq) {
    Serial.printf("[INIT] Bắt đầu khởi tạo Bus I2C (SDA: GPIO %d, SCL: GPIO %d, Freq: %d Hz)...\n",
                  sdaPin, sclPin, freq);

    // 1. Tải cấu hình từ Flash NVS
    loadConfigFromNVS();

    // 2. Khởi tạo Bus I2C
    Wire.begin(sdaPin, sclPin, freq);
    Wire.setTimeOut(50); // Timeout 50ms chống kẹt treo CPU nếu không cắm màn hình

    // 3. Khởi tạo RTC DS3231
    if (_rtc.begin()) {
        _rtcReady = true;
        if (_rtc.lostPower()) {
            Serial.println("[RTC] DS3231 mất nguồn dự phòng! Đặt giờ mặc định compile time.");
            _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        _lastCachedTime = _rtc.now();
        Serial.printf("[RTC] DS3231 sẵn sàng! Giờ hiện tại: %02d:%02d:%02d\n",
                      _lastCachedTime.hour(), _lastCachedTime.minute(), _lastCachedTime.second());
    } else {
        _rtcReady = false;
        Serial.println("[RTC] Không tìm thấy module RTC DS3231 trên bus I2C!");
    }

    // 4. Khởi tạo màn hình OLED SSD1306 (Kiểm tra ACK trước để không bị treo)
    Wire.beginTransmission(OLED_I2C_ADDRESS);
    if (Wire.endTransmission() == 0) {
        if (_display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
            _oledReady = true;
            _display.clearDisplay();
            _display.setTextColor(SSD1306_WHITE);
            _display.ssd1306_command(SSD1306_SETCONTRAST);
            _display.ssd1306_command(_cfg.oledContrast);
            _display.display();
            Serial.println("[OLED] SSD1306 khởi tạo thành công (128x32)!");
        } else {
            _oledReady = false;
            Serial.println("[OLED] Khởi tạo màn hình OLED SSD1306 thất bại!");
        }
    } else {
        _oledReady = false;
        Serial.println("[OLED] Không phát hiện màn hình OLED tại địa chỉ I2C 0x3C (bỏ qua an toàn)!");
    }

    return _oledReady;
}

void DisplayRtcManager::showSplashScreen() {
    if (!_oledReady) return;
    renderWelcomeScreen();
    delay(1200);
}

void DisplayRtcManager::showMessage(const char* title, const char* subtitle, uint16_t durationMs) {
    if (!_oledReady) return;
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
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
    return 25.0f;
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
        _display.drawRoundRect(x, y, 7, 8, 3, SSD1306_WHITE);
        _display.fillRoundRect(x + 1, y + 1, 5, 6, 2, SSD1306_WHITE);
        _display.drawFastHLine(x + 8, y + 1, 4, SSD1306_WHITE);
        _display.drawFastHLine(x + 8, y + 4, 5, SSD1306_WHITE);
        _display.drawFastHLine(x + 8, y + 7, 4, SSD1306_WHITE);
    }
}

void DisplayRtcManager::drawNavIcon(int16_t x, int16_t y, NavIconType icon, uint8_t size) {
    if (!_oledReady) return;
    uint8_t half = size / 2;
    switch (icon) {
        case NavIconType::STRAIGHT:
            _display.fillTriangle(x + half, y, x + 2, y + half, x + size - 2, y + half, SSD1306_WHITE);
            _display.fillRect(x + half - 2, y + half, 5, half, SSD1306_WHITE);
            break;
        case NavIconType::TURN_LEFT:
            _display.fillTriangle(x, y + half, x + half, y + 2, x + half, y + size - 2, SSD1306_WHITE);
            _display.fillRect(x + half - 1, y + half - 2, half - 1, 5, SSD1306_WHITE);
            break;
        case NavIconType::TURN_RIGHT:
            _display.fillTriangle(x + size, y + half, x + half, y + 2, x + half, y + size - 2, SSD1306_WHITE);
            _display.fillRect(x + 2, y + half - 2, half, 5, SSD1306_WHITE);
            break;
        default:
            _display.fillCircle(x + half, y + half, 3, SSD1306_WHITE);
            break;
    }
}

void DisplayRtcManager::drawCallOverlay(const char* caller) {
    if (!_oledReady) return;
    _display.fillRect(2, 2, 124, 28, SSD1306_BLACK);
    _display.drawRect(2, 2, 124, 28, SSD1306_WHITE);
    _display.setTextSize(1);
    _display.setCursor(18, 5);
    _display.print("CUOC GOI DEN!");
    _display.setCursor(6, 17);
    _display.print(caller ? caller : "Khong xac dinh");
}

// --- 4 TRANG HIỂN THỊ CHÍNH (CHUẨN VOTOLAIO) ---

void DisplayRtcManager::renderPageClock(const DateTime &time, const VotolData &votol, const ANTBMSData &bms, const VehicleSignals &signals, bool bleConnected) {
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);

    // 1. Tín hiệu xi nhan và đèn pha
    drawTurnSignalLeft(0, 0, signals.shouldShowLeftBlink(), 7);
    drawTurnSignalRight(120, 0, signals.shouldShowRightBlink(), 7);
    drawHeadlightIcon(38, 0, signals.isHeadlightOn());

    // 2. Giờ phút lớn (Size 2)
    _display.setTextSize(2);
    _display.setCursor(14, 4);
    _display.printf("%02d:%02d", time.hour(), time.minute());

    // Giây nhỏ
    _display.setTextSize(1);
    _display.setCursor(76, 4);
    _display.printf(":%02d", time.second());

    // Cấp số xe (P, ECO, D, S, R)
    drawGearBadge(96, 2, votol.getGearString());

    // 3. Dòng dưới: Thứ + Ngày/Tháng + BLE OK + Chỉ báo Trang [P1]
    static const char* DAY_NAMES_VN[] = { "CN", "HAI", "BA", "TU", "NAM", "SAU", "BAY" };
    static const char* MONTH_NAMES_EN[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

    uint8_t dIdx = time.dayOfTheWeek() % 7;
    uint8_t mIdx = (time.month() >= 1 && time.month() <= 12) ? (time.month() - 1) : 0;

    _display.setTextSize(1);
    _display.setCursor(0, 23);
    _display.printf("%s %02d %s", DAY_NAMES_VN[dIdx], time.day(), MONTH_NAMES_EN[mIdx]);

    drawBleBadge(82, 23, bleConnected);
    _display.setCursor(92, 23);
    _display.print(bleConnected ? "BT" : "--");

    _display.setCursor(110, 23);
    _display.print("P1");

    _display.display();
}

void DisplayRtcManager::renderPageTemps(const VotolData &votol, const ANTBMSData &bms) {
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);

    // Header
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.print("NHIET DO (C)     [P2]");
    _display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

    // 3 Nhãn: ECU, MOTOR, BATT
    _display.setCursor(6, 12);
    _display.print("ECU");
    _display.setCursor(48, 12);
    _display.print("MOTOR");
    _display.setCursor(92, 12);
    _display.print("BATT");

    // 3 Giá trị nhiệt độ (Size 2)
    _display.setTextSize(2);
    _display.setCursor(4, 21);
    _display.printf("%2d", votol.controllerTemp);

    _display.setCursor(48, 21);
    _display.printf("%2d", votol.motorTemp);

    float bTemp = bms.isConnected ? bms.getBatteryTemp() : getRtcTemperature();
    _display.setCursor(92, 21);
    _display.printf("%2.0f", bTemp);

    _display.display();
}

void DisplayRtcManager::renderPageBms(const ANTBMSData &bms, const VotolData &votol) {
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);

    float volt = bms.isConnected ? bms.totalVoltage : votol.voltage;
    float curr = bms.isConnected ? bms.current : votol.current;
    uint8_t soc = bms.isConnected ? bms.soc : 0;

    // Hàng trên: Nhãn VOLT + Điện áp pin
    _display.setTextSize(1);
    _display.setCursor(0, 2);
    _display.print("VOLT");

    _display.setTextSize(2);
    _display.setCursor(30, 0);
    _display.printf("%5.1fV", volt);

    _display.setTextSize(1);
    _display.setCursor(104, 2);
    _display.print("[P3]");

    // Hàng dưới: Nhãn ARUS + Dòng điện (+ khi sạc, - khi xả)
    _display.setCursor(0, 20);
    _display.print("ARUS");

    _display.setTextSize(2);
    _display.setCursor(30, 17);
    _display.printf("%+5.1fA", curr);

    _display.setTextSize(1);
    _display.setCursor(104, 20);
    _display.printf("%2d%%", soc);

    _display.display();
}

void DisplayRtcManager::renderPagePower(const VotolData &votol, const ANTBMSData &bms) {
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);

    float volt = bms.isConnected ? bms.totalVoltage : votol.voltage;
    float curr = bms.isConnected ? bms.current : votol.current;
    float powerW = volt * curr;

    // Hàng trên: Công suất WATT hoặc kW (Size 2)
    _display.setTextSize(2);
    _display.setCursor(0, 0);
    if (fabs(powerW) >= 1000.0f) {
        _display.printf("%+4.2fkW", powerW / 1000.0f);
    } else {
        _display.printf("%+5.0fW", powerW);
    }

    _display.setTextSize(1);
    _display.setCursor(104, 2);
    _display.print("[P4]");

    // Hàng dưới: Tốc độ km/h + Cấp số
    _display.setCursor(0, 20);
    _display.printf("SPD:%3.0f km/h", votol.speedKmh);

    drawGearBadge(94, 18, votol.getGearString());

    _display.display();
}

// --- 4 TRANG CÀI ĐẶT NÂNG CAO (CHUẨN VOTOLAIO) ---

void DisplayRtcManager::renderPageSetup(SetupSubPage subPage) {
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);

    switch (subPage) {
        case SetupSubPage::SETUP_PAGE_POWER:
            _display.setCursor(0, 0);
            _display.print("[CAI DAT 1/4: NGUON]");
            _display.drawFastHLine(0, 9, 128, SSD1306_WHITE);
            _display.setCursor(0, 12);
            _display.printf("Cat ap yeu : %.1f V", _cfg.lowVoltageCutoff);
            _display.setCursor(0, 22);
            _display.printf("Dong xa max: %d A", _cfg.currentLimitA);
            break;

        case SetupSubPage::SETUP_PAGE_SAFETY:
            _display.setCursor(0, 0);
            _display.print("[CAI DAT 2/4: AN TOAN]");
            _display.drawFastHLine(0, 9, 128, SSD1306_WHITE);
            _display.setCursor(0, 12);
            _display.printf("Bao qua nhiet: %d C", _cfg.overheatLimitC);
            _display.setCursor(0, 22);
            _display.printf("Canh bao spd : %d kmh", _cfg.speedWarnKmh);
            break;

        case SetupSubPage::SETUP_PAGE_MOTOR:
            _display.setCursor(0, 0);
            _display.print("[CAI DAT 3/4: MOTOR]");
            _display.drawFastHLine(0, 9, 128, SSD1306_WHITE);
            _display.setCursor(0, 12);
            _display.printf("Banh xe: %.4f m", _cfg.wheelDiameterM);
            _display.setCursor(0, 22);
            _display.printf("Poles: %d | Gear: %.1f", _cfg.motorPoles, _cfg.gearRatio);
            break;

        case SetupSubPage::SETUP_PAGE_SYSTEM:
            _display.setCursor(0, 0);
            _display.print("[CAI DAT 4/4: HE THONG]");
            _display.drawFastHLine(0, 9, 128, SSD1306_WHITE);
            _display.setCursor(0, 12);
            _display.printf("Lech Cell: %d mV", _cfg.deltaCellMv);
            _display.setCursor(0, 22);
            _display.printf("OLED: %d | Factory RST", _cfg.oledContrast);
            break;

        default:
            break;
    }

    _display.display();
}

void DisplayRtcManager::renderWelcomeScreen() {
    _display.clearDisplay();
    _display.drawRect(0, 0, 128, OLED_SCREEN_HEIGHT, SSD1306_WHITE);
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    _display.setCursor(24, 6);
    _display.print("* SMART EV *");
    _display.setCursor(28, 18);
    _display.print("-- WELCOME --");

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

    // 1. Màn hình khởi động chào mừng trong 1.5 giây đầu
    if (now < 1500) {
        renderWelcomeScreen();
        return;
    }

    // 2. Hiển thị trang hiện tại theo FSM (Chuyển trang bằng nút GPIO 4)
    switch (_currentPage) {
        case ScreenPage::PAGE_CLOCK:
            renderPageClock(curTime, votol, bms, signals, nav.isConnected);
            break;
        case ScreenPage::PAGE_TEMPS:
            renderPageTemps(votol, bms);
            break;
        case ScreenPage::PAGE_BMS:
            renderPageBms(bms, votol);
            break;
        case ScreenPage::PAGE_POWER:
            renderPagePower(votol, bms);
            break;
        case ScreenPage::PAGE_SETUP:
            renderPageSetup(_currentSetupPage);
            break;
        default:
            _currentPage = ScreenPage::PAGE_CLOCK;
            renderPageClock(curTime, votol, bms, signals, nav.isConnected);
            break;
    }

    // 3. Popup cuộc gọi đến nếu có
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
