#pragma once
#include <Arduino.h>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

#define ANT_BMS_MAX_CELLS 32
#define ANT_BMS_MAX_TEMPS 6
#define ANT_BMS_FRAME_LEN 140

/**
 * ============================================================================
 * STRUCT DỮ LIỆU BÓC TÁCH TỪ BMS ANT (140 BYTES FRAME)
 * Theo kỹ năng votol-bms-reader và project-plan.md Task 2.2
 * ============================================================================
 */
struct ANTBMSData {
    // Thông số điện năng chính
    float totalVoltage;             // Điện áp tổng khối pin (V)
    float current;                  // Dòng xả (+) hoặc sạc (-) (A)
    float power;                    // Công suất tức thời (W)
    uint8_t soc;                    // Dung lượng pin còn lại (% SoC, 0 - 100)
    float capacityRemaining;        // Dung lượng còn lại (Ah)
    float totalCapacity;            // Dung lượng danh định (Ah)
    float cycleCapacity;            // Dung lượng chu kỳ tích lũy (Ah)
    uint32_t uptimeSeconds;         // Thời gian hoạt động liên tục (giây)

    // Thông số từng Cell Pin
    uint8_t cellCount;              // Số lượng cell pin thực tế (VD: 16S, 20S, 24S...)
    float cellVoltages[ANT_BMS_MAX_CELLS]; // Mảng điện áp từng cell (V, độ phân giải 1mV)
    float maxCellVoltage;           // Điện áp cell cao nhất (V)
    uint8_t maxCellIndex;           // Vị trí cell cao nhất (1-based: Cell 1..32)
    float minCellVoltage;           // Điện áp cell thấp nhất (V)
    uint8_t minCellIndex;           // Vị trí cell thấp nhất (1-based: Cell 1..32)
    float deltaCellVoltage;         // Độ lệch áp lớn nhất giữa các cell (V hoặc mV)
    float averageCellVoltage;       // Điện áp trung bình các cell (V)

    // Cảm biến nhiệt độ (6 cảm biến)
    float temperatures[ANT_BMS_MAX_TEMPS]; // [0]: MOSFET, [1]: Mạch cân bằng, [2..5]: Pin
    float getBatteryTemp() const;   // Lấy nhiệt độ khối pin lớn nhất (°C)
    float getMosfetTemp() const;    // Lấy nhiệt độ MOSFET công suất (°C)

    // Trạng thái vận hành & bảo vệ MOSFET
    bool chargeMosfet;              // Cờ đóng ngắt sạc (true = ON / Cho phép)
    bool dischargeMosfet;           // Cờ đóng ngắt xả (true = ON / Cho phép)
    bool isBalancing;               // Có cell nào đang được cân bằng không
    uint32_t balancedCellBitmask;   // Bitmask các cell đang cân bằng

    // Thống kê kết nối
    bool isConnected;               // Trạng thái kết nối BMS
    uint32_t lastReceivedMs;        // Timestamp millis() lần nhận gói tin gần nhất
    uint32_t validPacketsCount;     // Số gói tin nhận đúng
    uint32_t checksumErrorsCount;   // Số gói tin sai checksum
};

/**
 * ============================================================================
 * CLASS XỬ LÝ GIAO THỨC UART BMS ANT (NON-BLOCKING + THREAD-SAFE MUTEX)
 * ============================================================================
 */
class AntBmsProtocolHandler {
public:
    using DataCallback = std::function<void(const ANTBMSData&)>;

    AntBmsProtocolHandler();
    ~AntBmsProtocolHandler();

    // Khởi tạo UART2 kết nối BMS ANT
    void begin(HardwareSerial &serialPort,
               int8_t rxPin = PIN_BMS_RX,
               int8_t txPin = PIN_BMS_TX,
               uint32_t baudRate = BMS_BAUD_RATE);

    // Quét buffer UART không chặn (chạy trên FreeRTOS Core 0)
    void update();

    // Lấy bản sao an toàn dữ liệu đa luồng cho Core 1 vẽ OLED
    bool getSnapshot(ANTBMSData &outData, TickType_t waitTicks = pdMS_TO_TICKS(10));

    // Gửi lệnh kích hoạt truy vấn dữ liệu từ BMS ANT
    void sendPollQuery();

    // Đăng ký callback khi có gói tin mới
    void onData(DataCallback callback) { _callback = callback; }

    // Tính Checksum 16-bit tổng (Sum of bytes 4..137)
    static uint16_t calcChecksum(const uint8_t* buffer, size_t length);

private:
    HardwareSerial* _serial;
    ANTBMSData _data;
    DataCallback _callback;

    // Mutex bảo vệ struct _data
    SemaphoreHandle_t _mutex;

    // Bộ đệm trượt non-blocking
    static constexpr size_t RX_BUFFER_SIZE = 256;
    uint8_t _rxBuffer[RX_BUFFER_SIZE];
    size_t _rxLen;

    uint32_t _lastPollMs;

    // Hàm nội bộ giải mã khung 140 bytes
    bool parseFrame(const uint8_t* frame, size_t len);
    void checkConnectionTimeout();
};
