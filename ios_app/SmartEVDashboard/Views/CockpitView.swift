import SwiftUI

/// Tab 1: Digital Cockpit HUD phong cách R-Speedo EV Dashboard (Hỗ trợ chế độ Cockpit & Dragger đo gia tốc)
struct CockpitView: View {
    @ObservedObject var bleManager: ESP32BLEManager
    @State private var displayMode: Int = 0 // 0: EV Cockpit, 1: Dragger (Đo gia tốc)
    
    // Dragger Timer State
    @State private var dragReady: Bool = false
    @State private var dragStarted: Bool = false
    @State private var dragStartTime: Date? = nil
    @State private var time0_30: Double = 0.0
    @State private var time0_50: Double = 0.0
    @State private var time0_60: Double = 0.0
    @State private var time0_100: Double = 0.0
    @State private var peakPowerKw: Float = 0.0
    @State private var peakCurrentA: Float = 0.0
    @State private var isFinished: Bool = false
    
    var body: some View {
        ScrollView {
            VStack(spacing: 16) {
                // Header Connection Status & Mode Switcher
                headerStatusAndModeSwitcher
                
                if displayMode == 0 {
                    // --- CHẾ ĐỘ 1: R-SPEEDO EV COCKPIT ---
                    // Vòng cung tốc độ lớn Neon Arc Gauge
                    speedGaugeArcSection
                    
                    // Dải đèn tín hiệu xe (Xi nhan, Pha, Phanh, Chân chống, Regen)
                    signalsBarSection
                    
                    // Thẻ Nguồn Điện & Công Suất Tức Thời
                    electricalPowerSection
                    
                    // Thẻ Nhiệt Độ & Chẩn Đoán Lỗi Votol
                    temperaturesAndFaultSection
                    
                    // Thống kê Chuyến Đi (Trip & ODO)
                    tripStatsSection
                } else {
                    // --- CHẾ ĐỘ 2: R-SPEEDO DRAGGER PERFORMANCE (0-60 km/h, 0-100 km/h) ---
                    draggerPerformanceSection
                }
            }
            .padding(.horizontal, 16)
            .padding(.top, 6)
            .padding(.bottom, 36)
        }
        .onChange(of: bleManager.telemetry.spd) { newSpeed in
            handleDraggerTelemetry(speed: newSpeed, power: bleManager.telemetry.p, current: bleManager.telemetry.a)
        }
    }
    
    // MARK: - Header Status & Mode Switcher
    private var headerStatusAndModeSwitcher: some View {
        VStack(spacing: 10) {
            HStack {
                HStack(spacing: 8) {
                    Circle()
                        .fill(bleManager.isConnected ? Color.green : (bleManager.isScanning ? Color.yellow : Color.red))
                        .frame(width: 9, height: 9)
                        .shadow(color: bleManager.isConnected ? Color.green.opacity(0.8) : Color.clear, radius: 4)
                    
                    VStack(alignment: .leading, spacing: 2) {
                        Text(bleManager.isConnected ? bleManager.deviceName : "R-SPEEDO EV COCKPIT")
                            .font(.system(size: 13, weight: .black, design: .monospaced))
                            .foregroundColor(.white)
                        Text(bleManager.statusMessage)
                            .font(.system(size: 10, weight: .medium))
                            .foregroundColor(.gray)
                    }
                }
                
                Spacer()
                
                Button(action: {
                    if bleManager.isConnected {
                        bleManager.disconnect()
                    } else {
                        bleManager.startScanning()
                    }
                }) {
                    Text(bleManager.isConnected ? "NGẮT" : (bleManager.isScanning ? "QUÉT..." : "KẾT NỐI"))
                        .font(.system(size: 10, weight: .bold))
                        .foregroundColor(bleManager.isConnected ? .red : .cyan)
                        .padding(.horizontal, 10)
                        .padding(.vertical, 5)
                        .background(Capsule().stroke(bleManager.isConnected ? Color.red.opacity(0.6) : Color.cyan.opacity(0.8), lineWidth: 1))
                }
            }
            
            // Mode Picker: EV Cockpit vs Dragger
            Picker("Chế độ hiển thị", selection: $displayMode) {
                Text("🏎️ EV COCKPIT").tag(0)
                Text("⏱️ DRAGGER (GIA TỐC)").tag(1)
            }
            .pickerStyle(SegmentedPickerStyle())
        }
        .padding(12)
        .background(RoundedRectangle(cornerRadius: 16).fill(Color.white.opacity(0.04)))
    }
    
