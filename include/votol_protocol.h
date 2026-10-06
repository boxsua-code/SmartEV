#pragma once
#include <Arduino.h>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

/**
 * ============================================================================
 * ĐỊNH NGHĨA CÁC ENUM VÀ STRUCT DỮ LIỆU IC VOTOL
 * ============================================================================
 */

// Chế độ số xe (Gear)
enum class VotolGear : uint8_t {
    GEAR_UNKNOWN = 0,
    GEAR_PARK,      // P (Đỗ xe / Khóa ga)
    GEAR_ECO,       // Số 1 (Tiết kiệm điện)
    GEAR_DRIVE,     // Số 2 (Chế độ tiêu chuẩn D / Normal)
    GEAR_SPORT,     // Số 3 (Chế độ thể thao Sport / S)
    GEAR_REVERSE    // Số lùi (R)
};

// Trạng thái vận hành IC (Controller State)
enum class VotolState : uint8_t {
    STATE_IDLE = 0,   // Chờ (0)
    STATE_INIT,       // Khởi tạo (1)
    STATE_START,      // Bắt đầu (2)
    STATE_RUN,        // Đang chạy (3)
    STATE_STOP,       // Dừng (4)
    STATE_BRAKE,      // Đang bóp phanh (5)
    STATE_WAIT,       // Chờ điều kiện an toàn (6)
    STATE_FAULT,      // Báo lỗi (7)
    STATE_UNKNOWN
};

// Cờ báo lỗi Bitmask từ IC Votol (32-bit Fault Code)
namespace VotolFault {
    constexpr uint32_t EBRAKE_ON           = 0x00000001; // Phanh điện kích hoạt
    constexpr uint32_t OVER_CURRENT_HW     = 0x00000002; // Quá dòng phần cứng
    constexpr uint32_t UNDER_VOLTAGE       = 0x00000004; // Tụt áp (Bảo vệ pin thấp)
    constexpr uint32_t THROTTLE_HALL_ERR   = 0x00000008; // Lỗi tay ga / cảm biến Hall tay ga
    constexpr uint32_t OVER_VOLTAGE        = 0x00000010; // Quá áp pin
    constexpr uint32_t MCU_ERROR           = 0x00000020; // Lỗi chip điều khiển
    constexpr uint32_t MOTOR_BLOCK         = 0x00000040; // Động cơ bị bó / kẹt
    constexpr uint32_t FOOTPLATE_ERR       = 0x00000080; // Lỗi cảm biến chân ga
    constexpr uint32_t SPEED_RUNAWAY       = 0x00000100; // Mất kiểm soát tốc độ
    constexpr uint32_t EEPROM_WRITING      = 0x00000200; // Đang lưu dữ liệu vào EEPROM
    constexpr uint32_t STARTUP_FAILURE     = 0x00000800; // Lỗi tự kiểm tra khi bật nguồn
    constexpr uint32_t CONTROLLER_OVERHEAT = 0x00001000; // Quá nhiệt IC điều khiển
    constexpr uint32_t OVER_CURRENT_SW     = 0x00002000; // Quá dòng phần mềm
    constexpr uint32_t THROTTLE_PEDAL_ERR  = 0x00004000; // Lỗi bàn đạp tăng tốc
    constexpr uint32_t CURRENT_SENSOR1_ERR = 0x00008000; // Lỗi cảm biến dòng điện 1
    constexpr uint32_t CURRENT_SENSOR2_ERR = 0x00010000; // Lỗi cảm biến dòng điện 2
    constexpr uint32_t BRAKE_FAILURE       = 0x00020000; // Hỏng hóc cảm biến phanh
    constexpr uint32_t MOTOR_HALL_ERR      = 0x00040000; // Lỗi mắt đọc Hall động cơ
    constexpr uint32_t MOSFET_DRIVER_ERR   = 0x00080000; // Lỗi IC kích cổng MOSFET
    constexpr uint32_t MOSFET_HIGH_SHORT   = 0x00100000; // Chập MOSFET vế cao
    constexpr uint32_t PHASE_WIRE_OPEN     = 0x00200000; // Hở dây pha động cơ
    constexpr uint32_t PHASE_WIRE_SHORT    = 0x00400000; // Ngắn mạch dây pha
    constexpr uint32_t MCU_CHIP_ERROR      = 0x00800000; // Lỗi phần cứng MCU
    constexpr uint32_t PRECHARGE_ERROR     = 0x01000000; // Lỗi nạp trước Precharge
    constexpr uint32_t MOTOR_OVERHEAT      = 0x08000000; // Động cơ quá nhiệt
    constexpr uint32_t SOC_ZERO_ERROR      = 0x80000000; // Dung lượng pin về 0%
}

