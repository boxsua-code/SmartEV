# KẾ HOẠCH TỔNG THỂ DỰ ÁN ĐỒNG HỒ XE ĐIỆN ESP32-S3

> **Tiến độ tổng thể**: **100% (Hoàn thành toàn bộ 5/5 Task phần mềm cốt lõi)**  
> **Trạng thái**: Đã hoàn thành toàn bộ mã nguồn firmware PlatformIO & Sẵn sàng nạp lên phần cứng thật  
> **Cập nhật ngày**: 2026-10-05  

---

## Giai đoạn 1: Chuẩn bị Phần cứng & Sơ đồ đấu nối (Hardware Setup)

### 1. Danh sách linh kiện chính
- **Vi điều khiển**: ESP32-S3 (8MB Flash / 8MB PSRAM OPI).
- **Màn hình**: OLED 0.91 inch / 0.96 inch (Giao tiếp I2C - SSD1306) để hiện giờ & trạng thái.
- **Thời gian thực**: Module RTC DS3231 (I2C) giữ giờ chính xác khi mất điện.
- **Động cơ & Pin**: IC Votol EM150sp (UART1), BMS ANT (UART2).
- **Hạ áp & Bảo vệ**: Mạch hạ áp Buck DC-DC (Hỗ trợ áp từ 48V–96V xuống 5V/3.3V cấp cho ESP32).

### 2. Phân bổ Chân (Pinout Assignment) trên ESP32-S3
- **I2C Bus** (Màn hình OLED + DS3231 RTC dùng chung bus):
  - `SCL`: GPIO 8
  - `SDA`: GPIO 9
- **UART 1** (Kết nối IC Votol):
  - `TX1`: GPIO 17 → RX Votol (Mức logic 3.3V)
  - `RX1`: GPIO 18 → TX Votol (Baudrate: 9600)
- **UART 2** (Kết nối BMS ANT):
  - `TX2`: GPIO 15 → RX BMS ANT
  - `RX2`: GPIO 16 → TX BMS ANT (Baudrate: 19200)
- **Nút bấm điều khiển (Button Control - INPUT_PULLUP)**:
  - `BTN_SET` (Cài đặt/Chuyển trang): GPIO 4
  - `BTN_UP` (Lên/Tăng): GPIO 5
  - `BTN_DOWN` (Xuống/Giảm): GPIO 6
- **Tín hiệu Đèn và Xi nhan xe điện (Input Optocoupler / Pullup)**:
  - `SIGNAL_LEFT` (Xi nhan Trái): GPIO 1
  - `SIGNAL_RIGHT` (Xi nhan Phải): GPIO 2
  - `SIGNAL_HEADLIGHT` (Đèn Pha / High Beam): GPIO 7

---

## Giai đoạn 2: Phát triển các Module Phần mềm (Software Modules)

- [x] **Task 2.1: Module Đọc dữ liệu IC Votol (UART)** - *(HOÀN THÀNH - 2026-10-05)*
  - Tham khảo giao thức giao tiếp từ APP-Votol Repository và khung Telemetry 24-byte Votol EM.
  - **Dữ liệu đã trích xuất**: Vận tốc ($km/h$ & $RPM$), Điện áp pin ($V$), Dòng điện ($A$), Công suất ($W$), Nhiệt độ IC & Động cơ ($^\circ\text{C}$), Cấp số ($P/Eco/D/Sport/R$), Trạng thái vận hành và 26 mã lỗi chi tiết.
  - **Cơ chế kỹ thuật**: Bộ đệm trượt không chặn (non-blocking sliding buffer), xác thực Checksum XOR, chạy trên Task FreeRTOS ghim vào **Core 0**, chia sẻ dữ liệu an toàn đa luồng bằng Mutex (`getSnapshot`).
  - **Mã nguồn liên quan**: `include/votol_protocol.h`, `src/votol_protocol.cpp`, `src/main.cpp`, `include/config.h`.

