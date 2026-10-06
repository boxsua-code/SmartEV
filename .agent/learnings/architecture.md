# Architecture Decisions

---

### FreeRTOS Dual-Core & Mutex Snapshot cho Đồng hồ Xe điện
- **Ngày**: 2026-10-05
- **Task**: Task 2.1 - Module Đọc dữ liệu IC Votol (UART)
- **Chi tiết**:
  Phân tách kiến trúc 2 nhân ESP32-S3: Core 0 đảm nhận Task đọc UART (Votol, BMS ANT) với độ ưu tiên cao (Priority 5) và chu kỳ nghỉ nhỏ `vTaskDelay(pdMS_TO_TICKS(10))`. Core 1 xử lý giao diện OLED (Render 20-30 FPS) và quét nút bấm. Dữ liệu chia sẻ qua struct `VotolData` được bảo vệ bằng FreeRTOS Mutex (`xSemaphoreCreateMutex`). Core 1 gọi hàm `getSnapshot()` với timeout ngắn để sao chép dữ liệu mà không làm đơ khung hình giao diện.
- **Files liên quan**: `src/main.cpp`, `include/votol_protocol.h`, `src/votol_protocol.cpp`

---

### Quản lý Đa Cảm biến UART trên Cùng 1 Task FreeRTOS Core 0
- **Ngày**: 2026-10-05
- **Task**: Task 2.2 - Module Đọc dữ liệu BMS ANT (UART)
- **Chi tiết**:
  Thay vì tạo nhiều task riêng lẻ cho từng cổng UART (tốn stack RAM và context switch), gộp tất cả việc quét buffer UART (Votol trên UART1 và BMS ANT trên UART2) vào chung một FreeRTOS task `taskSensorReader` trên Core 0. Mỗi handler (`VotolProtocolHandler`, `AntBmsProtocolHandler`) sở hữu Mutex độc lập để bảo vệ struct dữ liệu tương ứng. Core 1 có thể đọc snapshot của Votol và BMS bất kỳ lúc nào mà không bị block lẫn nhau.
- **Files liên quan**: `src/main.cpp`, `include/ant_bms_protocol.h`, `src/ant_bms_protocol.cpp`

---

### Quản lý Render Giao diện OLED trên Core 1 Giới hạn FPS
- **Ngày**: 2026-10-05
- **Task**: Task 2.3 - Module Thời gian thực (RTC DS3231) & Display (OLED)
- **Chi tiết**:
  Việc gửi toàn bộ buffer 1024 bytes (128x64) hoặc 512 bytes (128x32) qua bus I2C tốn khoảng 3-10ms. Ghim việc vẽ màn hình hoàn toàn trên Core 1 trong `loop()`, đồng thời áp dụng Rate Limiter `OLED_REFRESH_INTERVAL_MS = 60ms` (~16-20 FPS). Điều này đảm bảo Core 0 hoàn toàn rảnh tay để nhận từng byte UART tốc độ cao từ IC Votol và BMS mà không lo bị drop byte hay jitter.
- **Files liên quan**: `src/main.cpp`, `src/display_rtc_manager.cpp`

---

### Phân Tách FSM Chuyển Trang & Quét Phím Bấm Cơ Học Trên Core 1
- **Ngày**: 2026-10-05
- **Task**: Task 2.4 - Luồng Giao diện & Chuyển Trang (State Machine & Button Controller)
- **Chi tiết**:
  Các thao tác người dùng (nhấn nút SET, UP, DOWN) và máy trạng thái hiển thị (FSM 3 trang: Main Dashboard, BMS Detail, Settings Menu) được xử lý hoàn toàn trong chu kỳ `loop()` trên Core 1.
  - Phím bấm được debounce bằng phần mềm (Software Debounce 40ms) không dùng ngắt (interrupt) để loại trừ hiện tượng kích hoạt giả (false trigger) do xung cảm ứng cao áp từ hệ thống động cơ xe điện.
  - Sự kiện phím được chuyển giao ngay cho `displayRtc.handleButtons()`, cho phép người dùng thay đổi trang hoặc điều chỉnh thông số xe (đường kính lốp, giới hạn dòng, giờ RTC) tức thì mà không can thiệp hay cản trở chu kỳ đọc UART cảm biến tốc độ cao trên Core 0.
