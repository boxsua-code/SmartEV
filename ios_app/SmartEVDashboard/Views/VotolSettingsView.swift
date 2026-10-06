import SwiftUI

/// Tab 3: Cài đặt nâng cao thông số IC VOTOL EM Series (4 Trang chuẩn phần mềm VOTOL-EM-V3 PC)
struct VotolSettingsView: View {
    @ObservedObject var bleManager: ESP32BLEManager
    @State private var settings = VotolSettings()
    @State private var selectedPage = 0
    @State private var showSuccessAlert = false
    @State private var alertMessage = ""
    
    private let pageTitles = ["1. PIN & GA", "2. CẤP SỐ", "3. MOTOR", "4. CỔNG & XE"]
    
    var body: some View {
        VStack(spacing: 0) {
            // Action Bar: Đọc IC / Nạp IC / Mặc định
            actionBarHeader
                .padding(.horizontal, 16)
                .padding(.top, 8)
                .padding(.bottom, 12)
            
            // Segmented Tab Picker 4 Trang
            Picker("Trang Cài Đặt", selection: $selectedPage) {
                ForEach(0..<pageTitles.count, id: \.self) { idx in
                    Text(pageTitles[idx]).tag(idx)
                }
            }
            .pickerStyle(SegmentedPickerStyle())
            .padding(.horizontal, 16)
            .padding(.bottom, 12)
            
            // Nội dung theo từng Trang
            ScrollView {
                VStack(spacing: 16) {
                    switch selectedPage {
                    case 0:
                        page1BasicAndThrottle
                    case 1:
                        page2SpeedAndSport
                    case 2:
                        page3MotorAndFunctions
                    case 3:
                        page4PortsAndDisplay
                    default:
                        EmptyView()
                    }
                }
                .padding(.horizontal, 16)
                .padding(.bottom, 32)
            }
        }
        .alert(isPresented: $showSuccessAlert) {
            Alert(title: Text("Thông Báo"), message: Text(alertMessage), dismissButton: .default(Text("OK")))
        }
    }
    
