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

// Định nghĩa 5 trang hiển thị (FSM Pages)
enum class ScreenPage : uint8_t {
    PAGE_MAIN_DASHBOARD = 0,    // Trang 1: Đồng hồ chính + Xi nhan + Đèn pha
    PAGE_BMS_DETAIL,            // Trang 2: Chi tiết 32 Cell Pin & Nhiệt độ BMS
    PAGE_QUICK_SETTINGS,        // Trang 3: Cài đặt nhanh (Bánh xe, Dòng xả, Giờ RTC, Reset Trip)
    PAGE_ADVANCED_SETTINGS,     // Trang 4: Cài đặt chuyên sâu (Cắt áp, Quá nhiệt, Cặp cực, OLED, v.v.)
    PAGE_NAVIGATION,            // Trang 5: Dẫn đường Android Auto / Media BLE
    PAGE_COUNT
};

// Các mục trong menu cài đặt nhanh (Quick Settings)
enum class QuickSettingItem : uint8_t {
    SETTING_WHEEL_DIAMETER = 0, // Đường kính bánh xe (m)
    SETTING_CURRENT_LIMIT,     // Giới hạn dòng (A)
    SETTING_RTC_HOUR,          // Cài đặt giờ RTC
    SETTING_RTC_MINUTE,        // Cài đặt phút RTC
    SETTING_RESET_TRIP,        // Reset quãng đường Trip
    QUICK_SETTING_COUNT
};

// Các mục trong menu cài đặt chuyên sâu (Advanced Settings)
enum class AdvancedSettingItem : uint8_t {
    ADV_LOW_VOLT_CUTOFF = 0,   // Ngưỡng cắt áp bảo vệ pin yếu (V)
    ADV_OVERHEAT_LIMIT,        // Cảnh báo quá nhiệt IC / Motor (°C)
    ADV_DELTA_CELL_MV,         // Ngưỡng cảnh báo lệch Cell (mV)
    ADV_MOTOR_POLES,           // Số cặp cực động cơ điện
    ADV_GEAR_RATIO,            // Tỉ số truyền (1.0 = Hub Motor)
    ADV_OLED_CONTRAST,         // Độ sáng màn hình OLED (10 - 255)
    ADV_SPEED_WARN_KMH,        // Cảnh báo quá tốc độ (km/h)
    ADV_SPEED_UNIT,            // Đơn vị tốc độ (0: km/h, 1: mph)
    ADV_FACTORY_RESET,         // Khôi phục cài đặt gốc
    ADV_SETTING_COUNT
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
    uint8_t _selectedQuickSetting;
    uint8_t _selectedAdvSetting;
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

    // Vẽ giao diện cho màn hình 128x32 (0.91 inch)
    void renderMainDashboard32(const VotolData &votol, const ANTBMSData &bms, const BleNavData &nav, const VehicleSignals &signals, const DateTime &time);
    void renderBmsDetail32(const ANTBMSData &bms);
    void renderQuickSettings32(const DateTime &time);
    void renderAdvancedSettings32();
    void renderNavigation32(const BleNavData &nav, const DateTime &time);

    // Vẽ giao diện cho màn hình 128x64 (0.96 inch)
    void renderMainDashboard64(const VotolData &votol, const ANTBMSData &bms, const BleNavData &nav, const VehicleSignals &signals, const DateTime &time);
    void renderBmsDetail64(const ANTBMSData &bms);
    void renderQuickSettings64(const DateTime &time);
    void renderAdvancedSettings64();
    void renderNavigation64(const BleNavData &nav, const DateTime &time);

    // Các chế độ hiển thị tinh gọn: Welcome, Chờ BLE, Đồng hồ RTC khi đã kết nối
    void renderWelcomeScreen();
    void renderWaitingBleScreen(const DateTime &time);
    void renderClockScreen(const DateTime &time, bool bleConnected, uint8_t soc, float speedKmh);

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
