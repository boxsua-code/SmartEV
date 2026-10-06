#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"
#include "votol_protocol.h"
#include "ant_bms_protocol.h"
#include "button_controller.h"
#include "ble_nav_manager.h"
#include "vehicle_signals.h"
#include "display_rtc_manager.h"
#include "can_bus_handler.h"

// Đối tượng quản lý giao tiếp cảm biến, phím bấm, tín hiệu xe, CAN & hiển thị
VotolProtocolHandler votol;
AntBmsProtocolHandler bms;
CanBusHandler canBus;
DisplayRtcManager displayRtc;
ButtonController buttons(PIN_BTN_WAKE, -1, -1);
BleNavManager bleNav;
VehicleSignals signals(PIN_SIGNAL_LEFT, PIN_SIGNAL_RIGHT, PIN_SIGNAL_HEADLIGHT, SIGNAL_INPUT_ACTIVE_LOW);

// Task Handle cho FreeRTOS trên Core 0
TaskHandle_t taskSensorHandle = nullptr;

// Định thời in log debug ra Serial Monitor & tính quãng đường Trip
unsigned long lastLogTime = 0;
const unsigned long LOG_INTERVAL_MS = 1000;
unsigned long lastTripCalcMs = 0;
static unsigned long lastWakeBtnMs = 0;

/**
 * ============================================================================
 * FREERTOS TASK: ĐỌC DỮ LIỆU CẢM BIẾN (VOTOL + BMS) & BLE TRÊN CORE 0
 * ============================================================================
 */