    // MARK: - Vòng Cung Tốc Độ Neon (Speed Arc Gauge)
    private var speedGaugeArcSection: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 24)
                .fill(Color.white.opacity(0.03))
                .overlay(RoundedRectangle(cornerRadius: 24).stroke(Color.cyan.opacity(0.18), lineWidth: 1))
            
            VStack(spacing: 2) {
                // Gear Indicator Badge (P, ECO, D, S, R)
                Text(bleManager.telemetry.gear)
                    .font(.system(size: 22, weight: .black, design: .rounded))
                    .foregroundColor(gearColor(bleManager.telemetry.gear))
                    .padding(.horizontal, 22)
                    .padding(.vertical, 3)
                    .background(Capsule().fill(gearColor(bleManager.telemetry.gear).opacity(0.18)))
                    .overlay(Capsule().stroke(gearColor(bleManager.telemetry.gear).opacity(0.5), lineWidth: 1))
                    .shadow(color: gearColor(bleManager.telemetry.gear).opacity(0.3), radius: 6)
                    .padding(.top, 14)
                
                // Speed Gauge Center
                ZStack {
                    // Gauge Background Arc
                    Circle()
                        .trim(from: 0.15, to: 0.85)
                        .stroke(Color.white.opacity(0.08), style: StrokeStyle(lineWidth: 12, lineCap: .round))
                        .rotationEffect(.degrees(90))
                        .frame(width: 170, height: 170)
                    
                    // Gauge Active Neon Arc (0 - 140 km/h)
                    let speedRatio = min(1.0, max(0.0, CGFloat(bleManager.telemetry.spd) / 120.0))
                    Circle()
                        .trim(from: 0.15, to: 0.15 + (0.70 * speedRatio))
                        .stroke(
                            LinearGradient(colors: [.cyan, .blue, .green, .yellow, .red], startPoint: .leading, endPoint: .trailing),
                            style: StrokeStyle(lineWidth: 12, lineCap: .round)
                        )
                        .rotationEffect(.degrees(90))
                        .frame(width: 170, height: 170)
                        .animation(.easeOut(duration: 0.2), value: bleManager.telemetry.spd)
                    
                    // Central Speed Digital Number
                    VStack(spacing: -6) {
                        Text("\(Int(bleManager.telemetry.spd))")
                            .font(.system(size: 64, weight: .black, design: .rounded))
                            .foregroundColor(.white)
                        Text("km/h")
                            .font(.system(size: 14, weight: .bold, design: .monospaced))
                            .foregroundColor(.cyan)
                    }
                }
                .padding(.vertical, -4)
                
                // Estimated Range & Power Subtitle
                HStack(spacing: 12) {
                    HStack(spacing: 4) {
                        Image(systemName: "road.lanes")
                        Text("Tầm đi: ~\(bleManager.telemetry.estimatedRangeKm) km")
                    }
                    .font(.system(size: 11, weight: .semibold))
                    .foregroundColor(.gray)
                    
                    Text("•").foregroundColor(.gray.opacity(0.5))
                    
                    HStack(spacing: 4) {
                        Image(systemName: "bolt.fill")
                        Text(String(format: "%.2f kW", bleManager.telemetry.p / 1000.0))
                    }
                    .font(.system(size: 11, weight: .semibold, design: .monospaced))
                    .foregroundColor(.yellow)
                }
                .padding(.bottom, 14)
            }
        }
        .frame(height: 255)
    }
    
    // MARK: - Dải Đèn Tín Hiệu Xe
    private var signalsBarSection: some View {
        HStack(spacing: 8) {
            SignalLightItem(name: "arrow.left.circle.fill", label: "Trái", isActive: bleManager.telemetry.turn_l == 1, activeColor: .green)
            SignalLightItem(name: "light.beacon.max.fill", label: "Pha", isActive: bleManager.telemetry.beam == 1, activeColor: .blue)
            SignalLightItem(name: "arrow.right.circle.fill", label: "Phải", isActive: bleManager.telemetry.turn_r == 1, activeColor: .green)
            SignalLightItem(name: "exclamationmark.triangle.fill", label: "Phanh", isActive: bleManager.telemetry.brk == 1, activeColor: .red)
            SignalLightItem(name: "minus.circle.fill", label: "Chống", isActive: bleManager.telemetry.stand == 1, activeColor: .orange)
            SignalLightItem(name: "bolt.badge.clock.fill", label: "Regen", isActive: bleManager.telemetry.rgn == 1, activeColor: .purple)
        }
    }
    
    // MARK: - Nguồn Điện & Công Suất
    private var electricalPowerSection: some View {
        HStack(spacing: 12) {
            CockpitMetricCard(
                title: "DUNG LƯỢNG PIN",
                value: "\(bleManager.telemetry.soc)%",
                subtitle: String(format: "%.1f V Pack", bleManager.telemetry.v),
                icon: "battery.100bolt",
                accentColor: bleManager.telemetry.soc > 20 ? .green : .red
            )
            
            CockpitMetricCard(
                title: "DÒNG ĐIỆN",
                value: String(format: "%.1f A", bleManager.telemetry.a),
                subtitle: String(format: "%.0f W Tức thời", bleManager.telemetry.p),
                icon: "bolt.horizontal.circle.fill",
                accentColor: .yellow
            )
        }
    }
    
    // MARK: - Nhiệt Độ & Chẩn Đoán Lỗi Votol
    private var temperaturesAndFaultSection: some View {
        VStack(spacing: 12) {
            HStack(spacing: 12) {
                CockpitMetricCard(
                    title: "NHIỆT ĐỘ IC",
                    value: "\(bleManager.telemetry.temp_c)°C",
                    subtitle: bleManager.telemetry.temp_c > 75 ? "⚠️ Nhiệt độ cao" : "Mát mẻ (OK)",
                    icon: "thermometer.medium",
                    accentColor: bleManager.telemetry.temp_c > 75 ? .red : .cyan
                )
                
                CockpitMetricCard(
                    title: "NHIỆT ĐỘ MOTOR",
                    value: "\(bleManager.telemetry.temp_m)°C",
                    subtitle: bleManager.telemetry.temp_m > 80 ? "⚠️ Quá nhiệt" : "Bình thường (OK)",
                    icon: "thermometer.high",
                    accentColor: bleManager.telemetry.temp_m > 80 ? .red : .orange
                )
            }
            
            // Fault Card
            VStack(alignment: .leading, spacing: 6) {
                HStack {
                    Image(systemName: bleManager.telemetry.err == 0 ? "checkmark.shield.fill" : "exclamationmark.shield.fill")
                        .foregroundColor(bleManager.telemetry.err == 0 ? .green : .red)
                    Text("CHẨN ĐOÁN VOTOL LIVE")
                        .font(.system(size: 11, weight: .bold, design: .monospaced))
                        .foregroundColor(.white)
                    Spacer()
                    Text("0x\(String(bleManager.telemetry.err, radix: 16).uppercased())")
                        .font(.system(size: 11, weight: .bold, design: .monospaced))
                        .foregroundColor(.gray)
                }
                
                Text(bleManager.telemetry.faultSummary)
                    .font(.system(size: 12, weight: .medium))
                    .foregroundColor(bleManager.telemetry.err == 0 ? .green.opacity(0.9) : .red)
            }
            .padding(12)
            .background(RoundedRectangle(cornerRadius: 14).fill(Color.white.opacity(0.04)))
            .overlay(RoundedRectangle(cornerRadius: 14).stroke(bleManager.telemetry.err == 0 ? Color.green.opacity(0.3) : Color.red.opacity(0.5), lineWidth: 1))
        }
    }
    
    // MARK: - Thống Kê Chuyến Đi
    private var tripStatsSection: some View {
        HStack(spacing: 12) {
            Button(action: {
                bleManager.syncPhoneTimeToESP32()
            }) {
                HStack(spacing: 6) {
                    Image(systemName: "clock.arrow.2.circlepath")
                    Text("ĐỒNG BỘ GIỜ RTC")
                }
                .font(.system(size: 11, weight: .bold))
                .foregroundColor(.cyan)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 10)
                .background(RoundedRectangle(cornerRadius: 12).fill(Color.cyan.opacity(0.1)))
                .overlay(RoundedRectangle(cornerRadius: 12).stroke(Color.cyan.opacity(0.4), lineWidth: 1))
            }
        }
    }
    
    // MARK: - CHẾ ĐỘ DRAGGER ĐO GIA TỐC (0-30, 0-50, 0-60, 0-100 KM/H)
    private var draggerPerformanceSection: some View {
        VStack(spacing: 16) {
            // Speed & State Display
            VStack(spacing: 6) {
                Text(dragStatusText)
                    .font(.system(size: 13, weight: .black, design: .monospaced))
                    .foregroundColor(dragStatusColor)
                    .padding(.horizontal, 14)
                    .padding(.vertical, 4)
                    .background(Capsule().fill(dragStatusColor.opacity(0.18)))
                    .padding(.top, 12)
                
                HStack(alignment: .firstTextBaseline, spacing: 4) {
                    Text("\(Int(bleManager.telemetry.spd))")
                        .font(.system(size: 72, weight: .black, design: .rounded))
                        .foregroundColor(.white)
                    Text("km/h")
                        .font(.system(size: 16, weight: .bold, design: .monospaced))
                        .foregroundColor(.cyan)
                }
                .padding(.vertical, -8)
                
                // Peak Telemetry during Drag
                HStack(spacing: 16) {
                    Text("Công suất đỉnh: \(String(format: "%.2f", peakPowerKw)) kW")
                        .font(.system(size: 11, weight: .bold, design: .monospaced))
                        .foregroundColor(.yellow)
                    Text("Dòng xả đỉnh: \(String(format: "%.1f", peakCurrentA)) A")
                        .font(.system(size: 11, weight: .bold, design: .monospaced))
                        .foregroundColor(.orange)
                }
                .padding(.bottom, 12)
            }
            .frame(maxWidth: .infinity)
            .background(RoundedRectangle(cornerRadius: 20).fill(Color.white.opacity(0.04)))
            .overlay(RoundedRectangle(cornerRadius: 20).stroke(dragStatusColor.opacity(0.4), lineWidth: 1.5))
            
            // Split Timers Card
            VStack(spacing: 10) {
                DraggerSplitRow(label: "0 - 30 km/h", time: time0_30, isAchieved: time0_30 > 0)
                Divider().background(Color.gray.opacity(0.3))
                DraggerSplitRow(label: "0 - 50 km/h", time: time0_50, isAchieved: time0_50 > 0)
                Divider().background(Color.gray.opacity(0.3))
                DraggerSplitRow(label: "0 - 60 km/h (Chuẩn EV)", time: time0_60, isAchieved: time0_60 > 0)
                Divider().background(Color.gray.opacity(0.3))
                DraggerSplitRow(label: "0 - 100 km/h (Mở tua)", time: time0_100, isAchieved: time0_100 > 0)
            }
            .padding(14)
            .background(RoundedRectangle(cornerRadius: 16).fill(Color.white.opacity(0.04)))
            
            // Arm / Reset Action Buttons
            HStack(spacing: 12) {
                Button(action: {
                    resetDragger()
                    dragReady = true
                }) {
                    HStack {
                        Image(systemName: "flag.checkered")
                        Text(dragReady ? "SẴN SÀNG (DỪNG XE 0 KM/H ĐỂ BẮT ĐẦU)" : "CHUẨN BỊ XUẤT PHÁT")
                    }
                    .font(.system(size: 12, weight: .black))
                    .foregroundColor(.white)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 12)
                    .background(RoundedRectangle(cornerRadius: 12).fill(dragReady ? Color.green : Color.blue))
                }
                
                Button(action: {
                    resetDragger()
                }) {
                    Image(systemName: "arrow.counterclockwise")
                        .font(.system(size: 14, weight: .bold))
                        .foregroundColor(.orange)
                        .padding(12)
                        .background(RoundedRectangle(cornerRadius: 12).fill(Color.orange.opacity(0.15)))
                        .overlay(RoundedRectangle(cornerRadius: 12).stroke(Color.orange.opacity(0.5), lineWidth: 1))
                }
            }
        }
    }
    
    private var dragStatusText: String {
        if isFinished { return "🏁 ĐÃ HOÀN THÀNH LƯỢT ĐO!" }
        if dragStarted { return "⚡ ĐANG ĐO GIA TỐC (FULL GA)..." }
        if dragReady { return "🟢 SẴN SÀNG - VẶN GA LÀ TÍNH GIỜ" }
        return "⚪ NHẤN 'CHUẨN BỊ XUẤT PHÁT' ĐỂ ĐO"
    }
    
    private var dragStatusColor: Color {
        if isFinished { return .cyan }
        if dragStarted { return .red }
        if dragReady { return .green }
        return .gray
    }
    
    private func resetDragger() {
        dragReady = false
        dragStarted = false
        dragStartTime = nil
        time0_30 = 0.0
        time0_50 = 0.0
        time0_60 = 0.0
        time0_100 = 0.0
        peakPowerKw = 0.0
        peakCurrentA = 0.0
        isFinished = false
    }
    
    private func handleDraggerTelemetry(speed: Float, power: Float, current: Float) {
        guard dragReady || dragStarted else { return }
        
        let pKw = power / 1000.0
        if pKw > peakPowerKw { peakPowerKw = pKw }
        if current > peakCurrentA { peakCurrentA = current }
        
        // Tự động kích hoạt khi vận tốc > 1 km/h
        if dragReady && !dragStarted && speed >= 1.0 {
            dragStarted = true
            dragReady = false
            dragStartTime = Date()
        }
        
        if dragStarted, let start = dragStartTime {
            let elapsed = Date().timeIntervalSince(start)
            
            if speed >= 30.0 && time0_30 == 0 {
                time0_30 = elapsed
            }
            if speed >= 50.0 && time0_50 == 0 {
                time0_50 = elapsed
            }
            if speed >= 60.0 && time0_60 == 0 {
                time0_60 = elapsed
            }
            if speed >= 100.0 && time0_100 == 0 {
                time0_100 = elapsed
                isFinished = true
                dragStarted = false
            }
        }
    }
    
    private func gearColor(_ gear: String) -> Color {
        switch gear.uppercased() {
        case "SPORT", "SUPER", "S": return .red
        case "D", "DRIVE": return .green
        case "ECO", "LOW": return .cyan
        case "R", "REV": return .purple
        default: return .yellow
        }
    }
}

// MARK: - Dragger Split Row
struct DraggerSplitRow: View {
    let label: String
    let time: Double
    let isAchieved: Bool
    
    var body: some View {
        HStack {
            Text(label)
                .font(.system(size: 13, weight: .bold, design: .monospaced))
                .foregroundColor(.white)
            Spacer()
            if isAchieved {
                Text(String(format: "%.2f s", time))
                    .font(.system(size: 16, weight: .black, design: .monospaced))
                    .foregroundColor(.green)
            } else {
                Text("--.-- s")
                    .font(.system(size: 14, weight: .bold, design: .monospaced))
                    .foregroundColor(.gray.opacity(0.5))
            }
        }
    }
}
