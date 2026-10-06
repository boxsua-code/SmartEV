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

---

## 5. KINH NGHIỆM TƯƠNG THÍCH JAMFOXRS & SỬA LỖI TÍN HIỆU NÚT BẤM

* **Tránh lỗi kẹt tín hiệu nút bấm CAN Bus (Latching Bug):**
  * Trong hàm xử lý gói tin CAN `0x0A010810`, mode byte `m` chứa trạng thái P, R, Phanh, Chân chống.
  * **Quy tắc bắt buộc:** Các biến boolean trạng thái (`brake`, `sideStand`, `reverse`, `parked`) phải được gán trực tiếp bằng biểu thức điều kiện boolean của chu kỳ hiện tại (Ví dụ: `vd.brake = (m == 0x72 || m == 0xB2);`). Tuyệt đối không viết dạng `if (m == 0x72) vd.brake = true;` mà thiếu nhánh trả về `false`, vì sẽ làm đèn báo phanh/chân chống bị kẹt sáng vĩnh viễn trên màn hình sau lần nhả đầu tiên.
* **Tương thích BLE Kép (SmartEV + JAMFOXRS):**
  * App Android được trang bị khả năng nhận diện đa thiết bị: Tên phát sóng `ESP32-SmartDash`, `JAMFOXRS`, `Votol_BLE`, `Votol_TCH`.
  * Hỗ trợ tự động cả 2 Service GATT:
    * Nordic UART Service: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
    * JAMFOXRS Service: `4fafc201-1fb5-459e-8fcc-c5c9c331914b` (Telemetry: `beb5483e-36e1-4688-b7f5-ea07361b26a8`)
  * Trình giải mã JSON thông minh tự động nhận diện cả cú pháp SmartEV (`{"spd":...}`) và JAMFOXRS Fast/Full (`{"r":...,"s":...,"m":...}`). Dù xe đang cắm bo mạch ESP32 đời nào thì App điện thoại và Android Auto đều nhận tín hiệu mượt mà.
* **Phím tắt mở Cài đặt Android Auto trên điện thoại:**
  * App đã có nút nhấn mở trực tiếp `com.google.android.gms.car.settings.PROJECTION_SETTINGS` kèm hướng dẫn 4 bước trực quan ngay trên tab Android Auto để người dùng dễ dàng bật *Unknown sources* và tích chọn *Smart EV Dashboard* trong *Customize launcher*.

---

## 6. KINH NGHIỆM KHẮC PHỤC TRIỆT ĐỂ LỖI KẾT NỐI & GHÉP ĐÔI BLUETOOTH BLE

* **Thứ tự gọi Buffer UART trên ESP32:**
  * `setRxBufferSize(...)` **BẮT BUỘC PHẢI GỌI TRƯỚC** `begin(...)` (Ví dụ: `_serial->setRxBufferSize(256); _serial->begin(baud, ...);`). Nếu gọi sau, ESP32 sẽ báo lỗi nghiêm trọng `HardwareSerial: RX Buffer can't be resized when Serial is already running` và làm ngưng trệ chu trình khởi động.
* **Nguyên nhân thiết bị BLE bị ẩn tên ("Unknown / Null") do tràn 31 bytes:**
  * Giới hạn gói tin quảng bá chuẩn BLE là 31 bytes: Cờ Flags (3B) + Tên "ESP32-SmartDash" (17B) + UUID 128-bit (18B) = 38 bytes > 31 bytes! Khi vượt ngưỡng, Bluedroid sẽ tự động cắt bỏ tên, khiến điện thoại chỉ thấy thiết bị vô danh ("Unknown") và ẩn đi trong cài đặt Bluetooth.
  * **Giải pháp chuẩn:** Đặt Tên thiết bị vào `advData` (Primary Advertisement Packet), và đặt UUID 128-bit vào `scanRespData` (Scan Response Packet). Điện thoại sẽ thấy tên ngay lập tức ở packet đầu tiên.
* **Cấu hình ghép đôi trực tiếp từ Cài đặt Bluetooth của Điện thoại (Pairing / Bonding):**
  * Để điện thoại bấm "Ghép đôi" (Pair) thành công từ Settings của máy mà không bị báo lỗi "Mã PIN không đúng / Bị từ chối", ESP32 bắt buộc phải đăng ký đầy đủ cả Encryption Key khởi tạo và Key phản hồi:
    ```cpp
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE); // "Just Works"
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK); // BẮT BUỘC: Thiếu cờ này Android sẽ từ chối kết nối
    ```
  * **LỖI STALE BONDING KHI VỪA FLASH LẠI FIRMWARE:** Khi ESP32 vừa nạp lại firmware (flash làm sạch NVS), khóa bảo mật cũ bị xóa, nhưng điện thoại vẫn giữ khóa ghép đôi cũ. Khi đó điện thoại sẽ báo lỗi "Không thể ghép đôi". **Cách khắc phục:** Vào Cài đặt Bluetooth trên điện thoại > Bấm biểu tượng bánh răng ⚙️ cạnh `ESP32-SmartDash` > Nhấn **BỎ GHÉP ĐÔI (UNPAIR / FORGET)** rồi bấm ghép đôi lại là 100% thành công.
