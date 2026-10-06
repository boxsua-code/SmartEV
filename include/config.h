#pragma once
#include <Arduino.h>

/**
 * ============================================================================
 * CẤU HÌNH PHẦN CỨNG & THÔNG SỐ XE (ESP32-S3)
 * Tinh gọn: Màn hình OLED Welcome + Đồng hồ RTC khi đã kết nối
 * Chỉ 1 nút Wakeup BLE (GPIO 4)
 * Votol Passive Mode an toàn chống báo lỗi xe
 * ============================================================================
 */

// --- 1. GIAO TIẾP I2C (Màn hình OLED SSD1306 + RTC DS3231) ---
#define PIN_I2C_SCL               8   // GPIO 8 SCL
#define PIN_I2C_SDA               9   // GPIO 9 SDA
#define I2C_FREQ                  400000 // 400kHz Fast-mode I2C

// Cấu hình màn hình OLED SSD1306 (0.91 inch: 128x32, 0.96 inch: 128x64)
#define OLED_SCREEN_WIDTH         128
#define OLED_SCREEN_HEIGHT        32
#define OLED_I2C_ADDRESS          0x3C
#define OLED_REFRESH_INTERVAL_MS  100

// --- 2. UART 1: IC ĐIỀU KHIỂN VOTOL ---
// Lưu ý cú pháp Serial: rxPin nối vào TX Votol, txPin nối vào RX Votol
#define PIN_VOTOL_RX              18  // ESP32 RX1 (nhận) -> Votol TX
#define PIN_VOTOL_TX              17  // ESP32 TX1 (truyền) -> Votol RX
#define VOTOL_BAUD_RATE           9600   // Baudrate chính: 9600 baud
#define VOTOL_BAUD_RATE_ALT       115200 // Baudrate phụ: 115200 baud
#define VOTOL_PASSIVE_MODE        true   // TRUE: Chỉ lắng nghe thụ động, không gửi byte spam tránh lỗi IC Votol!
#define VOTOL_POLL_INTERVAL_MS    1000
#define VOTOL_TIMEOUT_MS          3000

// --- 3. UART 2: BMS ANT (ĐỂ CHÂN CHỜ, HỖ TRỢ CẢ BLE) ---
#define PIN_BMS_RX                16  // ESP32 RX2 (nhận) -> Chân chờ BMS TX
#define PIN_BMS_TX                15  // ESP32 TX2 (truyền) -> Chân chờ BMS RX
#define BMS_BAUD_RATE             19200
#define BMS_UART_PASSIVE_WAIT     true // TRUE: Để chân chờ thụ động, không spam xung khi ANT BMS dùng BLE
#define BMS_POLL_INTERVAL_MS      1000
#define BMS_TIMEOUT_MS            3000

// --- 3.5. CAN BUS TWAI (CHUẨN XE ĐIỆN EV / JAMFOXRS) ---
#define PIN_CAN_TX                5   // GPIO 5: TWAI TX -> CAN Transceiver TX
#define PIN_CAN_RX                6   // GPIO 6: TWAI RX -> CAN Transceiver RX
#define CAN_BAUD_RATE             250000 // 250 kbps chuẩn EV Polytron / Votol

// --- 4. NÚT BẤM (CHỈ 1 NÚT WAKEUP DUY NHẤT TẠI GPIO 4) ---
#define PIN_BTN_WAKE              4   // GPIO 4: Nhấn để đánh thức / kích hoạt lại BLE
#define PIN_BTN_SET               4
#define PIN_BTN_UP                -1  // Không dùng
#define PIN_BTN_DOWN              -1  // Không dùng

// --- 5. THÔNG SỐ BÁNH XE (TÍNH TỐC ĐỘ KM/H CHÍNH XÁC) ---
#define DEFAULT_WHEEL_DIAMETER_M  0.4728f
#define DEFAULT_GEAR_RATIO        1.0f

// --- 6. SERIAL DEBUG CONSOLE ---
#define DEBUG_BAUD_RATE           115200

// --- 7. BLUETOOTH BLE (ANDROID AUTO / PHONE COMPANION) ---
#define BLE_DEVICE_NAME           "ESP32-SmartDash"
#define BLE_SERVICE_UUID          "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHARACTERISTIC_RX     "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHARACTERISTIC_TX     "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_TELEMETRY_INTERVAL_MS 1000

// --- 8. TÍN HIỆU ĐÈN VÀ XI NHAN XE ĐIỆN (INPUT_PULLUP) ---
#define PIN_SIGNAL_LEFT           1   // GPIO 1: Xi nhan trái
#define PIN_SIGNAL_RIGHT          2   // GPIO 2: Xi nhan phải
#define PIN_SIGNAL_HEADLIGHT      7   // GPIO 7: Đèn pha (High Beam)
#define SIGNAL_INPUT_ACTIVE_LOW   true
#define TURN_SIGNAL_BLINK_MS      400

// --- 9. THÔNG SỐ CÀI ĐẶT CHUYÊN SÂU MẶC ĐỊNH ---
#define DEFAULT_LOW_VOLTAGE_CUTOFF 62.0f
#define DEFAULT_OVERHEAT_LIMIT_C   80
#define DEFAULT_DELTA_CELL_MV      30
#define DEFAULT_MOTOR_POLE_PAIRS   15
#define DEFAULT_SPEED_WARN_KMH     70
#define DEFAULT_OLED_CONTRAST      255
