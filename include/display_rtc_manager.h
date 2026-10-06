#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <Preferences.h>
#include "config.h"
#include "votol_protocol.h"
#include "ant_bms_protocol.h"
#include "button_controller.h"
#include "ble_nav_manager.h"
#include "vehicle_signals.h"

// 4 Trang Hiển Thị Chính chuẩn VotolAIO
enum class ScreenPage : uint8_t {
    PAGE_CLOCK = 0,         // Trang 1: Đồng hồ RTC lớn (Giờ:Phút, Thứ, Ngày/Tháng, Năm)
    PAGE_TEMPS,             // Trang 2: Bộ 3 nhiệt độ (ECU, Motor, Pin Battery)
    PAGE_BMS,               // Trang 3: Dữ liệu BMS (Điện áp Volt & Dòng điện Arus có dấu nạp/xả)
    PAGE_POWER,             // Trang 4: Công suất (Watt / kW) & Vận tốc km/h / Cấp số
    PAGE_SETUP,             // Màn hình Cài Đặt Nâng Cao (4 trang setup chuẩn VotolAIO)
    PAGE_COUNT
};

// 4 Trang Cài Đặt Nâng Cao Chuẩn VotolAIO
enum class SetupSubPage : uint8_t {
    SETUP_PAGE_POWER = 0,   // Trang Cài Đặt 1: Cắt áp pin yếu (V) & Giới hạn dòng (A)
    SETUP_PAGE_SAFETY,      // Trang Cài Đặt 2: Quá nhiệt IC/Motor (°C) & Cảnh báo tốc độ (km/h)
    SETUP_PAGE_MOTOR,       // Trang Cài Đặt 3: Bánh xe (m), Cặp cực motor, Tỉ số truyền
    SETUP_PAGE_SYSTEM,      // Trang Cài Đặt 4: Lệch cell (mV), Độ sáng OLED, Khôi phục gốc
    SETUP_PAGE_COUNT
};

// Cấu trúc cấu hình xe điện lưu trong bộ nhớ Flash NVS
struct VehicleConfig {
    float wheelDiameterM = DEFAULT_WHEEL_DIAMETER_M;
    uint16_t currentLimitA = 45;
    float lowVoltageCutoff = DEFAULT_LOW_VOLTAGE_CUTOFF;
    uint8_t overheatLimitC = DEFAULT_OVERHEAT_LIMIT_C;
    uint16_t deltaCellMv = DEFAULT_DELTA_CELL_MV;
    uint8_t motorPoles = DEFAULT_MOTOR_POLE_PAIRS;
    float gearRatio = DEFAULT_GEAR_RATIO;
    uint8_t oledContrast = DEFAULT_OLED_CONTRAST;
    uint8_t speedWarnKmh = DEFAULT_SPEED_WARN_KMH;
    bool useMph = false;
};

/**
 * ============================================================================
 * CLASS QUẢN LÝ MÀN HÌNH OLED SSD1306 VÀ RTC DS3231 (I2C) + FSM 5 TRANG
 * Tích hợp Xi nhan Trái/Phải, Đèn Pha, và Cài đặt chuyên sâu
 * ============================================================================
 */
class DisplayRtcManager {
public:
    DisplayRtcManager();

    // Khởi tạo Bus I2C, RTC DS3231 và màn hình OLED SSD1306
    bool begin(int8_t sdaPin = PIN_I2C_SDA, int8_t sclPin = PIN_I2C_SCL, uint32_t freq = I2C_FREQ);

    // Cập nhật giao diện màn hình theo chu kỳ không chặn (chạy trên Core 1)
    void update(const VotolData &votol, const ANTBMSData &bms, const BleNavData &nav, const VehicleSignals &signals);
    void update(const VotolData &votol, const ANTBMSData &bms, const BleNavData &nav); // Overload
    void update(const VotolData &votol, const ANTBMSData &bms);                         // Overload

    // Xử lý sự kiện từ 3 nút bấm (SET, UP, DOWN)
    void handleButtons(ButtonEvent setEvt, ButtonEvent upEvt, ButtonEvent downEvt, VotolProtocolHandler &votol);

    // Chuyển trang trực tiếp
    void setPage(ScreenPage page) { _currentPage = page; }
    ScreenPage getCurrentPage() const { return _currentPage; }

    // Hiển thị màn hình khởi động (Splash Screen)
    void showSplashScreen();

    // Hiển thị thông báo tạm thời lên màn hình
    void showMessage(const char* title, const char* subtitle = nullptr, uint16_t durationMs = 1500);

    // Các hàm giao tiếp với RTC DS3231
    DateTime getDateTime();
    String getFormattedTime(bool showSeconds = false);
    String getFormattedDate();
    float getRtcTemperature();
    void setDateTime(const DateTime &dt);

    // Cập nhật quãng đường Trip (km)
    void updateTrip(float speedKmh, uint32_t deltaMs);
    float getTripKm() const { return _tripDistanceKm; }

    // Lấy cấu hình xe điện hiện tại
    const VehicleConfig& getConfig() const { return _cfg; }

    // Trạng thái phần cứng
    bool isOledReady() const { return _oledReady; }
    bool isRtcReady()  const { return _rtcReady; }

private:
    Adafruit_SSD1306 _display;
    RTC_DS3231 _rtc;
    Preferences _prefs;

    bool _oledReady;
    bool _rtcReady;
    uint32_t _lastRenderMs;
    DateTime _lastCachedTime;
    uint32_t _lastRtcReadMs;

    // FSM State Variables
    ScreenPage _currentPage;
    SetupSubPage _currentSetupPage;
    bool _isEditMode;
    uint8_t _bmsCellPageOffset; // Cuộn xem các cell (0..7, 8..15, 16..23, 24..31)
    float _tripDistanceKm;

    // Đối tượng cấu hình xe
    VehicleConfig _cfg;

    // Quản lý bộ nhớ Non-Volatile Flash (NVS)
    void loadConfigFromNVS();
    void saveConfigToNVS();
    void resetToFactoryDefaults(VotolProtocolHandler &votol);

    // Áp dụng cấu hình phần cứng
    void applyHardwareConfig(VotolProtocolHandler &votol);

    // 4 Trang Hiển Thị Chính (Tương thích 128x32 và 128x64)
    void renderPageClock(const DateTime &time, const VotolData &votol, const ANTBMSData &bms, const VehicleSignals &signals, bool bleConnected);
    void renderPageTemps(const VotolData &votol, const ANTBMSData &bms);
    void renderPageBms(const ANTBMSData &bms, const VotolData &votol);
    void renderPagePower(const VotolData &votol, const ANTBMSData &bms);

    // 4 Trang Cài Đặt Nâng Cao (Chuẩn VotolAIO)
    void renderPageSetup(SetupSubPage subPage);
    void renderWelcomeScreen();

    // Các hàm đồ họa bổ trợ
    void drawBatteryIcon(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t soc);
    void drawGearBadge(int16_t x, int16_t y, const char* gearStr);
    void drawBleBadge(int16_t x, int16_t y, bool isConnected);
    void drawNavIcon(int16_t x, int16_t y, NavIconType icon, uint8_t size = 16);
    void drawCallOverlay(const char* caller);

    // Đồ họa Xi Nhan và Đèn Pha
    void drawTurnSignalLeft(int16_t x, int16_t y, bool active, uint8_t size = 8);
    void drawTurnSignalRight(int16_t x, int16_t y, bool active, uint8_t size = 8);
    void drawHeadlightIcon(int16_t x, int16_t y, bool active);
};
