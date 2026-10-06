(Kỹ năng thiết kế giao diện OLED và quản lý trạng thái các trang)

name: UI OLED Manager
description: Quản lý các trang hiển thị OLED 0.91 inch và chuyển đổi bằng nút bấm

Instruction for OLED Screen & State Machine
Thư viện sử dụng: Ưu tiên U8g2lib hoặc Adafruit_SSD1306.

Các trang hiển thị (UI States):

STATE_MAIN (Đồng hồ chính):

Giờ hiện tại (lấy từ RTC DS3231)

Vận tốc km/h (Font to, rõ ràng)

% Pin (Icon pin + con số)

Chế độ số (P/N/D/S)

Trạng thái kết nối (Icons: Votol, BMS, Bluetooth)

STATE_BMS_INFO (Chi tiết Pin):

Điện áp cell min/max

Dòng xả thực tế

Nhiệt độ khối pin

STATE_SETTINGS (Cài đặt):

Chỉnh bán kính bánh xe (để tính đúng tốc độ)

Cài đặt giờ RTC

Reset Trip km

Xử lý nút bấm (Button Handling):

Nhấn BTN_SET (Short Press): Chuyển giữa các trang (MAIN -> BMS_INFO -> SETTINGS -> MAIN).

Nhấn giữ BTN_SET (Long Press > 2s): Vào chế độ chỉnh sửa thông số trong SETTINGS.

Nhấn BTN_UP / BTN_DOWN: Tăng/Gảm giá trị khi đang ở chế độ chỉnh sửa.