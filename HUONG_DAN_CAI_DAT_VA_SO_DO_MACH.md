# HƯỚNG DẪN KỸ THUẬT: SƠ ĐỒ ĐI DÂY BO MẠCH VÀ CÀI ĐẶT HỆ THỐNG SMART EV DASHBOARD

Tài liệu này hướng dẫn chi tiết sơ đồ chân hàn dây phần cứng cho bo mạch **ESP32-S3**, cách đấu nối với **IC Votol EM150sp**, **ANT BMS**, **Màn hình OLED**, **Đồng hồ RTC**, **Đèn/Xi-nhan**, cùng quy trình cài đặt ứng dụng trên **Điện thoại Android** và **Màn hình Android Auto**.

---

## PHẦN 1: BẢN VẼ SƠ ĐỒ CHÂN & HÀN DÂY PHẦN CỨNG (PINOUT)

### 1. Bảng tra cứu chân GPIO trên bo mạch ESP32-S3

| Tên thiết bị / Chức năng | Chân trên Thiết bị | Chân GPIO trên ESP32-S3 | Chú thích kỹ thuật quan trọng |
| :--- | :--- | :--- | :--- |
| **Màn hình OLED SSD1306**<br>*(I2C Bus 400kHz)* | **SCL**<br>**SDA**<br>**VCC**<br>**GND** | **GPIO 8**<br>**GPIO 9**<br>**3V3 (hoặc 5V)**<br>**GND** | Dùng chung 2 dây SDA/SCL với module RTC DS3231.<br>Hỗ trợ màn hình 0.91" (128x32) hoặc 0.96" (128x64). |
| **Đồng hồ RTC DS3231**<br>*(Lưu giờ thực tế)* | **SCL**<br>**SDA**<br>**VCC**<br>**GND** | **GPIO 8** (nối chung SCL)<br>**GPIO 9** (nối chung SDA)<br>**3V3 (hoặc 5V)**<br>**GND** | Giữ giờ thực kể cả khi rút bình điện nhờ pin cúc áo CR2032 trên module. |
| **IC Votol EM150sp**<br>*(Giắc cắm UART)* | **TX** (Dây truyền)<br>**RX** (Dây nhận)<br>**GND** | **GPIO 18** (RX1 của ESP32)<br>**GPIO 17** (TX1 của ESP32)<br>**GND** | **BẮT BUỘC NỐI CHUNG GND** giữa IC Votol và ESP32.<br>Tốc độ: 9600 Baud. |
| **Mạch ANT BMS**<br>*(Giắc cắm UART)* | **TX** (Dây truyền)<br>**RX** (Dây nhận)<br>**GND** | **GPIO 16** (RX2 của ESP32)<br>**GPIO 15** (TX2 của ESP32)<br>**GND** | **BẮT BUỘC NỐI CHUNG GND** giữa ANT BMS và ESP32.<br>Tốc độ: 19200 Baud. |
| **Nút bấm Đánh thức (Wake BLE)**<br>*(1 Nút nhấn nhả duy nhất)* | **Nút WAKE (Đánh thức BLE)** | **GPIO 4** | Chỉ 1 nút bấm duy nhất:<br>• 1 dây hàn vào chân **GPIO 4**<br>• 1 dây hàn vào **GND**<br>Nhấn để bật lại Bluetooth khi chờ lâu.<br>*(Firmware đã bật sẵn điện trở Pull-up nội, không cần gắn trở ngoài).* |
| **Tín hiệu Đèn & Xi-nhan**<br>*(Qua mạch cách ly Opto PC817)* | **Xi-nhan Trái (Turn Left)**<br>**Xi-nhan Phải (Turn Right)**<br>**Đèn Pha (High Beam)** | **GPIO 1**<br>**GPIO 2**<br>**GPIO 7** | Đọc tín hiệu 12V từ công tắc đèn xe máy điện.<br>**Bắt buộc qua Optocoupler để tránh cháy chip** (xem sơ đồ cách ly bên dưới). |
| **Nguồn cấp cho ESP32-S3** | **+5V (Cực dương)**<br>**GND (Mass xe)** | **Chân 5V / VIN**<br>**Chân GND** | Lấy từ tẩu sạc USB xe điện hoặc cục hạ áp Buck DC-DC 12V -> 5V (dòng tối thiểu 1.5A - 2A). |

