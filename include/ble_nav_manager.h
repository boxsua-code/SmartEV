#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"
#include "votol_protocol.h"
#include "ant_bms_protocol.h"
#include "vehicle_signals.h"

/**
 * ============================================================================
 * LOẠI ICON ĐIỀU HƯỚNG DẪN ĐƯỜNG (TURN-BY-TURN NAVIGATION)
 * ============================================================================
 */
enum class NavIconType : uint8_t {
    NONE = 0,       // Không có chỉ đường
    STRAIGHT = 1,   // Đi thẳng ⬆
    TURN_LEFT = 2,  // Rẽ trái ⬅
    TURN_RIGHT = 3, // Rẽ phải ➡
    SLIGHT_LEFT = 4,// Chếch trái ↖
    SLIGHT_RIGHT = 5,// Chếch phải ↗
    U_TURN = 6,     // Quay đầu ↩
    DESTINATION = 7 // Đích đến 🏁
};

/**
 * ============================================================================
 * CẤU TRÚC DỮ LIỆU ĐIỀU HƯỚNG & MEDIA TỪ ĐIỆN THOẠI (BLE SNAPSHOT)
 * ============================================================================
 */
struct BleNavData {
    bool isConnected = false;         // Cờ kết nối BLE với điện thoại
    bool hasNav = false;              // Đang có dữ liệu chỉ đường
    NavIconType icon = NavIconType::NONE;
    char distance[16] = {0};          // Khoảng cách tới ngã rẽ ("150 m", "1.2 km")
    char instruction[64] = {0};       // Tên đường hoặc chỉ dẫn ("Nguyen Hue", "Re trai vao...")

    char mediaTitle[32] = {0};        // Tên bài hát đang phát
    char mediaArtist[32] = {0};       // Tên ca sĩ
    bool isPlaying = false;           // Trạng thái phát nhạc

    char incomingCall[32] = {0};      // Tên hoặc số người gọi đến
    bool hasIncomingCall = false;     // Đang có cuộc gọi đến
    uint32_t callPopupStartTime = 0;  // Thời điểm bắt đầu popup cuộc gọi

    uint32_t lastNavUpdateMs = 0;     // Thời điểm nhận gói tin chỉ đường cuối

    // Kiểm tra xem chỉ dẫn có bị timeout không (mặc định quá 60s không cập nhật -> hết dẫn đường)
    bool isNavActive() const {
        if (!hasNav) return false;
        if (millis() - lastNavUpdateMs > 60000) return false;
        return true;
    }

    const char* getIconName() const {
        switch (icon) {
            case NavIconType::STRAIGHT:     return "DI THANG";
            case NavIconType::TURN_LEFT:    return "RE TRAI";
            case NavIconType::TURN_RIGHT:   return "RE PHAI";
            case NavIconType::SLIGHT_LEFT:  return "CHECH TRAI";
            case NavIconType::SLIGHT_RIGHT: return "CHECH PHAI";
            case NavIconType::U_TURN:       return "QUAY DAU";
            case NavIconType::DESTINATION:  return "DICH DEN";
            default:                        return "";
        }
    }
};

/**
 * ============================================================================
 * CLASS QUẢN LÝ BLE NAVIGATION (GATT SERVER NORDIC UART SERVICE)
 * Chạy trên Core 0, bảo vệ an toàn đa luồng bằng Mutex
 * Đáp ứng Task 2.5 trong project-plan.md
 * ============================================================================
 */
class BleNavManager {
public:
    BleNavManager();
    ~BleNavManager();

    // Khởi tạo BLE Server & Quảng bá thiết bị
    void begin(const char* deviceName = BLE_DEVICE_NAME);

    // Lấy bản sao dữ liệu an toàn đa luồng cho Core 1 render OLED
    bool getSnapshot(BleNavData &out);

    // Gửi thông số xe (Votol, BMS & Tín hiệu đèn/nút bấm) về điện thoại qua BLE TX (Notify)
    void sendTelemetry(const VotolData &vd, const ANTBMSData &bd, const SignalState &sig);

    // Cập nhật ngầm định kỳ (gửi telemetry nếu đến chu kỳ)
    void update(const VotolData &vd, const ANTBMSData &bd, const SignalState &sig);

    // Trạng thái kết nối BLE
    bool isClientConnected() const { return _deviceConnected; }

    // Đánh thức / Khởi động lại quảng bá BLE khi bị ngắt kết nối
    void startAdvertising() {
        BLEDevice::startAdvertising();
    }

    // Phương thức mô phỏng dữ liệu qua Serial Monitor để kiểm thử (Test Simulation)
    void simulateNav(NavIconType icon, const char* dist, const char* instruction);
    void simulateMedia(const char* title, const char* artist, bool isPlaying);
    void simulateCall(const char* caller);
    void clearNav();
    void clearCall();

    // Parser xử lý gói tin văn bản nhận được từ BLE RX
    void parseIncomingMessage(const String &msg);

    // Callback đồng bộ thời gian từ điện thoại
    using TimeSyncCallback = std::function<void(uint16_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t)>;
    void onTimeSync(TimeSyncCallback callback) { _timeSyncCallback = callback; }

    // Callback điều khiển UART từ App điện thoại
    using CommandCallback = std::function<void()>;
    using SetBaudCallback = std::function<void(uint32_t)>;
    void onPollVotol(CommandCallback cb) { _onPollVotolCallback = cb; }
    void onSetBaud(SetBaudCallback cb) { _onSetBaudCallback = cb; }
    void onSwapUart(CommandCallback cb) { _onSwapUartCallback = cb; }

    // Các hàm callback nội bộ
    void onConnect();
    void onDisconnect();

private:
    BLEServer* _pServer;
    BLECharacteristic* _pTxCharacteristic;
    BLECharacteristic* _pRxCharacteristic;

    bool _deviceConnected;
    bool _oldDeviceConnected;
    uint32_t _lastTelemetrySendMs;

    BleNavData _data;
    SemaphoreHandle_t _mutex;
    TimeSyncCallback _timeSyncCallback = nullptr;
    CommandCallback _onPollVotolCallback = nullptr;
    SetBaudCallback _onSetBaudCallback = nullptr;
    CommandCallback _onSwapUartCallback = nullptr;

    NavIconType parseIconType(const String &str);
};