    // MARK: - Action Bar Header
    private var actionBarHeader: some View {
        HStack(spacing: 8) {
            Button(action: {
                bleManager.requestReadVotolSettings()
                alertMessage = "Đã gửi yêu cầu đọc thông số từ IC Votol qua BLE!"
                showSuccessAlert = true
            }) {
                HStack(spacing: 4) {
                    Image(systemName: "arrow.down.doc.fill")
                    Text("ĐỌC IC")
                }
                .font(.system(size: 11, weight: .bold))
                .foregroundColor(.cyan)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 8)
                .background(RoundedRectangle(cornerRadius: 10).fill(Color.cyan.opacity(0.12)))
                .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.cyan.opacity(0.4), lineWidth: 1))
            }
            
            Button(action: {
                bleManager.sendVotolSettings(settings)
                alertMessage = "Đã nạp toàn bộ thông số cấu hình xuống IC Votol thành công!"
                showSuccessAlert = true
            }) {
                HStack(spacing: 4) {
                    Image(systemName: "bolt.badge.checkmark.fill")
                    Text("LƯU XUỐNG IC")
                }
                .font(.system(size: 11, weight: .bold))
                .foregroundColor(.green)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 8)
                .background(RoundedRectangle(cornerRadius: 10).fill(Color.green.opacity(0.15)))
                .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.green.opacity(0.5), lineWidth: 1))
            }
            
            Button(action: {
                settings = VotolSettings()
                bleManager.requestResetVotolSettings()
                alertMessage = "Đã khôi phục thông số mặc định của IC Votol."
                showSuccessAlert = true
            }) {
                HStack(spacing: 4) {
                    Image(systemName: "arrow.counterclockwise")
                    Text("MẶC ĐỊNH")
                }
                .font(.system(size: 11, weight: .bold))
                .foregroundColor(.orange)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 8)
                .background(RoundedRectangle(cornerRadius: 10).fill(Color.orange.opacity(0.12)))
                .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color.orange.opacity(0.4), lineWidth: 1))
            }
        }
    }
    
    // MARK: - PAGE 1: NGUỒN PIN & TAY GA
    private var page1BasicAndThrottle: some View {
        VStack(spacing: 16) {
            SettingsCard(title: "THIẾT LẬP NGUỒN PIN & DÒNG ĐIỆN", icon: "bolt.fill") {
                VStack(spacing: 12) {
                    SettingNumberRow(title: "Điện áp quá áp (Overvoltage)", unit: "V", value: $settings.overvoltage)
                    SettingNumberRow(title: "Cắt áp thấp (Undervoltage)", unit: "V", value: $settings.undervoltage)
                    SettingNumberRow(title: "Cắt áp mềm (Soft Undervolt)", unit: "V", value: $settings.softUndervoltage)
                    SettingNumberRow(title: "Độ lệch tụt áp (Variation)", unit: "V", value: $settings.variation)
                    SettingIntRow(title: "Dòng xả bình (Busbar Current)", unit: "A", value: $settings.busbarCurrent)
                    SettingIntRow(title: "Dòng pha (Phase Current)", unit: "A", value: $settings.phaseCurrent)
                }
            }
            
            SettingsCard(title: "ĐIỆN ÁP TAY GA (THROTTLE SETUP)", icon: "speedometer") {
                VStack(spacing: 12) {
                    SettingNumberRow(title: "Low Protect", unit: "V", value: $settings.throttleLowProtect)
                    SettingNumberRow(title: "Start Voltage", unit: "V", value: $settings.throttleStart)
                    SettingNumberRow(title: "The End of (Full ga)", unit: "V", value: $settings.throttleEnd)
                    SettingNumberRow(title: "High Protect", unit: "V", value: $settings.throttleHighProtect)
                }
            }
            
            SettingsCard(title: "ĐỘ NHẠY KHỞI ĐỘNG (START SETTING)", icon: "gauge") {
                VStack(spacing: 12) {
                    SettingIntRow(title: "Start Torque (Mô-men đầu)", unit: "%", value: $settings.startTorque)
                    SettingIntRow(title: "Combinative Torque", unit: "%", value: $settings.combinativeTorque)
                    SettingIntRow(title: "Tốc độ tăng ga (Rate of Rise)", unit: "", value: $settings.rateOfRise)
                    SettingIntRow(title: "Tốc độ giảm ga (Rate of Decline)", unit: "", value: $settings.rateOfDecline)
                }
            }
        }
    }
    
    // MARK: - PAGE 2: CẤP SỐ & CHẾ ĐỘ LÁI
    private var page2SpeedAndSport: some View {
        VStack(spacing: 16) {
            SettingsCard(title: "CHẾ ĐỘ SPORT BOOST (SPORT SETUP)", icon: "flame.fill") {
                VStack(spacing: 12) {
                    SettingIntRow(title: "Giới hạn dòng Sport (Current Limit)", unit: "A", value: $settings.sportCurrentLimit)
                    SettingIntRow(title: "Flux-Weakening (Mở tua)", unit: "A", value: $settings.sportFluxWeakening)
                    SettingIntRow(title: "Thời gian duy trì Sport", unit: "Giây", value: $settings.sportLogoutTime)
                    SettingIntRow(title: "Thời gian hồi Sport", unit: "Giây", value: $settings.sportRecoveryTime)
                }
            }
            
            SettingsCard(title: "3 CẤP SỐ (THREE-SPEED SETTING)", icon: "square.grid.3x1.below.line.grid.1x2") {
                VStack(spacing: 12) {
                    SettingIntRow(title: "Số 1 - Tốc độ (Low Speed)", unit: "%", value: $settings.speedLowRatio)
                    SettingIntRow(title: "Số 1 - Dòng điện (Low Current)", unit: "%", value: $settings.currentLowRatio)
                    Divider().background(Color.gray.opacity(0.3))
                    SettingIntRow(title: "Số 2 - Tốc độ (Mid Speed)", unit: "%", value: $settings.speedMidRatio)
                    SettingIntRow(title: "Số 2 - Dòng điện (Mid Current)", unit: "%", value: $settings.currentMidRatio)
                    Divider().background(Color.gray.opacity(0.3))
                    SettingIntRow(title: "Số 3 - Tốc độ (High Speed)", unit: "%", value: $settings.speedHighRatio)
                    SettingIntRow(title: "Số 3 - Dòng điện (High Current)", unit: "%", value: $settings.currentHighRatio)
                    Divider().background(Color.gray.opacity(0.3))
                    SettingIntRow(title: "Mid Flux-Weakening", unit: "A", value: $settings.midFluxWeakening)
                    SettingIntRow(title: "High Flux-Weakening", unit: "A", value: $settings.highFluxWeakening)
                }
            }
            
            SettingsCard(title: "TÙY CHỈNH HỖ TRỢ VẬN HÀNH", icon: "shield.checkered") {
                VStack(spacing: 12) {
                    SettingToggleRow(title: "Khởi động êm (Soft Start)", isOn: $settings.softStartEnable)
                    if settings.softStartEnable {
                        SettingIntRow(title: "Cấp độ Soft Start (1-5)", unit: "Cấp", value: $settings.softStartLevel)
                    }
                    SettingToggleRow(title: "Khởi hành ngang dốc (HHC)", isOn: $settings.hhcEnable)
                    SettingToggleRow(title: "Hỗ trợ đổ đèo (HDC)", isOn: $settings.hdcEnable)
                    SettingIntRow(title: "Giới hạn tốc độ tổng (Speed Limit)", unit: "%", value: $settings.overallSpeedLimit)
                }
            }
        }
    }
    
    // MARK: - PAGE 3: ĐỘNG CƠ & CẢM BIẾN
    private var page3MotorAndFunctions: some View {
        VStack(spacing: 16) {
            SettingsCard(title: "THÔNG SỐ ĐỘNG CƠ (MOTOR SETTING)", icon: "gearshape.2.fill") {
                VStack(spacing: 12) {
                    SettingIntRow(title: "Số cặp cực (Pole Pairs)", unit: "Cực", value: $settings.polePairs)
                    SettingToggleRow(title: "Đổi màu dây Hall Vàng-Xanh", isOn: $settings.hallYellowGreenSwap)
                    SettingToggleRow(title: "Đổi màu dây pha Xanh-Lá", isOn: $settings.phaseBlueGreenSwap)
                    SettingIntRow(title: "Góc lệch Hall (Shift Angle)", unit: "°", value: $settings.hallShiftAngle)
                }
            }
            
            SettingsCard(title: "AN TOÀN & PHANH TÁI TẠO", icon: "hand.raised.fill") {
                VStack(spacing: 12) {
                    SettingIntRow(title: "Giới hạn tốc độ lùi (Reverse)", unit: "%", value: $settings.reverseSpeedLimit)
                    SettingIntRow(title: "Tỉ lệ phanh điện tử (EBS Ratio)", unit: "%", value: $settings.ebsRatio)
                    SettingToggleRow(title: "Ngắt ga phanh thấp (Low Brake)", isOn: $settings.lowBrakeEnable)
                    SettingToggleRow(title: "Khởi động an toàn (Secure Boot)", isOn: $settings.secureBootEnable)
                }
            }
            
            SettingsCard(title: "TÍN HIỆU ĐỒNG HỒ & TIỆN ÍCH", icon: "display") {
                VStack(spacing: 12) {
                    SettingToggleRow(title: "Trợ lực dắt xe (Moving Booster)", isOn: $settings.movingBoosterEnable)
                    SettingToggleRow(title: "Ga tự động (Cruise Control)", isOn: $settings.cruiseControlEnable)
                    SettingToggleRow(title: "Nhận diện áp kép (Double-Volt)", isOn: $settings.doubleVoltageEnable)
                }
            }
        }
    }
    
    // MARK: - PAGE 4: CỔNG CHỨC NĂNG & XE
    private var page4PortsAndDisplay: some View {
        VStack(spacing: 16) {
            SettingsCard(title: "GÁN CHỨC NĂNG CỔNG I/O IC", icon: "cable.connector") {
                VStack(spacing: 12) {
                    SettingTextRow(title: "Cổng PD0 (Chân 12)", text: $settings.portPD0)
                    SettingTextRow(title: "Cổng PB3 (Chân 13)", text: $settings.portPB3)
                    SettingTextRow(title: "Cổng PA0 (Chân 14)", text: $settings.portPA0)
                    SettingTextRow(title: "Cổng PB2 (Chân 15)", text: $settings.portPB2)
                    SettingTextRow(title: "Cổng PC14 (Chân 16)", text: $settings.portPC14)
                }
            }
            
            SettingsCard(title: "THÔNG SỐ BÁNH XE & MÀN HÌNH OLED", icon: "car.side.fill") {
                VStack(spacing: 12) {
                    SettingIntRow(title: "Chu vi bánh xe (Tire Circumference)", unit: "mm", value: $settings.tireCircumferenceMm)
                    SettingNumberRow(title: "Tỉ số truyền (Gear Ratio)", unit: "", value: $settings.gearRatio)
                    SettingIntRow(title: "Độ sáng màn hình OLED xe SSD1306", unit: "%", value: $settings.oledBrightness)
                }
            }
        }
    }
}