- **Files liên quan**: `src/main.cpp`, `include/button_controller.h`, `src/button_controller.cpp`, `include/display_rtc_manager.h`, `src/display_rtc_manager.cpp`

---

### Tích Hợp Bluetooth BLE Vào Luồng Core 0 và Đồng Bộ Snapshot 3 Chiều
- **Ngày**: 2026-10-05
- **Task**: Task 2.5 - Tích hợp Android Auto rút gọn / Bluetooth BLE
- **Chi tiết**:
  Tuân thủ triệt để quy tắc `project-rules.md.txt`:
  - **Core 0**: Chịu trách nhiệm toàn bộ các giao tiếp ngoại vi không đồng bộ và mạng không dây bao gồm: UART1 (Votol EM150), UART2 (BMS ANT) và BLE GATT Server (Nordic UART Service). Cứ mỗi 1000ms, Core 0 tự động trích xuất snapshot Votol & BMS để gửi telemetry dạng JSON về điện thoại.
  - **Core 1**: Đảm nhiệm chu kỳ vẽ màn hình OLED SSD1306 (16-20 FPS) và quét 3 phím bấm. Trong mỗi frame render, Core 1 lấy snapshot an toàn từ 3 Mutex độc lập (`VotolData`, `ANTBMSData`, `BleNavData`). Nếu có cuộc gọi đến, Core 1 tự động hiển thị Call Banner Popup trong 5 giây mà không làm gián đoạn chu kỳ đọc sensor hay kết nối BLE trên Core 0.
- **Files liên quan**: `src/main.cpp`, `include/ble_nav_manager.h`, `src/ble_nav_manager.cpp`, `src/display_rtc_manager.cpp`

---

### Kiến Trúc FSM 5 Trang & Tách Biệt Tầng Tín Hiệu Xe Điện (Signals Layer)
- **Ngày**: 2026-10-05
- **Nâng cấp**: Tín hiệu Đèn/Xi nhan & Cài đặt chuyên sâu
- **Chi tiết**:
  - **Tách biệt Tín hiệu Xe (VehicleSignals)**: Các chân GPIO đọc tín hiệu công tắc đèn pha và xi nhan (GPIO 1, 2, 7) được bọc trong module `VehicleSignals` với mức logic cách ly bảo vệ. Xung chớp nháy được tính toán theo đồng hồ hệ thống `millis()`, đảm bảo tần số nhấp nháy 2.5Hz (400ms ON / 400ms OFF) chuẩn xác bất kể tải CPU.
  - **Mở rộng FSM 5 Trang Hoàn Chỉnh**:
    - **Trang 1: Main Dashboard** (Vận tốc font to, Cấp số, Giờ, % Pin, Trip, Xi nhan trái/phải nhấp nháy, Đèn pha).
    - **Trang 2: BMS Detail** (32 cell pin, min/max, delta cell mV, nhiệt độ MOSFET/pin).
    - **Trang 3: Quick Settings** (Đường kính bánh xe, giới hạn dòng, giờ phút RTC, reset Trip).
    - **Trang 4: Advanced Settings** (Cắt áp pin yếu, cảnh báo quá nhiệt, cảnh báo lệch cell, cặp cực motor, độ sáng OLED, cảnh báo quá tốc độ, đổi đơn vị km/h - mph, khôi phục cài đặt gốc).
    - **Trang 5: Navigation & Media HUD** (Android Auto / Google Maps turn-by-turn & bài hát Bluetooth).
  - **Lưu trữ NVS Non-blocking**: Dữ liệu cài đặt được nạp một lần khi khởi động và chỉ ghi vào Flash NVS khi người dùng bấm Lưu (SET click) hoặc thoát Edit Mode, tránh hao mòn Flash EEPROM.
- **Files liên quan**: `include/vehicle_signals.h`, `src/vehicle_signals.cpp`, `include/display_rtc_manager.h`, `src/display_rtc_manager.cpp`, `src/main.cpp`



