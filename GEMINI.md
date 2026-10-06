# KHO TRI THỨC TỰ HỌC DỰ ÁN ESP32-S3 SMART DASHBOARD & ANDROID AUTO

> **Ghi chú:** File này lưu trữ toàn bộ các quy tắc phần cứng, chuẩn giao thức truyền thông, kinh nghiệm sửa lỗi, và cấu hình tối ưu của dự án để AI luôn ghi nhớ và áp dụng chính xác.

---

## 1. QUY TẮC PHẦN CỨNG VI ĐIỀU KHIỂN ESP32-S3

* **Giới hạn chân GPIO trên ESP32-S3:**
  * ESP32-S3 **KHÔNG CÓ GPIO 22, 23, 24, 25** (chip nhảy thẳng từ GPIO 21 sang 26). Tuyệt đối không cấu hình các chân này.
* **Sơ đồ chân chuẩn của xe:**
  * **UART 1 (Votol EM150sp):** `RX = GPIO 18` (nhận từ TX Votol), `TX = GPIO 17` (phát tới RX Votol).
  * **UART 2 (BMS ANT / Chân chờ):** `RX = GPIO 16`, `TX = GPIO 15` (chế độ thụ động an toàn).
  * **I2C Bus (OLED SSD1306 + RTC DS3231):** `SCL = GPIO 8`, `SDA = GPIO 9`, tốc độ 400kHz.
  * **Nút bấm Đánh thức (Wake BLE):** Duy nhất 1 nút tại `GPIO 4` (kéo xuống GND).
  * **Chân tín hiệu xe (Active LOW):** Xi nhan Trái (`GPIO 1`), Xi nhan Phải (`GPIO 2`), Đèn Pha (`GPIO 7`).
  * **Dự phòng CAN Bus:** Nếu xe dùng CAN Bus qua transceiver SN65HVD230: Dùng `CAN_TX: GPIO 5`, `CAN_RX: GPIO 6`, Baudrate chuẩn EV là `250000` (250 kbps).
* **Nối mass chung (GND):** Bắt buộc nối chân GND của ESP32 với GND của IC Votol/xe điện để tránh nhiễu và điện áp trôi nổi làm mất tín hiệu UART.

---

## 2. GIAO THỨC TRUYỀN THÔNG IC VOTOL (EM SERIES)
*(Tham khảo chuẩn xác từ cộng đồng reverse-engineering Ming2k8-Coder/VotolAIO)*

* **Tốc độ Baudrate:** Mặc định **`9600 bps`** (phụ: `115200 bps`), 8N1.
* **Lệnh Handshake đánh thức Votol (`LDGET` - 24 bytes):**
  ```hex
  C9 14 02 4C 44 47 45 54 00 00 00 00 00 00 00 00 00 00 00 00 00 00 81 0D
  ```
  * Phải gửi lệnh này khi khởi động để đánh thức IC Votol mở cổng giao tiếp.
* **Lệnh Thăm dò Telemetry (`SHOW` - 24 bytes):**
  ```hex
  C9 14 02 53 48 4F 57 00 00 00 00 00 AA 00 00 00 00 AA 00 00 00 00 DC 0D
  ```
  * Byte 12 = `0xAA`: Local control (xe tự lái, không chiếm quyền tay ga qua UART gây lỗi IC).
  * Byte 16 = `0x00`, Checksum XOR byte 0..21 = `0xDC`, Byte kết thúc = `0x0D`.
* **Cấu trúc gói phản hồi dữ liệu đồng hồ (`YB` - 24 bytes):**
  * Header: `0xC0 0x14 0x0D 0x59 0x42` (hoặc `0xC0 0x14`)
  * `Byte 5 & 6`: Điện áp pin = `raw / 10.0f` (Volt)
  * `Byte 7 & 8`: Dòng điện xả/nạp = `raw / 10.0f` (Amps)
  * `Byte 10~13`: Mã lỗi hệ thống 32-bit bitmask
  * `Byte 14 & 15`: Vòng tua động cơ (RPM) -> Tính ra $km/h$ theo chu vi bánh xe
  * `Byte 16`: Nhiệt độ IC điều tốc = `raw - 50` (°C)
  * `Byte 17`: Nhiệt độ động cơ / cảm biến ngoài = `raw - 50` (°C)
  * `Byte 20`: Giải mã Cấp số, Tín hiệu nút bấm và Cảm biến xe (Chuẩn VotolAIO):
    * **Nhóm 4 bit thấp (Low nibble):**
      * `Bits 0-1 (0x03)`: Cấp số: `0 = Low (Eco)`, `1 = Mid (Drive)`, `2 = High (Sport)`, `3 = Super Sport`
      * `Bit 2 (0x04)`: Số lùi (**Reverse / R**)
      * `Bit 3 (0x08)`: Số Parked (**P**) - *Lưu ý: Không nhầm lẫn với 0x80*
    * **Nhóm 4 bit cao (High nibble):**
      * `Bit 4 (0x10)`: Bóp phanh điện (**Brake**)
      * `Bit 5 (0x20)`: Khóa chống trộm (**Lock / Anti-theft**)
      * `Bit 6 (0x40)`: Gạt chân chống (**SideStand**)
      * `Bit 7 (0x80)`: Phanh tái tạo điện tử (**Regen**)
  * `Byte 21`: Trạng thái IC (0: IDLE, 1: INIT, 2: START, 3: RUN, 4: STOP, 5: BRAKE, 6: WAIT, 7: FAULT)
  * `Byte 22`: Checksum XOR của 22 bytes đầu.

