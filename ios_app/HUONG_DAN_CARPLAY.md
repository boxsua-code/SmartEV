# HƯỚNG DẪN CÀI ĐẶT VÀ CHẠY ỨNG DỤNG APPLE CARPLAY CHO IPHONE (SMART EV DASHBOARD)

Tài liệu này hướng dẫn chi tiết cách biên dịch, cài đặt và chạy ứng dụng **Smart EV Dashboard** trên iPhone và chiếu trực tiếp màn hình Digital Cockpit lên màn hình ô tô / xe điện hỗ trợ **Apple CarPlay**.

---

## 1. CẤU TRÚC DỰ ÁN IPHONE / CARPLAY

Thư mục dự án: `ios_app/SmartEVDashboard/`
- `App/SmartEVDashboardApp.swift`: Điểm khởi chạy chính ứng dụng SwiftUI.
- `App/CarPlaySceneDelegate.swift`: Xử lý kết nối và chiếu màn hình lên hệ thống Apple CarPlay (`CPTemplateApplicationSceneDelegate`).
- `BLE/ESP32BLEManager.swift`: Quản lý kết nối Bluetooth BLE (`CoreBluetooth`) kết nối tới ESP32-S3 theo UUID:
  - Service UUID: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
  - TX Characteristic (Telemetry từ ESP32): `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`
  - RX Characteristic (Lệnh/Chỉ đường tới ESP32): `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
- `CarPlay/CarPlayManager.swift`: Xây dựng giao diện CarPlay dạng `CPGridTemplate` hiển thị trực quan Tốc độ (km/h), Dung lượng Pin (%), Công suất (W), Điện áp (V), Nhiệt độ IC / Động cơ (°C), Cấp số (P/Eco/D/Sport/R).
- `Views/iPhoneDashboardView.swift`: Giao diện Cockpit điện tử hiện đại dành cho màn hình iPhone khi chưa cắm vào CarPlay.
- `Models/VehicleTelemetry.swift`: Model dữ liệu nhận từ ESP32 theo định dạng JSON.

---

## 2. YÊU CẦU MÔI TRƯỜNG BIÊN DỊCH

1. Máy tính **macOS** (hoặc macOS Virtual Machine / Hackintosh) cài sẵn **Xcode 14.0 trở lên**.
2. Điện thoại **iPhone (iOS 15.0 trở lên)** hoặc **Xcode Simulator**.
3. Cáp Lightning / USB-C kết nối với iPhone hoặc xe có hỗ trợ Apple CarPlay (có dây hoặc không dây CarPlay).

---

## 3. CÁC BƯỚC MỞ VÀ BUILD DỰ ÁN TRÊN XCODE

### Bước 1: Mở dự án trong Xcode
1. Mở ứng dụng **Xcode**.
2. Chọn **Open a project or file** > Tìm đến thư mục `e:\ESP32\ESP32_Android Auto\ios_app\SmartEVDashboard`.
3. Hoặc tạo mới một Xcode iOS App Project đặt tên `SmartEVDashboard` và kéo toàn bộ các file trong `SmartEVDashboard/` vào Xcode.

### Bước 2: Cấu hình Signing & Capabilities
1. Chọn target **SmartEVDashboard** > thẻ **Signing & Capabilities**.
2. Chọn **Team** cá nhân của bạn (Personal Team Apple ID miễn phí).
3. Nhấn nút `+ Capability` > Thêm **Background Modes** > Tích chọn:
   - `Uses Bluetooth LE accessories`
   - `External accessory communication`
4. Thêm `Entitlements.plist` với cờ `com.apple.developer.carplay-app = true`.

---

## 4. CHẠY VÀ KIỂM THỬ TRÊN XCODE SIMULATOR (CÓ CARPLAY)

1. Trên Xcode, chọn thiết bị giả lập: **iPhone 15 Pro** (hoặc thiết bị iOS bất kỳ).
2. Nhấn nút **Run (Cmd + R)** để biên dịch ứng dụng.
3. Khi Simulator iPhone khởi chạy:
   - Vào menu Xcode: **I/O** > **External Displays** > Chọn **CarPlay** (hoặc **CarPlay (1080p)**).
   - Một cửa sổ màn hình ô tô Apple CarPlay ảo sẽ xuất hiện bên cạnh iPhone.
4. Mở icon **Smart EV Dashboard** trên màn hình CarPlaySimulator để xem giao diện Cockpit ô tô hoạt động đồng bộ với iPhone!

---

## 5. TÍNH NĂNG TỰ ĐỘNG ĐỒNG BỘ BLE VỚI ESP32-S3

- Khi iPhone ở gần xe, ứng dụng sẽ **tự động quét và kết nối với ESP32-SmartDash**.
- Tự động đồng bộ thời gian từ iPhone sang đồng hồ RTC DS3231 của ESP32 qua lệnh `TIME:YYYY:MM:DD:HH:mm:ss`.
- Telemetry điện áp, tốc độ, nhiệt độ, lỗi Votol từ IC được ESP32 gửi lên iPhone **mỗi 1 giây** và cập nhật tức thì lên cả màn hình iPhone và màn hình Apple CarPlay trên xe điện!
