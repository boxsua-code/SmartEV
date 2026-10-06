# Coding Patterns & Conventions

---

### Non-blocking Sliding Window Buffer cho UART Xe điện
- **Ngày**: 2026-10-05
- **Task**: Task 2.1 - Module Đọc dữ liệu IC Votol (UART)
- **Chi tiết**:
  Khi đọc dữ liệu UART từ IC xe điện, xung nhiễu từ động cơ rất dễ làm mất byte hoặc chèn byte rác. Áp dụng kỹ thuật sliding window buffer trong vòng lặp `update()`: gom byte bằng `Serial.available()`, kiểm tra Header `0xC0 0x14` và xác thực Checksum XOR trên frame 24-byte. Nếu checksum sai, chỉ trượt bỏ 1 byte thay vì xóa toàn bộ buffer để không bỏ lỡ frame tiếp theo. Tuyệt đối không dùng hàm chặn như `readBytesUntil()`.
- **Files liên quan**: `include/votol_protocol.h`, `src/votol_protocol.cpp`

---

### Giải mã Frame 140 bytes BMS ANT với Checksum 16-bit Sum
- **Ngày**: 2026-10-05
- **Task**: Task 2.2 - Module Đọc dữ liệu BMS ANT (UART)
- **Chi tiết**:
  Khung dữ liệu BMS ANT chuẩn có độ dài 140 bytes với header cố định `0xAA 0x55 0xAA 0xFF`. Checksum được tính bằng tổng số học 16-bit (Sum of bytes) từ byte index 4 đến 137 (134 bytes), so khớp với 2 bytes cuối (byte 138-139, Big-Endian). Lệnh kích hoạt truy vấn tiêu chuẩn là chuỗi 6 bytes `0x5A 0x5A 0x00 0x00 0x00 0x00` gửi theo chu kỳ 500ms.
- **Files liên quan**: `include/ant_bms_protocol.h`, `src/ant_bms_protocol.cpp`

---

### Tối ưu Bus I2C Dùng Chung Màn hình OLED và RTC DS3231
- **Ngày**: 2026-10-05
- **Task**: Task 2.3 - Module Thời gian thực (RTC DS3231) & Display (OLED)
- **Chi tiết**:
  Khi dùng chung bus I2C (GPIO 9 SDA, GPIO 8 SCL) giữa OLED SSD1306 và RTC DS3231, bắt buộc gọi `Wire.begin(sda, scl, 400000)` trước khi khởi tạo các thiết bị. Để tránh xung đột băng thông bus I2C, không bao giờ đọc dữ liệu từ RTC trong mỗi frame render. Thay vào đó, áp dụng mẫu thiết kế Cached Timer: lưu `DateTime` vào biến đệm và chỉ truy vấn vật lý chip DS3231 mỗi 500ms.
- **Files liên quan**: `include/display_rtc_manager.h`, `src/display_rtc_manager.cpp`

---

### Software Debounce Nút Bấm và FSM Menu Xe Điện
- **Ngày**: 2026-10-05
- **Task**: Task 2.4 - Luồng Giao diện & Chuyển Trang (State Machine & Button Controller)
- **Chi tiết**:
  Trên xe điện, rung chấn cơ học và xung nhiễu điện từ cuộn dây động cơ gây ra hiện tượng bouncing phím rất mạnh. Áp dụng debounce thời gian thực `_debounceMs = 40ms` kết hợp theo dõi thời gian nhấn giữ `_longPressMs = 1500ms`. Trả về sự kiện tách biệt `CLICK` và `LONG_PRESS`.
  Trong FSM giao diện:
  - Phím SET Click: Chuyển tuần hoàn 4 trang (Main -> BMS Detail -> Settings -> Navigation -> Main).
  - Phím SET Long Press trong Settings: Bật / Tắt chế độ Edit (`_isEditMode`).
  - Phím UP / DOWN: Điều hướng danh mục khi ở chế độ xem; Tăng / Giảm thông số (đường kính bánh xe, giới hạn dòng, giờ phút RTC) khi ở chế độ Edit.
- **Files liên quan**: `include/button_controller.h`, `src/button_controller.cpp`, `include/display_rtc_manager.h`, `src/display_rtc_manager.cpp`

---

