# HƯỚNG DẪN ỨNG DỤNG ANDROID KÉP: SMART EV DASHBOARD & ANDROID AUTO

Ứng dụng **Smart EV Dashboard** (`com.smartev.dashboard`) được thiết kế đặc biệt cho xe điện thông minh, kết nối trực tiếp với bo mạch **ESP32-S3** (IC điều tốc Votol EM150sp, BMS ANT, màn hình OLED SSD1306, RTC DS3231, Xi-nhan trái/phải, Đèn pha).

---

## 🌟 TÍNH NĂNG CỐT LÕI

### 1. Giao diện trực tiếp trên Điện thoại (Phone Cockpit & Tuning)
- **Tab 1: Digital Cockpit (Đồng hồ tốc độ kỹ thuật số)**:
  - Hiển thị vận tốc lớn (km/h) kèm cấp số xe (`P`, `R`, `N`, `D`, `1`, `2`, `3`, `S`).
  - Thanh đo dung lượng Pin SoC (%), Điện áp tổng Pack (V), Dòng xả tức thời (A), Công suất (kW).
  - Tín hiệu đèn Realtime: Xi-nhan Trái `[◀]`, Xi-nhan Phải `[▶]`, Đèn Pha `[D]` chớp nháy đồng bộ với xe.
  - Nhiệt độ động cơ (°C), Nhiệt độ IC Votol (°C).
  - Cảnh báo mã lỗi xe điện Votol (26 mã lỗi chi tiết).
- **Tab 2: Giám sát ANT BMS 32 Cell Pin**:
  - Lưới 4 cột hiển thị trực quan từ 16 đến 32 cell pin.
  - Tự động phát hiện và tô màu **ĐỎ** cell pin thấp nhất (`MIN`), màu **CYAN** cell cao nhất (`MAX`).
  - Hiển thị độ lệch điện áp cell (`Delta mV`) và 2 cảm biến nhiệt độ pin (`T1`, `T2`).
- **Tab 3: Bảng Cài đặt chuyên sâu xe điện (Lưu vào Flash NVS)**:
  - Cài đặt Đường kính ngoài lốp xe (mm) để chuẩn hóa tốc độ thực tế.
  - Cài đặt Số cặp cực từ động cơ (Motor Pole Pairs, vd: 4, 5).
  - Cài đặt Giới hạn dòng bình (Bus Current Limit - A).
  - Cài đặt Giới hạn dòng cực đại pha (Phase Current Limit - A).
  - Cài đặt Cấp độ phanh tái sinh (Regenerative Braking Level: Tắt, Nhẹ, Vừa, Mạnh).
  - Cài đặt Chế độ lái mặc định (Eco, Normal, Sport).
  - **Nút "LƯU CÀI ĐẶT VÀO XE"**: Gửi lệnh qua BLE xuống ESP32-S3 và ghi vĩnh viễn vào NVS Flash.
- **Tab 4: Trợ lý Dẫn đường & Kết nối Android Auto**:
  - Hướng dẫn kết nối xe và cấp quyền Notification Access để đọc Turn-by-Turn từ Google Maps và bài hát đang phát sang màn hình OLED trên ghi đông xe.

---

### 2. Giao diện chiếu lên Màn hình Android Auto (Car Display)
- Sử dụng chuẩn thư viện chính thức của Google: **`androidx.car.app:app:1.4.0`** (Android for Cars App Library).
- Khi bạn kết nối điện thoại với màn hình xe hơi / Head Unit Android Auto (qua cáp USB hoặc Wireless Android Auto):
  - Biểu tượng ứng dụng **Smart EV Dashboard** sẽ xuất hiện trên màn hình chính của Android Auto.
  - **Màn hình chính Car Dashboard (`EvDashboardScreen`)**: Hiển thị vận tốc lớn, cấp số, công suất tiêu thụ kW, % pin, đèn xi-nhan và đèn pha theo giao diện chuẩn lái xe an toàn của Google.
  - **Màn hình Chi tiết Pin Car BMS (`EvBmsScreen`)**: Cho phép tài xế theo dõi độ cân bằng pin (Delta mV) và nhiệt độ pack pin ngay trên màn cảm ứng lớn của ô tô.