// MARK: - Reusable Settings Components
struct SettingsCard<Content: View>: View {
    let title: String
    let icon: String
    let content: Content
    
    init(title: String, icon: String, @ViewBuilder content: () -> Content) {
        self.title = title
        self.icon = icon
        self.content = content()
    }
    
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 6) {
                Image(systemName: icon)
                    .foregroundColor(.cyan)
                    .font(.system(size: 13))
                Text(title)
                    .font(.system(size: 11, weight: .bold, design: .monospaced))
                    .foregroundColor(.white)
            }
            
            Divider().background(Color.gray.opacity(0.3))
            
            content
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 16).fill(Color.white.opacity(0.04)))
        .overlay(RoundedRectangle(cornerRadius: 16).stroke(Color.white.opacity(0.08), lineWidth: 1))
    }
}

struct SettingNumberRow: View {
    let title: String
    let unit: String
    @Binding var value: Float
    
    var body: some View {
        HStack {
            Text(title)
                .font(.system(size: 12, weight: .medium))
                .foregroundColor(.gray)
            Spacer()
            HStack(spacing: 4) {
                TextField("", text: Binding(
                    get: { String(format: "%.1f", value) },
                    set: { if let v = Float($0) { value = v } }
                ))
                .keyboardType(.decimalPad)
                .font(.system(size: 13, weight: .bold, design: .monospaced))
                .foregroundColor(.cyan)
                .multilineTextAlignment(.trailing)
                .frame(width: 60)
                if !unit.isEmpty {
                    Text(unit)
                        .font(.system(size: 11, weight: .semibold))
                        .foregroundColor(.gray)
                }
            }
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
            .background(RoundedRectangle(cornerRadius: 8).fill(Color.white.opacity(0.06)))
        }
    }
}