- [x] **Task 2.2: Module Đọc dữ liệu BMS ANT (UART)** - *(HOÀN THÀNH - 2026-10-05)*
  - Giải mã khung dữ liệu chuẩn của BMS ANT (độ dài 140 bytes, header `0xAA 0x55 0xAA 0xFF`, Checksum 16-bit Sum).
  - **Dữ liệu đã trích xuất**: % Pin (SoC), Điện áp tổng ($V$), Dòng sạc/xả có dấu ($A$), Công suất ($W$), Dung lượng còn lại/danh định ($Ah$), Mảng điện áp 32 cells (độ phân giải 1mV), Vị trí và giá trị Cell Min/Max, Độ lệch áp delta cell, 6 cảm biến nhiệt độ (MOSFET, Balancer, Pin), trạng thái đóng ngắt sạc/xả MOSFET và trạng thái cân bằng cell.
  - **Cơ chế kỹ thuật**: Chạy đồng thời trên Task FreeRTOS Core 0 cùng module Votol, cơ chế non-blocking sliding buffer, chu kỳ poll 500ms, bảo vệ bằng Mutex (`getSnapshot`).
  - **Mã nguồn liên quan**: `include/ant_bms_protocol.h`, `src/ant_bms_protocol.cpp`, `src/main.cpp`, `include/config.h`.

- [x] **Task 2.3: Module Thời gian thực (RTC DS3231) & Display (OLED)** - *(HOÀN THÀNH - 2026-10-05)*
  - Khởi tạo bus I2C (SDA 9, SCL 8 @ 400kHz) dùng chung giữa SSD1306 và DS3231.
  - Tích hợp thư viện Adafruit_SSD1306, Adafruit_GFX và RTClib.
  - **Tính năng hoàn thành**: Thiết kế giao diện Dashboard hiển thị Vận tốc font to, Cấp số (P/Eco/D/Sport/R), % Pin & Icon pin động, Đồng hồ giờ:phút đọc từ DS3231, Cờ trạng thái kết nối Votol/BMS. Tự động tương thích cả 2 chuẩn màn hình 0.91" (128x32) và 0.96" (128x64).
  - **Cơ chế kỹ thuật**: Render giao diện độc lập trên Core 1, cơ chế Rate Limiter chống nghẽn I2C (60ms ~16 FPS), cache thời gian RTC 500ms.
  - **Mã nguồn liên quan**: `include/display_rtc_manager.h`, `src/display_rtc_manager.cpp`, `src/main.cpp`, `include/config.h`, `platformio.ini`.

- [x] **Task 2.4: Luồng Giao diện & Chuyển Trang (State Machine & Button Controller)** - *(HOÀN THÀNH - 2026-10-05)*
  - **Xử lý 3 nút bấm cơ học**: Lớp `ButtonController` với Software Debounce 40ms, lọc nhiễu rung sóc xe điện, phân biệt sự kiện Click ngắn và Long Press (>1.5s).
  - **FSM State Machine 4 Trang hoàn chỉnh**:
    - **Trang 1 - Đồng hồ chính (Main Dashboard)**: Vận tốc (Font lớn), Cấp số hộp chữ bo góc, % Pin và thanh pin đồ họa, Đồng hồ RTC, Cờ kết nối Votol/BMS/BLE, Quãng đường Trip (km), Tích hợp tóm tắt chỉ đường ngã rẽ tiếp theo.
    - **Trang 2 - Thông số pin chi tiết (BMS Detail)**: Tổng áp, Dòng xả, Độ lệch áp $\Delta \text{Cell}$, Nhiệt độ BMS; nút UP/DOWN cuộn xem từng cụm 8 cell (hỗ trợ tối đa 32 cell).
    - **Trang 3 - Cài đặt (Menu Settings)**: Điều hướng danh mục cài đặt (Đường kính bánh xe, Giới hạn dòng điện, Cài đặt giờ/phút RTC, Reset quãng đường Trip). Nhấn giữ SET 1.5s để vào/thoát chế độ chỉnh sửa (Edit Mode).
    - **Trang 4 - Màn hình Điều Hướng & Âm Nhạc (Navigation / Media HUD)**: Hiển thị đồ họa Turn-by-Turn Navigation từ Google Maps / Android Auto hoặc Trình phát nhạc Bluetooth.
  - **Mô phỏng & Test**: Tích hợp phím test ảo qua Serial Monitor (`s` = SET click, `S` = SET giữ, `u` = UP, `d` = DOWN, `1/2/3/4` = chọn trang trực tiếp).
  - **Mã nguồn liên quan**: `include/button_controller.h`, `src/button_controller.cpp`, `include/display_rtc_manager.h`, `src/display_rtc_manager.cpp`, `src/main.cpp`.