### BLE GATT Server (Nordic UART Service) Cho Android Auto Navigation & Media HUD
- **Ngày**: 2026-10-05
- **Task**: Task 2.5 - Tích hợp Android Auto rút gọn / Bluetooth BLE
- **Chi tiết**:
  Thay vì truyền tải video stream H.264 cồng kềnh không phù hợp với màn hình OLED nhỏ (128x32/128x64), áp dụng giao thức BLE Peripheral chuẩn Nordic UART Service (NUS, Service UUID `6E400001-...`):
  - Nhận gói tin Text nhanh qua RX Characteristic (`WRITE`):
    - Dẫn đường Turn-by-Turn: `NAV:<ICON>:<DIST>:<STREET>` (ví dụ `NAV:LEFT:250m:Nguyen Hue`) với 7 loại icon đồ họa mũi tên (Thẳng, Trái, Phải, Chếch, Quay đầu, Đích đến).
    - Trình phát nhạc: `MEDIA:<TITLE>:<ARTIST>:<STATUS>`.
    - Thông báo cuộc gọi đến: `CALL:<CALLER_INFO>` kích hoạt Call Banner Overlay đè tạm thời 5 giây lên màn hình.
  - Phản hồi Telemetry hai chiều qua TX Characteristic (`NOTIFY`): Gửi dữ liệu tốc độ, cấp số, % pin SoC, điện áp, dòng điện thực tế về điện thoại mỗi 1000ms dưới dạng JSON rút gọn.
- **Files liên quan**: `include/ble_nav_manager.h`, `src/ble_nav_manager.cpp`, `include/display_rtc_manager.h`, `src/display_rtc_manager.cpp`

---

### Quản Lý Tín Hiệu Xi Nhan Chớp Nháy & Đèn Pha Trên Đồng Hồ Xe Điện
- **Ngày**: 2026-10-05
- **Nâng cấp**: Tín hiệu Đèn và Xi nhan xe điện (Signals & Indicators)
- **Chi tiết**:
  - Xi nhan trái / phải được tạo nhịp chớp nháy mềm mại `(millis() / TURN_SIGNAL_BLINK_MS) % 2 == 0` (chu kỳ 400ms ON / 400ms OFF). Khi bật, mũi tên `◀` hoặc `▶` chuyển đổi giữa trạng thái tô đặc `fillTriangle` và viền rỗng `drawTriangle`, mang lại trải nghiệm trực quan như taplo ô tô / xe máy hiện đại.
  - Đèn pha (High Beam): Vẽ chóa đèn cong bo tròn cùng 3 tia sáng ngang vươn sang phải, duy trì sáng liên tục khi đèn pha bật.
  - Hỗ trợ đọc cả GPIO vật lý (Active LOW qua Optocoupler cách ly bảo vệ khỏi điện áp 12V của xe điện) lẫn phím mô phỏng bàn phím qua Serial (`[`, `]`, `l`, `h`).
- **Files liên quan**: `include/vehicle_signals.h`, `src/vehicle_signals.cpp`, `include/display_rtc_manager.h`, `src/display_rtc_manager.cpp`

---

### Lưu Trữ Cấu Hình Xe Điện Chuyên Sâu Qua Flash NVS (Preferences)
- **Ngày**: 2026-10-05
- **Nâng cấp**: Cài đặt chuyên sâu xe điện (Advanced EV Settings)
- **Chi tiết**:
  Xe điện có nguồn điện acquy/pin dao động lớn và thường xuyên tắt mở khoá điện (Key switch). Mọi thông số hiệu chỉnh:
  - Cắt áp bảo vệ pin yếu (Low Voltage Cutoff 50-80V)
  - Cảnh báo quá nhiệt IC / Motor (60-110°C)
  - Cảnh báo lệch áp cell BMS (10-100mV)
  - Số cặp cực động cơ & tỉ số truyền
  - Độ sáng tương phản OLED (Contrast 10-255)
  - Cảnh báo quá tốc độ & đơn vị đo (km/h hoặc mph)
  Được lưu trữ vĩnh viễn trong phân vùng NVS (Non-volatile Storage) của ESP32 qua thư viện `Preferences`. Người dùng chỉnh sửa trực tiếp trên màn hình OLED bằng 3 phím bấm (nhấn giữ SET 1.5s để vào Edit, UP/DOWN tăng giảm, SET ngắn để Lưu vào Flash).
- **Files liên quan**: `include/display_rtc_manager.h`, `src/display_rtc_manager.cpp`