* **Quyền quét BLE trên Android 12+ (Loại bỏ `neverForLocation`):**
  * Tuyệt đối không để `android:usesPermissionFlags="neverForLocation"` trong `AndroidManifest.xml` nếu không dùng ScanFilter chặt chẽ, vì Android 12+ sẽ lọc bỏ kết quả quét. Thay vào đó, cấp quyền `ACCESS_FINE_LOCATION` cùng `BLUETOOTH_SCAN` để tìm thấy 100% thiết bị ngoại vi BLE.
* **Cổng Native USB CDC trên ESP32-S3:**
  * Bắt buộc có cờ `-DARDUINO_USB_CDC_ON_BOOT=1` và `-DARDUINO_USB_MODE=1` trong `platformio.ini` để luồng `Serial` Arduino xuất trực tiếp qua cổng USB máy tính.

---

## 7. KẾT NỐI VÀ ĐỒNG BỘ PIN ANT BMS QUA BLUETOOTH BLE KHÔNG DÂY

* **Nguyên lý kết nối không dây Pin ANT BMS:**
  * Pin ANT BMS có mạch Bluetooth riêng, thường phát sóng với tên `ANT-BMS`, `ANT_...`, `VB...`, `JK...`.
  * Trên Android App: Cung cấp nút **"🔍 TÌM KIẾM BLE PIN ANT"** mở hộp thoại quét sóng BLE xung quanh, hiển thị danh sách thiết bị kèm RSSI và địa chỉ MAC.
  * Khi người dùng chạm vào thiết bị: App kết nối GATT Service `0000ffe0...`, Characteristic `0000ffe1...`, kích hoạt Notify và tự động ghi nhớ địa chỉ MAC vào `SharedPreferences` để các lần sau tự động kết nối lại khi mở app.
* **Giải mã gói tin 140 Bytes ANT BMS:**
  * Header: `0xAA 0x55` (hoặc `0xAA 0x55 0xAA 0xFF`).
  * Checksum: Tổng từ byte index 4 đến 137 so với `(byte[138] << 8) | byte[139]`.
  * Trích xuất thông số:
    * Điện áp tổng Pack Pin: `((byte[4] << 8) | byte[5]) * 0.1f` (V)
    * Điện áp 32 cell pin: `((byte[6 + i*2] << 8) | byte[7 + i*2])` (mV)
    * Dòng điện xả/nạp: 4 bytes `byte[70..73]` (Signed Int32 * 0.1f Amps)
    * Phần trăm pin: `byte[74]` (% SoC)
    * Nhiệt độ: `byte[93] - 40` (°C), `byte[95] - 40` (°C)
    * Dung lượng còn lại: 4 bytes `byte[79..82] * 0.000001f` (Ah)
* **Đồng bộ thời gian thực sang ESP32 qua BLE:**
  * Khi App nhận dữ liệu pin từ ANT BMS, App tự động đóng gói chuỗi `BMS:vTot:curr:soc:t1:t2:deltaMv:minMv:maxMv` gửi sang ESP32 qua BLE NUS/JAMFOXRS.
  * ESP32 nhận chuỗi này và nạp thẳng vào snapshot hiển thị của màn hình OLED SSD1306 (Trang 3 BMS và Trang 1, 4).
  * Nhờ vậy, ngay cả khi người dùng không cắm dây UART2 vào ESP32, màn hình OLED trên xe vẫn hiển thị đầy đủ 100% điện áp, dòng xả, % pin và nhiệt độ pack pin từ ANT BMS!

---

## 8. HƯỚNG DẪN XỬ LÝ LỖI "ỨNG DỤNG BỊ CHẶN" VÀ "APK KHÔNG CÀI ĐƯỢC"

* **Lỗi 1: Google Play Protect báo "Ứng dụng bị chặn" (Blocked by Play Protect):**
  * **Nguyên nhân:** Do file APK cài trực tiếp (sideload) chưa xuất bản lên Google Play nên Play Protect cảnh báo ứng dụng từ nhà phát triển không xác định.
  * **Cách xử lý:** Khi pop-up cảnh báo hiện ra, chạm vào dòng chữ nhỏ **"Chi tiết khác" (More details)** > Chọn **"Vẫn cài đặt" (Install anyway)**.
