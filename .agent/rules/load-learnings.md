description: Bài học kinh nghiệm và lưu ý đặc thù phần cứng cho dự án Xe điện ESP32-S3

Hardware & Code Learnings (Ghi nhớ dự án)
1. Lưu ý phần cứng ESP32-S3
Xung đột Bus I2C: Màn hình OLED SSD1306 và RTC DS3231 dùng chung bus I2C (GPIO 8 & 9). Luôn đảm bảo khởi tạo Wire.begin(9, 8) trước khi gọi các hàm .begin() của màn hình hoặc RTC.

Mức điện áp UART Votol & BMS: Mạch UART từ IC Votol/BMS ANT có thể phát ra điện áp cao hơn 3.3V. Cần qua mạch chuyển đổi mức logic (Logic Level Shifter) hoặc trở phân áp trước khi đi vào chân RX của ESP32-S3 để tránh gây cháy chip.

2. Kinh nghiệm xử lý Data UART
Kiểm tra Checksum (CRC): Không bao giờ lấy trực tiếp dữ liệu UART mà chưa qua hàm kiểm tra tính đúng đắn (Checksum/CRC). Khung truyền từ Votol và BMS rất hay bị nhiễu do xung từ động cơ xe điện.

Tránh tràn Buffer: Sử dụng Serial1.available() đọc từng byte vào ring buffer thay vì dùng các hàm chờ nhận như readBytesUntil() gây nghẽn (blocking) luồng hiển thị OLED.

3. Tối ưu Giao diện OLED & Luồng FreeRTOS
Tần số quét màn hình (FPS): Chỉ redraw OLED khi dữ liệu thay đổi hoặc theo chu kỳ 50ms - 100ms. Việc vẽ liên tục từng ms sẽ làm CPU bị quá tải làm lag ứng dụng.

Phân chia Task FreeRTOS:

Task_UART_Read (Core 0, Priority High): Chuyên đọc dữ liệu cảm biến.

Task_UI_Render (Core 1, Priority Normal): Chuyên cập nhật màn hình và bắt sự kiện nút bấm.

Sử dụng Mutex khi truyền nhận biến cấu trúc (VotolData, BMSData) giữa 2 Core để tránh lỗi race condition.

4. Xử lý nút bấm (Debounce)
Nút bấm cơ trên xe điện rất hay bị nẩy (bounce) do rung sóc.

Bắt buộc dùng thuật toán Software Debounce (chờ 30-50ms) hoặc dùng thư viện xử lý phím bấm như OneButton để phân biệt chính xác Nhấn ngắn (Short Press) và Nhấn giữ (Long Press).