---

### 2. Sơ đồ khối nối dây trực quan (Schematic Diagram)

```text
               ┌────────────────────────────────────────────────────────┐
               │                  ESP32-S3 DEVKIT                       │
               │                                                        │
[OLED & RTC]   │ GPIO 8 (SCL) ──────────────┬─── SCL (Màn hình OLED)    │
(Bus I2C chung)│                            └─── SCL (Module RTC DS3231)│
               │ GPIO 9 (SDA) ──────────────┬─── SDA (Màn hình OLED)    │
               │                            └─── SDA (Module RTC DS3231)│
               │ 3V3 / 5V     ────────────────── VCC (OLED & RTC)       │
               │ GND          ────────────────── GND (OLED & RTC)       │
               │                                                        │
[IC VOTOL]     │ GPIO 17 (TX1) ───────────────── RX (Giắc UART Votol)   │
(Cổng UART 1)  │ GPIO 18 (RX1) ───────────────── TX (Giắc UART Votol)   │
               │ GND          ────────────────── GND (Chung IC Votol)   │
               │                                                        │
[ANT BMS]      │ GPIO 15 (TX2) ───────────────── RX (Giắc UART ANT BMS) │
(Chân chờ)     │ GPIO 16 (RX2) ───────────────── TX (Giắc UART ANT BMS) │
               │ GND          ────────────────── GND (Chung ANT BMS)    │
               │                                                        │
[NÚT BẤM WAKE] │ GPIO 4 (WAKE) ───[ Nút Nhấn WAKE ]─── GND               │
               │                                                        │
[ĐÈN & XI-NHAN]│ GPIO 1 ────────── Chân C (Collector) Opto Xi-nhan Trái│
               │ GPIO 2 ────────── Chân C (Collector) Opto Xi-nhan Phải │
               │ GPIO 7 ────────── Chân C (Collector) Opto Đèn Pha      │
               │                                                        │
[NGUỒN HỆ THỐNG]│ 5V / VIN      ────── +5V (Tẩu sạc USB / Hạ áp Buck 5V) │
               │ GND           ────── GND MASS CHUNG TOÀN XE           │
               └────────────────────────────────────────────────────────┘
```

---

### 3. Mạch cách ly tín hiệu Đèn 12V (Bảo vệ an toàn cho ESP32)

> **CẢNH BÁO QUAN TRỌNG:**
> Điện áp trên hệ thống đèn xi-nhan và đèn pha xe điện là **12V** (từ bộ đổi nguồn DC-DC). Chân GPIO của ESP32 chỉ chịu điện áp tối đa **3.3V**. Nối trực tiếp 12V vào ESP32 sẽ làm nổ vi điều khiển ngay lập tức!

#### Giải pháp khuyên dùng: Dùng Module Optocoupler PC817 (4 kênh)
- **Đầu vào 12V (Phía xe)**:
  - Dây dương (+) bóng đèn (12V) ─── **Trở 1.5kΩ - 2kΩ** ─── Chân Anode (1) của PC817.
  - Dây mass (-) bóng đèn (GND 12V) ─────────────────────── Chân Cathode (2) của PC817.
- **Đầu ra 3.3V (Phía ESP32)**:
  - Chân Collector (4) của PC817 ──────────────────────── Chân GPIO (1, 2, hoặc 7).
  - Chân Emitter (3) của PC817 ────────────────────────── Chân GND của ESP32.

*Nguyên lý:* Khi bật xi-nhan hoặc đèn pha, điện áp 12V làm sáng LED bên trong Opto -> kéo chân GPIO xuống GND (kích hoạt mức LOW an toàn 100%).

---

## PHẦN 2: HƯỚNG DẪN THAO TÁC 3 NÚT BẤM TRÊN XE

Màn hình OLED có **5 trang hiển thị** luân chuyển:

| Nút bấm | Cách thao tác | Chức năng thực hiện |
| :--- | :--- | :--- |
| **Nút SET**<br>*(GPIO 4)* | **Nhấn 1 lần (Click)** | Chuyển trang màn hình: <br>Trang 1 (Cockpit) ➔ Trang 2 (BMS Cell) ➔ Trang 3 (Cài đặt nhanh) ➔ Trang 4 (Cài đặt chuyên sâu NVS) ➔ Trang 5 (Dẫn đường HUD). |
| **Nút SET**<br>*(GPIO 4)* | **Nhấn giữ 1.5 giây** | • Tại Trang 3 & 4: Vào/Thoát chế độ chỉnh sửa thông số.<br>• Tại màn hình HUD: Tắt nhanh chỉ đường. |
| **Nút UP**<br>*(GPIO 5)* | **Nhấn 1 lần** | • Tăng giá trị thông số đang chọn (Cỡ lốp mm, Dòng xả A, Cặp cực động cơ).<br>• Chuyển chế độ lái (Eco ➔ Normal ➔ Sport). |
| **Nút DOWN**<br>*(GPIO 6)* | **Nhấn 1 lần** | • Giảm giá trị thông số đang chọn.<br>• Giảm chế độ lái (Sport ➔ Normal ➔ Eco). |

---

## PHẦN 3: HƯỚNG DẪN CÀI ĐẶT ỨNG DỤNG ANDROID & ANDROID AUTO

### Bước 1: Mở dự án bằng Android Studio
1. Mở phần mềm **Android Studio** trên máy tính:
   `C:\Program Files\Android\Android Studio`
2. Chọn **Open** và chọn thư mục:
   `E:\ESP32\ESP32_Android Auto\android_app`
3. Chờ Android Studio tải xong thư viện và báo **"BUILD SUCCESSFUL"**.

### Bước 2: Cài đặt ứng dụng vào điện thoại
1. Trên điện thoại Android: Vào **Cài đặt** ➔ **Tùy chọn nhà phát triển** ➔ Bật **Gỡ lỗi qua USB (USB debugging)**.
2. Cắm cáp USB nối điện thoại với máy tính.
3. Trong Android Studio, chọn thiết bị điện thoại của bạn trên thanh công cụ và nhấn nút **Run 'app'** (tam giác xanh ▶).
4. Mở app **Smart EV Dashboard** trên điện thoại và bấm **"Cho phép"** các quyền Bluetooth, Vị trí và Đọc thông báo.

### Bước 3: Sử dụng trên Điện thoại
- Bấm nút **"KẾT NỐI XE"**: Ứng dụng sẽ tự động quét và kết nối với bo mạch `ESP32-SmartDash` qua Bluetooth.
- Xem vận tốc xe, dung lượng pin, đèn pha, xi-nhan và chi tiết 32 cell pin.
- Tab **"Cài đặt xe"**: Cho phép bạn nhập cỡ lốp, dòng xả Bus/Phase, phanh tái sinh và nhấn **"LƯU CÀI ĐẶT VÀO XE"** để ghi vĩnh viễn vào bộ nhớ Flash của ESP32.

### Bước 4: Chiếu lên màn hình Android Auto trên ô tô
- Dùng cáp USB nối điện thoại vào cổng Android Auto của xe hơi (hoặc kết nối không dây Wireless Android Auto).
- Chọn biểu tượng **Smart EV Dashboard** trên màn hình lớn của xe để xem giao diện Digital Cockpit cảm ứng cực kỳ sắc nét và hiện đại!

---

## PHẦN 4: LỆNH NẠP LẠI FIRMWARE ESP32 KHI CẦN

Nếu trong tương lai bạn muốn cập nhật hoặc nạp lại firmware cho ESP32-S3, mở PowerShell trong thư mục dự án và chạy lệnh:
```powershell
& "C:\Users\boxsu\.platformio\penv\Scripts\pio.exe" run --target upload --upload-port COM6
```
*(Thay `COM6` bằng cổng COM tương ứng của bo mạch trên máy tính).*