* **Lỗi 2: Báo lỗi "Không thể cài đặt ứng dụng" (App not installed / Signature Mismatch):**
  * **Nguyên nhân:** Điện thoại đang cài một bản APK cũ có chữ ký (signature) khác với bản build mới.
  * **Cách xử lý:** Nhấn giữ biểu tượng ứng dụng **Smart EV Dashboard** cũ trên màn hình điện thoại > Chọn **Gỡ cài đặt (Uninstall)** > Sau đó mở file `SmartEV-Dashboard.apk` mới để cài đặt lại bình thường.

---

## 9. CẤU TRÚC 4 TRANG CÀI ĐẶT NÂNG CAO IC VOTOL TRÊN APP ANDROID (CHUẨN VOTOL-EM-V3 & VOTOLAIO)

* **Thiết kế phân trang điều hướng 3 Màn hình chính của App:**
  * **Màn 1 (Tab Cockpit):** Bảng đồng hồ công tơ mét kỹ thuật số (Tốc độ km/h, ODO/Trip, RPM, Công suất kW, Cấp số P/R/D/S, Xi-nhan, Đèn pha, Phanh, Chân chống, Bảng chẩn đoán UART Votol Live).
  * **Màn 2 (Tab BMS):** Pack Pin ANT BMS (Quét & Kết nối BLE Pin ANT, Áp Pack, Dòng xả/nạp, SoC%, Ah, 32 cell pin màu sắc, Delta mV, Min/Max cell, Nhiệt độ T1/T2).
  * **Màn 3 (Tab Settings):** Cài đặt nâng cao thông số IC Votol chia thành 4 Trang chuẩn giao diện phần mềm VOTOL-EM-V3 Debugging trên PC:
    * **PAGE 1: NGUỒN PIN & TAY GA (Basic & Throttle)**
      * *Basic Settings:* Model IC (EM-150/100/50), Điện áp quá áp Overvoltage (V), Cắt áp thấp Undervoltage (V), Cắt áp mềm Soft undervoltage (V), Độ lệch tụt áp Variation, Dòng xả bình Busbar current (A), Dòng pha Phase current (A).
      * *Throttle voltage set (max 5.5V):* Low protect (V), Start voltage (V), The end of the (V), High protect (V).
      * *Start setting:* Start torque, Combinative torque, Rate of rise, Rate of decline.
    * **PAGE 2: CẤP SỐ & CHẾ ĐỘ LÁI (Three-Speed & Sport Mode)**
      * *Sport mode setup:* Current-Limiting (A), Flux-Weakening mở tua, Automatic logout enablers, Logout time (S), Recovery time (S).
      * *Three-speed:* 3 Cấp số Low / Mid / High (% Tốc độ & % Dòng điện), Mid/High Flux-Weakening, Kiểu chuyển số (Nút bấm Button / Công tắc Switch), Cấp số mặc định khi mở khóa (Low/Mid/High), Khởi động êm Soft start & cấp độ.
      * *Hỗ trợ an toàn:* HHC (Khởi hành ngang dốc), HDC (Hỗ trợ đổ đèo), Giới hạn tốc độ xe Speed limit (%).
    * **PAGE 3: ĐỘNG CƠ & CẢM BIẾN (Motor Setting & Functions)**
      * *Motor Setting:* Số cặp cực Pole pairs (5 cho QS/Yuma), Đổi màu dây Hall Vàng-Xanh, Đổi màu dây pha Xanh-Lá, Kiểu động cơ Surface-mount / V-type, Góc lệch Hall shift Angle.
      * *An toàn & Phanh:* Giới hạn tốc độ lùi Reversing speed limit (%), Tỉ lệ phanh điện tử EBS ratio (%), Low brake, Secure boot.
      * *Output & Tiện ích:* Tín hiệu đồng hồ One-Lin / Hall, Trợ lực dắt xe Moving vehicle booster, Ga tự động Cruise control, Nhận diện áp kép Double-voltage.
    * **PAGE 4: CỔNG CHỨC NĂNG & THÔNG SỐ XE (Port Settings & Vehicle Display)**
      * *Port Settings:* Gán chức năng cổng I/O PD0, PB3, PA0, PB2, PC14, PA11... (Brake, Reverse, Park, Anti-theft, Cruise...).
      * *Vehicle Specs & Display:* Chu vi bánh xe Tire Circumference (mm), Tỉ số truyền Gear Ratio, Độ sáng màn hình OLED xe SSD1306 (10% - 100%).
    * **Bộ 3 nút thao tác:** `📥 ĐỌC IC (READ)`, `💾 LƯU XUỐNG IC (FLASH)`, `🔄 MẶC ĐỊNH (RESET)`.