void taskSensorReader(void *pvParameters) {
    Serial.printf("[FreeRTOS Core %d] Task đọc cảm biến (Votol & BMS) & BLE đang chạy...\n", xPortGetCoreID());

    while (true) {
        // 1. Quét buffer UART không chặn cho cả 2 thiết bị
        votol.update();
        bms.update();

        // 2. Lấy dữ liệu và quét thêm CAN Bus TWAI (chuẩn JAMFOXRS)
        VotolData vd;
        ANTBMSData bd;
        votol.getSnapshot(vd);
        bms.getSnapshot(bd);
        canBus.update(vd, bd); // Cập nhật từ CAN Bus nếu xe cắm qua mạng CAN

        // 3. Gửi telemetry xe định kỳ qua Bluetooth BLE về điện thoại
        bleNav.update(vd, bd, signals.getState());

        // Nhường CPU cho FreeRTOS scheduler
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * Gói tin giả lập Votol Telemetry (24 bytes) để test khi chưa cắm xe
 */
void sendMockVotolFrame() {
    Serial.println("\n[MOCK TEST] ---> Gửi gói tin giả lập Votol Telemetry (72V, 20A, 550 RPM, 38°C IC, SPORT)...");
    uint8_t mockFrame[24] = {
        0xC0, 0x14, 0x00, 0x00, 0x00,
        0x02, 0xD0, // 72.0V
        0x00, 0xC8, // 20.0A
        0x00,
        0x00, 0x00, 0x00, 0x00, // Fault code: 0 (No fault)
        0x02, 0x26, // 550 RPM (~45 km/h)
        0x58,       // IC Temp = 38°C (88 - 50)
        0x5F,       // Motor Temp = 45°C (95 - 50)
        0x00, 0x00,
        0x03,       // Gear: 0x03 = SPORT
        0x02,       // State: 0x02 = RUN
        0x00,       // Checksum placeholder
        0x0D
    };
    mockFrame[22] = VotolProtocolHandler::calcChecksum(mockFrame, 22);
    Serial1.write(mockFrame, sizeof(mockFrame));
}

/**
 * Gói tin giả lập ANT BMS Frame (140 bytes)
 */
void sendMockAntBmsFrame() {
    Serial.println("\n[MOCK TEST] ---> Gửi gói tin giả lập ANT BMS (72V, 15A, 20S Cell, 85% SoC)...");
    uint8_t mockFrame[140] = {0};
    mockFrame[0] = 0xAA; mockFrame[1] = 0x55; mockFrame[2] = 0xAA; mockFrame[3] = 0xFF;

    for (int i = 0; i < 20; i++) {
        uint16_t cellMv = 3600 + (i % 5) * 2;
        mockFrame[4 + (i * 2)] = (cellMv >> 8) & 0xFF;
        mockFrame[4 + (i * 2) + 1] = cellMv & 0xFF;
    }
    mockFrame[70] = 0x00; mockFrame[71] = 0x00; mockFrame[72] = 0x00; mockFrame[73] = 150; // 15.0A
    mockFrame[74] = 85; // 85% SoC
    mockFrame[93] = 0x00; mockFrame[94] = 32; // Temp1 = 32°C
    mockFrame[95] = 0x00; mockFrame[96] = 29; // Temp2 = 29°C

    mockFrame[115] = 5; mockFrame[116] = 0x0E; mockFrame[117] = 0x18; // Cell 5 max 3608mV
    mockFrame[118] = 1; mockFrame[119] = 0x0E; mockFrame[120] = 0x10; // Cell 1 min 3600mV
    mockFrame[123] = 20; // 20S

    uint16_t cs = AntBmsProtocolHandler::calcChecksum(mockFrame, 140);
    mockFrame[138] = (cs >> 8) & 0xFF;
    mockFrame[139] = cs & 0xFF;
    Serial2.write(mockFrame, sizeof(mockFrame));
}

void setup() {
    // Tắt đèn LED RGB WS2812 trắng chói mắt trên bo ESP32-S3 (GPIO 48 / GPIO 38)
#ifdef RGB_BUILTIN
    neopixelWrite(RGB_BUILTIN, 0, 0, 0);
#endif
    neopixelWrite(48, 0, 0, 0);
    neopixelWrite(38, 0, 0, 0);

    // 1. Khởi tạo Serial Debug
    Serial.begin(DEBUG_BAUD_RATE);
    delay(1000);

    Serial.println("==========================================================");
    Serial.println("   ESP32-S3 SMART DASHBOARD - PHIÊN BẢN TINH GỌN         ");
    Serial.println("   OLED: Welcome -> Chờ BLE -> Hiện Đồng hồ RTC          ");
    Serial.println("   1 Nút Bấm WAKE duy nhất (GPIO 4 kéo xuống GND)        ");
    Serial.println("   UART Votol (115200/9600) | BMS ANT (19200)             ");
    Serial.println("==========================================================");

    // 2. Khởi tạo nút bấm Wakeup BLE (GPIO 4)
    pinMode(PIN_BTN_WAKE, INPUT_PULLUP);

    // 3. Khởi tạo tín hiệu Xi nhan Trái/Phải và Đèn pha
    signals.begin();

    // 4. Khởi tạo I2C: Màn hình OLED SSD1306 và Module RTC DS3231
    displayRtc.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ);

    // 5. Khởi tạo Bluetooth Low Energy (BLE Server Nordic UART)
    bleNav.begin(BLE_DEVICE_NAME);

    // Tự động đồng bộ giờ điện thoại vào chip RTC DS3231
    bleNav.onTimeSync([](uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t min, uint8_t s) {
        displayRtc.setDateTime(DateTime(y, m, d, h, min, s));
        Serial.printf("[MAIN RTC] ✅ Đã nạp giờ chuẩn từ điện thoại vào DS3231: %02d:%02d:%02d %02d/%02d/%04d\n",
                      h, min, s, d, m, y);
    });

    // Nhận lệnh điều khiển UART Votol từ App điện thoại
    bleNav.onPollVotol([]() {
        Serial.println("[MAIN] App yêu cầu: BẮN GÓI TIN THĂM DÒ VOTOL SHOW!");
        votol.sendShowCommand();
    });
    bleNav.onSetBaud([](uint32_t baud) {
        Serial.printf("[MAIN] App yêu cầu: Đổi UART1 sang %d baud!\n", baud);
        votol.setBaudRate(baud);
    });
    bleNav.onSwapUart([]() {
        Serial.println("[MAIN] App yêu cầu: Đảo chéo chân RX ⮂ TX phần mềm!");
        votol.swapPins();
    });

    // 6. Khởi tạo UART1 cho IC Votol (9600 baud mặc định, an toàn Passive Mode)
    // Lưu ý cú pháp ESP32 Arduino Core: serial.begin(baud, SERIAL_8N1, rxPin, txPin);
    Serial.printf("[INIT] UART1 (Votol) @ %d baud (RX: GPIO %d, TX: GPIO %d) [PASSIVE LISTEN]\n",
                  VOTOL_BAUD_RATE, PIN_VOTOL_RX, PIN_VOTOL_TX);
    votol.begin(Serial1, PIN_VOTOL_RX, PIN_VOTOL_TX, VOTOL_BAUD_RATE);
    votol.setWheelParams(displayRtc.getConfig().wheelDiameterM, displayRtc.getConfig().gearRatio);

    // 7. Khởi tạo UART2 cho BMS ANT (19200 baud, GPIO 16 RX2, GPIO 15 TX2)
    Serial.printf("[INIT] UART2 (BMS ANT) @ %d baud (RX: GPIO %d, TX: GPIO %d)\n",
                  BMS_BAUD_RATE, PIN_BMS_RX, PIN_BMS_TX);
    bms.begin(Serial2, PIN_BMS_RX, PIN_BMS_TX, BMS_BAUD_RATE);

    // 7.5. Khởi tạo CAN Bus TWAI (250kbps, Listen-Only, chuẩn JAMFOXRS)
    canBus.begin(PIN_CAN_TX, PIN_CAN_RX, CAN_BAUD_RATE);

    // 8. Tạo Task FreeRTOS ghim vào Core 0 (Ưu tiên 5) đọc UART cảm biến & BLE Telemetry
    xTaskCreatePinnedToCore(
        taskSensorReader,
        "TaskSensorReader",
        6144,
        nullptr,
        5,
        &taskSensorHandle,
        0
    );

    Serial.println("[INIT] Hệ thống tinh gọn đã sẵn sàng!");
    Serial.println("[INIT] Nút bấm: Chỉ 1 nút WAKE tại GPIO 4 (Đánh thức lại BLE)");
    Serial.println("[INIT] OLED: Welcome -> Chờ kết nối -> Hiện Đồng hồ RTC");
    Serial.println("[INIT] Đổi baud Votol qua Serial: Gõ '1'=115200 baud, '9'=9600 baud");
}

void loop() {
    // 1. Quét nút bấm WAKE duy nhất (GPIO 4 kéo xuống GND)
    if (digitalRead(PIN_BTN_WAKE) == LOW) {
        if (millis() - lastWakeBtnMs > 300) { // Debounce 300ms
            lastWakeBtnMs = millis();
            Serial.println("[WAKE] Nút bấm được nhấn -> Đánh thức lại kết nối BLE!");
            bleNav.startAdvertising();
        }
    }

    // 2. Cập nhật tín hiệu đèn pha và xi nhan
    signals.update();

    // 3. Xử lý lệnh test và đổi baudrate từ Serial Monitor
    if (Serial.available()) {
        char c = Serial.read();
        if (c == '1') {
            Serial.println("[BAUD] Chuyển UART1 Votol sang 115200 baud...");
            votol.begin(Serial1, PIN_VOTOL_RX, PIN_VOTOL_TX, 115200);
        } else if (c == '9') {
            Serial.println("[BAUD] Chuyển UART1 Votol sang 9600 baud...");
            votol.begin(Serial1, PIN_VOTOL_RX, PIN_VOTOL_TX, 9600);
        } else if (c == 'w' || c == 'W') {
            Serial.println("[BLE] Đánh thức phát sóng Bluetooth BLE...");
            bleNav.startAdvertising();
        } else if (c == 't' || c == 'T') {
            sendMockVotolFrame();
        } else if (c == 'b' || c == 'B') {
            sendMockAntBmsFrame();
        } else if (c == 'p' || c == 'P') {
            Serial.println("[POLL] Gửi truy vấn Votol SHOW & BMS...");
            votol.sendTelemetryPoll();
            bms.sendPollQuery();
        } else if (c == 'r' || c == 'R') {
            Serial.printf("[RTC] Thời gian hiện tại: %s %s (Nhiệt độ RTC: %.1f°C)\n",
                          displayRtc.getFormattedTime(true).c_str(),
                          displayRtc.getFormattedDate().c_str(),
                          displayRtc.getRtcTemperature());
        } else if (c == '[') {
            signals.toggleTurnLeft();
        } else if (c == ']') {
            signals.toggleTurnRight();
        } else if (c == 'l' || c == 'L') {
            signals.toggleHeadlight();
        } else if (c == 'h' || c == 'H') {
            signals.toggleHazard();
        }
    }

    // 4. Lấy snapshot dữ liệu an toàn đa luồng (Mutex protected)
    VotolData vd;
    ANTBMSData bd;
    BleNavData nd;
    votol.getSnapshot(vd);
    bms.getSnapshot(bd);
    bleNav.getSnapshot(nd);

    // 5. Cập nhật và vẽ lên màn hình OLED trên Core 1
    displayRtc.update(vd, bd, nd, signals);

    // 6. In thông số ra Serial Monitor định kỳ
    unsigned long now = millis();
    if (now - lastLogTime >= LOG_INTERVAL_MS) {
        lastLogTime = now;

        Serial.printf("[STATUS] BLE: %s | RTC: %s | Votol: %s (%.1f km/h) | BMS: %s (%d%%)\n",
                      nd.isConnected ? "CONNECTED" : "WAITING",
                      displayRtc.getFormattedTime(true).c_str(),
                      vd.isConnected ? "CONNECTED" : "WAITING",
                      vd.speedKmh,
                      bd.isConnected ? "CONNECTED" : "WAITING",
                      bd.soc);
    }
}