---

## 🛠️ CẤU TRÚC MÃ NGUỒN (`android_app`)

```text
android_app/
├── app/
│   ├── src/main/
│   │   ├── AndroidManifest.xml              # Đầy đủ quyền BLE, NotificationListener & CarAppService
│   │   ├── java/com/smartev/dashboard/
│   │   │   ├── SmartEvApplication.kt        # Application Singleton
│   │   │   ├── MainActivity.kt              # Giao diện chính 4 Tab trên điện thoại
│   │   │   ├── model/
│   │   │   │   └── VehicleModels.kt         # Data model: Telemetry, BMS, Settings, Nav
│   │   │   ├── ble/
│   │   │   │   └── BleManager.kt            # Quản lý Bluetooth GATT NUS, tự động reconnect, parse JSON
│   │   │   ├── service/
│   │   │   │   └── MapNotificationService.kt# Bắt thông báo Google Maps & Media Player
│   │   │   ├── car/
│   │   │   │   ├── EvCarAppService.kt       # CarAppService cho Android Auto
│   │   │   │   ├── EvCarSession.kt          # Quản lý Session Android Auto
│   │   │   │   ├── EvDashboardScreen.kt     # Dashboard trên màn hình cảm ứng ô tô
│   │   │   │   └── EvBmsScreen.kt           # Màn hình BMS 32 cell trên Android Auto
│   │   │   └── ui/
│   │   │       └── BmsCellAdapter.kt        # Grid Adapter cho 32 cell pin
│   │   └── res/
│   │       ├── layout/
│   │       │   ├── activity_main.xml        # Giao diện Cockpit, BMS, Settings, Android Auto
│   │       │   └── item_bms_cell.xml        # Thẻ cell pin kèm thanh dung lượng
│   │       ├── menu/bottom_nav_menu.xml     # Menu 4 Tab
│   │       ├── xml/automotive_app_desc.xml  # Khai báo Automotive App Template
│   │       └── values/ (strings, colors, themes)
│   └── build.gradle                         # Config SDK 35, Car App 1.4.0, Gson
├── local.properties                         # sdk.dir trỏ tới C:\android-sdk
├── build.gradle
├── settings.gradle
└── gradle.properties
```

---

## 🚀 CÁCH CÀI ĐẶT & CHẠY ỨNG DỤNG

### Bước 1: Mở dự án trong Android Studio
1. Mở **Android Studio** trên máy tính: `C:\Program Files\Android\Android Studio`.
2. Chọn **Open** và duyệt đến thư mục:
   `E:\ESP32\ESP32_Android Auto\android_app`
3. Chờ Android Studio đồng bộ Gradle (Sync Project with Gradle Files).

### Bước 2: Cài đặt ứng dụng lên Điện thoại
1. Bật chế độ **Tùy chọn cho nhà phát triển (Developer Options)** và **Gỡ lỗi USB (USB Debugging)** trên điện thoại Android của bạn.
2. Cắm cáp USB nối điện thoại vào máy tính.
3. Trong Android Studio, chọn thiết bị của bạn ở thanh công cụ trên cùng và bấm nút **Run 'app'** (tam giác xanh ▶).
4. Cấp quyền Bluetooth và Quyền thông báo khi ứng dụng hỏi lần đầu tiên.

### Bước 3: Sử dụng trên Điện thoại & Xe hơi
1. **Trên điện thoại**:
   - Bấm **"KẾT NỐI XE"**: Ứng dụng sẽ tự động quét và kết nối với bo mạch `ESP32-SmartDash`.
   - Xem vận tốc, dung lượng pin, đèn pha, xi-nhan.
   - Chuyển sang Tab **"Cài đặt xe"** để chỉnh thông số IC Votol / Động cơ và bấm **"LƯU CÀI ĐẶT VÀO XE"**.
2. **Trên Màn hình Android Auto**:
   - Cắm điện thoại vào màn hình Android Auto trên xe (hoặc mở ứng dụng **Desktop Head Unit - DHU** để mô phỏng trên máy tính).
   - Chọn biểu tượng **Smart EV Dashboard** trên màn hình xe để trải nghiệm Digital Cockpit cỡ lớn!