struct SettingIntRow: View {
    let title: String
    let unit: String
    @Binding var value: Int
    
    var body: some View {
        HStack {
            Text(title)
                .font(.system(size: 12, weight: .medium))
                .foregroundColor(.gray)
            Spacer()
            HStack(spacing: 4) {
                TextField("", text: Binding(
                    get: { String(value) },
                    set: { if let v = Int($0) { value = v } }
                ))
                .keyboardType(.numberPad)
                .font(.system(size: 13, weight: .bold, design: .monospaced))
                .foregroundColor(.cyan)
                .multilineTextAlignment(.trailing)
                .frame(width: 50)
                if !unit.isEmpty {
                    Text(unit)
                        .font(.system(size: 11, weight: .semibold))
                        .foregroundColor(.gray)
                }
            }
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
            .background(RoundedRectangle(cornerRadius: 8).fill(Color.white.opacity(0.06)))
        }
    }
}

struct SettingToggleRow: View {
    let title: String
    @Binding var isOn: Bool
    
    var body: some View {
        HStack {
            Text(title)
                .font(.system(size: 12, weight: .medium))
                .foregroundColor(.gray)
            Spacer()
            Toggle("", isOn: $isOn)
                .labelsHidden()
                .toggleStyle(SwitchToggleStyle(tint: .cyan))
        }
    }
}

struct SettingTextRow: View {
    let title: String
    @Binding var text: String
    
    var body: some View {
        HStack {
            Text(title)
                .font(.system(size: 12, weight: .medium))
                .foregroundColor(.gray)
            Spacer()
            TextField("", text: $text)
                .font(.system(size: 12, weight: .semibold, design: .monospaced))
                .foregroundColor(.cyan)
                .multilineTextAlignment(.trailing)
                .frame(maxWidth: 150)
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background(RoundedRectangle(cornerRadius: 8).fill(Color.white.opacity(0.06)))
        }
    }
}