// Struct lưu toàn bộ thông số đọc từ IC Votol
struct VotolData {
    // Thông số động (Real-time Telemetry)
    float voltage;          // Điện áp pin (Volts, VD: 72.4V)
    float current;          // Dòng xả / nạp (Amps, VD: 12.5A)
    int16_t rpm;            // Tốc độ vòng tua động cơ (Vòng/Phút)
    float speedKmh;         // Tốc độ di chuyển (km/h)
    int8_t controllerTemp;  // Nhiệt độ IC điều khiển (°C)
    int8_t motorTemp;       // Nhiệt độ động cơ (°C)
    VotolGear gear;         // Cấp số xe (P, Eco, D, Sport, R)
    VotolState state;       // Trạng thái hệ thống (Run, Brake, Fault...)
    uint32_t faultCode;     // Mã lỗi dạng bitmask 32-bit
    
    // Trạng thái kết nối
    bool isConnected;       // true nếu nhận được dữ liệu hợp lệ trong thời gian timeout
    uint32_t lastReceivedMs;// Timestamp millis() lần nhận gói tin hợp lệ gần nhất
    uint32_t validPacketsCount;   // Số gói tin nhận đúng checksum
    uint32_t checksumErrorsCount; // Số gói tin lỗi checksum do nhiễu

    // Thông tin cấu hình từ APP-Votol (nếu truy vấn)
    uint16_t softwareVersion;
    uint16_t hardwareVersion;
    uint8_t model;
    uint8_t brand;
    uint16_t maxRpmLimit;
    uint16_t busCurrentLimit;

    // Chẩn đoán tín hiệu UART trực tiếp (Live Diagnostics)
    uint32_t totalBytesReceived = 0; // Tổng số byte nhận được từ Votol
    uint32_t totalBytesSent = 0;     // Tổng số byte gửi vào Votol
    uint32_t bytesPerSecond = 0;     // Tốc độ nhận byte/giây
    uint32_t currentBaudRate = 9600; // Baudrate hiện tại
    char lastRawHex[74] = {0};       // Mẫu byte nhận được gần nhất
    char lastFrameHex[74] = {0};     // Chuỗi Hex đầy đủ 24 byte của frame hợp lệ mới nhất
    bool pinsSwapped = false;        // true nếu đang đảo chân RX/TX (RX 17, TX 16)
    bool autoProbeLocked = false;    // true nếu đã bắt được tín hiệu và khóa cấu hình
    bool sideStand = false;          // true nếu đang gạt chân chống (Bit 6 - 0x40)
    bool brake = false;              // true nếu đang bóp phanh (Bit 4 - 0x10)
    bool regen = false;              // true nếu đang phanh tái sinh (Bit 7 - 0x80)
    bool parked = false;             // true nếu đang ở số P (Bit 3 - 0x08)
    bool reverse = false;            // true nếu đang ở số lùi R (Bit 2 - 0x04)
    bool locked = false;             // true nếu kích hoạt chống trộm (Bit 5 - 0x20)

    // Helper functions
    const char* getGearString() const;
    const char* getStateString() const;
    String getFaultString() const;
};

/**
 * ============================================================================
 * CLASS XỬ LÝ GIAO THỨC UART VOTOL
 * ============================================================================
 */
class VotolProtocolHandler {
public:
    using DataCallback = std::function<void(const VotolData&)>;

