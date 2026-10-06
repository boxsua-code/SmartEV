# Bugs & Solutions

---

### Xử lý Offset nhiệt độ và Khung truyền Big-Endian trên IC Votol
- **Ngày**: 2026-10-05
- **Task**: Task 2.1 - Module Đọc dữ liệu IC Votol (UART)
- **Chi tiết**:
  - Dữ liệu nhiệt độ (Byte 16 cho IC, Byte 17 cho Động cơ) trong khung truyền Votol có offset $+50^\circ\text{C}$. Nhiệt độ thực tế phải tính bằng `byte - 50` để hỗ trợ nhiệt độ âm.
  - Các trường 16-bit như Điện áp (đơn vị 0.1V), Dòng điện (đơn vị 0.1A có dấu) và RPM truyền theo định dạng Big-Endian (`(HighByte << 8) | LowByte`).
  - Cần thêm cơ chế Watchdog timeout (2000ms): nếu mất tín hiệu UART, tự động gán tốc độ và dòng điện về 0 để an toàn hiển thị.
- **Files liên quan**: `src/votol_protocol.cpp`

---

### Trích xuất Dòng điện Signed 32-bit và Điện áp Cell BMS ANT
- **Ngày**: 2026-10-05
- **Task**: Task 2.2 - Module Đọc dữ liệu BMS ANT (UART)
- **Chi tiết**:
  - Dòng điện của BMS ANT nằm ở 4 bytes (70..73) biểu diễn dưới dạng số nguyên có dấu 32-bit (Big-Endian), đơn vị $0.1\text{A}$. Ép kiểu `(int32_t)rawVal * 0.1f` để nhận diện chính xác dòng xả (+) và dòng sạc (-).
  - Mảng 32 cell (bytes 6..69) được lưu ở dạng $1\text{mV}$ Big-Endian, nhân với $0.001\text{f}$ để ra Volts.
  - Cần lấy số lượng cell thực tế từ byte 123 (`batteryStrings`) để tránh đọc các cell không tồn tại.
- **Files liên quan**: `include/ant_bms_protocol.h`, `src/ant_bms_protocol.cpp`

---

### Khắc phục Lag Màn hình và Mất Giờ khi Reset RTC DS3231
- **Ngày**: 2026-10-05
- **Task**: Task 2.3 - Module Thời gian thực (RTC DS3231) & Display (OLED)
- **Chi tiết**:
  - Khi pin nuôi CR2032 của module RTC bị cạn, hàm `rtc.lostPower()` trả về true khiến thời gian nhảy về 01/01/2000. Khắc phục bằng cách tự động nạp `DateTime(F(__DATE__), F(__TIME__))` từ macro biên dịch compiler.
  - Hỗ trợ linh hoạt cả 2 độ phân giải màn hình 0.91" (128x32) và 0.96" (128x64) qua `#define OLED_SCREEN_HEIGHT` giúp đổi linh kiện phần cứng mà không phải viết lại code vẽ.
- **Files liên quan**: `src/display_rtc_manager.cpp`, `include/config.h`