---

## 3. PHÁT TRIỂN ỨNG DỤNG ANDROID AUTO (CAR APP LIBRARY)

* **Thư viện bắt buộc cho Android Auto Projection:**
  * Cần cả `androidx.car.app:app` và `androidx.car.app:app-projected` trong `build.gradle`. Nếu thiếu `app-projected`, điện thoại sẽ không chiếu được giao diện sang màn hình ô tô.
* **Category trong AndroidManifest:**
  * Màn hình Digital Cockpit (đồng hồ đo, pin, tốc độ) trả về `PaneTemplate` hoặc `GridTemplate`.
  * **Chỉ khai báo DUY NHẤT một category:** `androidx.car.app.category.POI` trong `intent-filter` của `CarAppService`.
  * **Tuyệt đối không gộp chung nhiều category** (như cả `POI` và `IOT`) trong cùng 1 filter vì sẽ khiến Android Auto Host từ chối bind service.
  * **Tuyệt đối không dùng** `category.NAVIGATION` nếu không triển khai turn-by-turn NavigationManager.
* **File mô tả `automotive_app_desc.xml`:**
  * Chỉ khai báo: `<uses name="template" />`.
* **GIẢI PHÁP TRIỆT ĐỂ CHO LỖI APP KHÔNG HIỆN TRÊN MÀN HÌNH ANDROID AUTO (SIDELOAD RESTRICTION):**
  * **Nguyên nhân kỹ thuật:** Google áp đặt cơ chế bảo mật trên Android Auto: Các ứng dụng dùng Car App Library (`androidx.car.app`) khi cài đặt trực tiếp qua file APK (sideload) sẽ bị Android Auto Host ẩn đi vì trình cài đặt không phải là Google Play Store (`com.android.vending`).
  * **3 Cách khắc phục để App xuất hiện 100%:**
    1. **Cách 1 (Khuyên dùng trên điện thoại): Cài qua KingInstaller / AAAD:**
       - Tải và cài đặt app **KingInstaller** trên điện thoại.
       - Mở KingInstaller, chọn file `SmartEV-Dashboard.apk` và nhấn Install. KingInstaller sẽ giả lập nguồn cài đặt là từ Google Play Store, giúp Android Auto nhận diện app ngay lập tức.
    2. **Cách 2 (Cài qua ADB từ máy tính):**
       - Bật Gỡ lỗi USB (USB Debugging) trên điện thoại và cắm vào máy tính.
       - Chạy lệnh: `adb install -i com.android.vending SmartEV-Dashboard.apk`. Cờ `-i com.android.vending` khai báo nguồn cài là Google Play Store.
    3. **Cách 3 (Cấu hình Android Auto Developer Settings trên điện thoại):**
       - Mở **Cài đặt Android Auto** trên điện thoại > Cuộn xuống dưới cùng chạm 10 lần vào dòng **Phiên bản** để mở menu Nhà phát triển.
       - Nhấn dấu 3 chấm góc trên > **Cài đặt cho nhà phát triển (Developer settings)**.
       - Tích chọn **Nguồn không xác định (Unknown sources)** và chọn **Chế độ ứng dụng (Application Mode) = Nhà phát triển (Developer)**.
       - Quay lại màn hình Cài đặt Android Auto > Vào **Tùy chỉnh trình khởi chạy (Customize launcher)** > **TÍCH CHỌN `Smart EV Dashboard`** (Mặc định Android Auto sẽ bỏ tích các app mới cài ngoài).
       - Vào Cài đặt máy > Ứng dụng > Android Auto > Bộ nhớ > **Xóa bộ nhớ đệm (Clear cache)** rồi cắm lại cáp vào xe.

---

## 4. MÔI TRƯỜNG BIÊN DỊCH VÀ HỆ THỐNG MÁY TÍNH

* **JDK sử dụng:** Dùng JDK 21 tại `C:\Users\boxsu\.jdks\jbr-21.0.11` (Tránh dùng JBR 25 vì không tương thích với Groovy 3 trong Gradle).
* **Đường dẫn Gradle:** `GRADLE_USER_HOME=E:\.gradle` (Vì ổ C dung lượng rất thấp, ổ E còn >780 GB).
* **Cơ chế xác nhận:** Người dùng đã giao toàn quyền tự động biên dịch, nạp firmware qua cổng Serial, và build APK mà không cần dừng lại hỏi xác nhận.