    VotolProtocolHandler();
    ~VotolProtocolHandler();

    // Khởi tạo UART cho Votol
    void begin(HardwareSerial &serialPort, 
               int8_t rxPin = PIN_VOTOL_RX, 
               int8_t txPin = PIN_VOTOL_TX, 
               uint32_t baudRate = VOTOL_BAUD_RATE);

    // Thay đổi cấu hình UART
    bool applyUartConfig(int8_t rxPin, int8_t txPin, uint32_t baudRate);

    // Thay đổi Baudrate trực tiếp
    void setBaudRate(uint32_t baud);

    // Đảo chéo chân RX/TX bằng phần mềm
    void swapPins();

    // Bật / tắt chế độ tự động dò cấu hình (Auto Probe)
    void setAutoProbe(bool enabled) { _autoProbeEnabled = enabled; }
    bool isAutoProbeEnabled() const { return _autoProbeEnabled; }

    // Gửi lệnh Handshake LDGET đánh thức Votol
    void sendHandshake();

    // Gửi lệnh SHOW để kích hoạt Votol trả dữ liệu
    void sendShowCommand() { sendTelemetryPoll(); }

    // Hàm gọi liên tục trong FreeRTOS Task trên Core 0 (hoàn toàn non-blocking)
    void update();

    // Lấy bản sao dữ liệu an toàn đa luồng (Thread-safe Snapshot cho Core 1)
    bool getSnapshot(VotolData &outData, TickType_t waitTicks = pdMS_TO_TICKS(10));

    // Cài đặt đường kính bánh xe và tỉ số truyền để tính km/h
    void setWheelParams(float wheelDiameterM, float gearRatio = DEFAULT_GEAR_RATIO);

    // Gửi gói tin truy vấn dữ liệu Telemetry xoay vòng 3 gói
    void sendTelemetryPoll();

    // Gửi gói tin đọc tham số cài đặt APP-Votol (Read Request 0x3A)
    void sendReadConfigPoll();

    // Đăng ký callback khi có dữ liệu mới
    void onData(DataCallback callback) { _callback = callback; }

    // Tính Checksum XOR
    static uint8_t calcChecksum(const uint8_t* buffer, size_t length);

private:
    HardwareSerial* _serial;
    int8_t _rxPin;
    int8_t _txPin;
    uint32_t _baudRate;
    VotolData _data;
    DataCallback _callback;

    // Mutex bảo vệ struct _data khi trao đổi giữa Core 0 và Core 1
    SemaphoreHandle_t _mutex;

    // Bộ đệm nhận UART
    uint8_t _uartFrameBuffer[24];
    size_t _uartFrameLength;
    uint8_t _uartRxSample[16];
    size_t _uartRxSampleLength;
    uint32_t _uartRxIntervalBytes;
    uint32_t _uartRxTotalBytes;
    uint32_t _uartRxLastByteTime;
    uint32_t _uartValidFrameCount;
    uint32_t _uartLastValidFrameTime;
    char _uartLastFrameHex[74];

    // Quản lý chu kỳ gửi lệnh truy vấn xoay vòng
    bool _uartPollingEnabled;
    uint8_t _uartPollStep;
    uint32_t _uartNextPollStepAt;
    uint32_t _uartTxRequestCount;

    // Thông số tính tốc độ
    float _wheelDiameterM;
    float _gearRatio;

    // Báo cáo chẩn đoán định kỳ (1 giây / lần)
    uint32_t _lastReportMs;

    // Tự động dò cấu hình (Auto Probe 4 chế độ khi chưa có tín hiệu)
    bool _autoProbeEnabled;
    uint8_t _probeIndex;
    uint32_t _lastProbeSwitchMs;

    // Hàm nội bộ xử lý
    void processVotolUartData();
    bool parseVotolTelemetryFrame(const uint8_t* frame, size_t length);
    bool parseAppVotolFrame(const uint8_t* frame, size_t len);
    void reportUartRx();
    void checkConnectionTimeout();
    void checkAutoProbe();
};