- [x] **Task 2.5: Tích hợp Android Auto rút gọn / Bluetooth BLE** - *(HOÀN THÀNH - 2026-10-05)*
  - **Giao thức BLE GATT Server**: Tích hợp Nordic UART Service (NUS) chuẩn mở, tự động phát quảng bá và tái kết nối.
  - **Điều hướng Turn-by-Turn**: Giải mã lệnh chỉ đường (`NAV:<ICON>:<DIST>:<STREET>`), hỗ trợ 7 loại icon rẽ đồ họa (Thẳng, Trái, Phải, Chếch, Quay đầu, Đích đến) vẽ bằng vector hình học sắc nét.
  - **Thông tin Âm nhạc & Cuộc gọi đến**: Bắt tên bài hát / ca sĩ từ điện thoại (`MEDIA:...`), hiển thị Popup thông báo cuộc gọi đến (Call Banner Overlay) đè tạm thời 5s trên màn hình OLED.
  - **Đồng bộ Telemetry 2 chiều**: Tự động gửi gói tin JSON tốc độ, % pin SoC, dòng xả thực tế từ xe về điện thoại qua BLE TX Notify mỗi 1000ms.
  - **Mã nguồn liên quan**: `include/ble_nav_manager.h`, `src/ble_nav_manager.cpp`, `src/display_rtc_manager.cpp`, `src/main.cpp`, `include/config.h`.

---

## Giai đoạn 3: Lộ trình viết Prompt cho AI Agent / ChatGPT

- [x] **Prompt 1 (Khung chuẩn bị & Cấu trúc dự án)**: Đã tạo `platformio.ini`, `config.h`, cấu hình chân GPIO.
- [x] **Prompt 2 (Đọc IC Votol)**: Đã hoàn thành `votol_protocol.h/cpp` và kiểm tra biên dịch thành công.
- [x] **Prompt 3 (Đọc BMS ANT)**: Đã hoàn thành `ant_bms_protocol.h/cpp`, tích hợp Core 0 và test biên dịch thành công.
- [x] **Prompt 4 (Giao diện OLED, FSM & 3 Nút Bấm)**: Đã hoàn thành `display_rtc_manager.h/cpp`, `button_controller.h/cpp`, FSM 4 trang và biên dịch thành công 100%.
- [x] **Prompt 5 (Đồng bộ giờ RTC & Kết nối Android Auto / BLE)**: Đã hoàn thành `ble_nav_manager.h/cpp` và kiểm tra biên dịch thành công 100%.

---

## Giai đoạn 4: Kiểm thử & Tối ưu (Testing)

- **Test từng module riêng lẻ**:
  - [x] Test biên dịch UART Votol & gói tin giả lập (phím 't').
  - [x] Test biên dịch UART BMS ANT & gói tin giả lập (phím 'b').
  - [x] Test I2C: OLED + RTC DS3231 (Splash screen, render Dashboard & phím 'r' xem giờ RTC).
  - [x] Test nút bấm chuyển trang FSM (phím cứng GPIO 4, 5, 6 và phím ảo Serial 's', 'S', 'u', 'd', '1', '2', '3', '4').
  - [x] Test Bluetooth BLE Android Auto (phím ảo 'n' chỉ đường, 'm' nhạc, 'c' cuộc gọi, 'x' xóa).
- **Tối ưu Luồng (FreeRTOS Dual-Core)**:
  - **Core 0**: Đảm nhận việc đọc UART Votol, BMS ANT và xử lý dữ liệu Bluetooth/Android Auto BLE GATT Server.
  - **Core 1**: Chuyên trách vẽ giao diện OLED (Render 16-20 FPS) và quét phím bấm để phản hồi tức thì.