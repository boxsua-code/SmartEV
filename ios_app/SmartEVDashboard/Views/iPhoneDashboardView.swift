import SwiftUI

/// Modern Cyberpunk Digital Cockpit UI for iPhone display
struct iPhoneDashboardView: View {
    @ObservedObject var bleManager: ESP32BLEManager
    @State private var selectedTab = 0
    
    var body: some View {
        ZStack {
            // Background Dark Gradient
            LinearGradient(
                gradient: Gradient(colors: [Color(red: 0.05, green: 0.07, blue: 0.12), Color.black]),
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
            .ignoresSafeArea()
            
            VStack(spacing: 16) {
                // Top Header: Status Bar & Connection Indicator
                headerView
                
                ScrollView {
                    VStack(spacing: 20) {
                        // Main Speed Dial & Gear Badge
                        speedGaugeView
                        
                        // Signal Lights Indicator Bar (Turn Left, Beam, Turn Right, Stand, Brake)
                        signalsBarView
                        
                        // Battery SOC & Power Stats
                        batteryAndPowerView
                        
                        // Temperatures & Electrical Diagnostics Grid
                        telemetryMetricsGrid
                        
                        // Diagnostics & Fault Log Card
                        faultStatusCard
                        
                        // Test Navigation Simulation Buttons
                        navigationTestSection
                    }
                    .padding(.horizontal, 16)
                    .padding(.bottom, 24)
                }
            }
        }
    }
    
    // MARK: - Header View
    private var headerView: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text("SMART EV DASHBOARD")
                    .font(.system(size: 14, weight: .bold, design: .monospaced))
                    .foregroundColor(.cyan)
                Text("Apple CarPlay Companion")
                    .font(.system(size: 11, weight: .medium))
                    .foregroundColor(.gray)
            }
            Spacer()
            
            // BLE Connection Button & Status
            Button(action: {
                if bleManager.isConnected {
                    bleManager.disconnect()
                } else {
                    bleManager.startScanning()
                }
            }) {
                HStack(spacing: 6) {
                    Circle()
                        .fill(bleManager.isConnected ? Color.green : (bleManager.isScanning ? Color.yellow : Color.red))
                        .frame(width: 8, height: 8)
                    Text(bleManager.isConnected ? "ĐÃ KẾT NỐI" : (bleManager.isScanning ? "QUÉT..." : "KẾT NỐI BLE"))
                        .font(.system(size: 11, weight: .bold))
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 6)
                .background(Capsule().stroke(bleManager.isConnected ? Color.green : Color.cyan, lineWidth: 1))
            }
        }
        .padding(.horizontal, 16)
        .padding(.top, 8)
    }
    
    // MARK: - Speed Gauge View
    private var speedGaugeView: some View {
        ZStack {
            // Glassmorphism Card Container
            RoundedRectangle(cornerRadius: 24)
                .fill(Color.white.opacity(0.04))
                .overlay(RoundedRectangle(cornerRadius: 24).stroke(Color.cyan.opacity(0.2), lineWidth: 1))
            
            VStack(spacing: 4) {
                // Gear Badge (P, ECO, D, SPORT, R)
                Text(bleManager.telemetry.gear)
                    .font(.system(size: 22, weight: .heavy, design: .rounded))
                    .foregroundColor(gearColor(bleManager.telemetry.gear))
                    .padding(.horizontal, 16)
                    .padding(.vertical, 4)
                    .background(Capsule().fill(gearColor(bleManager.telemetry.gear).opacity(0.2)))
                    .padding(.top, 16)
                
                // Speed Number
                HStack(alignment: .firstTextBaseline, spacing: 4) {
                    Text("\(Int(bleManager.telemetry.spd))")
                        .font(.system(size: 72, weight: .bold, design: .rounded))
                        .foregroundColor(.white)
                    Text("km/h")
                        .font(.system(size: 18, weight: .semibold))
                        .foregroundColor(.cyan)
                }
                .padding(.vertical, -8)
                
                // Estimated Range
                Text("Tầm hoạt động dự kiến: ~\(bleManager.telemetry.estimatedRangeKm) km")
                    .font(.system(size: 12, weight: .medium))
                    .foregroundColor(.gray)
                    .padding(.bottom, 16)
            }
        }
        .frame(height: 170)
    }
    
    // MARK: - Vehicle Signal Bar View
    private var signalsBarView: some View {
        HStack(spacing: 12) {
            SignalIcon(name: "arrow.left.circle.fill", label: "Trái", isActive: bleManager.telemetry.turn_l == 1, activeColor: .green)
            SignalIcon(name: "light.beacon.max.fill", label: "Pha", isActive: bleManager.telemetry.beam == 1, activeColor: .blue)
            SignalIcon(name: "arrow.right.circle.fill", label: "Phải", isActive: bleManager.telemetry.turn_r == 1, activeColor: .green)
            SignalIcon(name: "exclamationmark.triangle.fill", label: "Phanh", isActive: bleManager.telemetry.brk == 1, activeColor: .red)
            SignalIcon(name: "minus.circle.fill", label: "Chân chống", isActive: bleManager.telemetry.stand == 1, activeColor: .orange)
        }
    }
    
    // MARK: - Battery & Power View
    private var batteryAndPowerView: some View {
        HStack(spacing: 12) {
            // Battery SOC Card
            MetricCard(
                title: "DUNG LƯỢNG PIN",
                value: "\(bleManager.telemetry.soc)%",
                subtitle: "\(String(format: "%.1f", bleManager.telemetry.v)) V",
                icon: "battery.100bolt",
                accentColor: bleManager.telemetry.soc > 20 ? .green : .red
            )
            
            // Power Output Card
            MetricCard(
                title: "CÔNG SUẤT",
                value: "\(Int(bleManager.telemetry.p)) W",
                subtitle: "\(String(format: "%.1f", bleManager.telemetry.a)) A",
                icon: "bolt.horizontal.circle.fill",
                accentColor: .yellow
            )
        }
    }
    
    // MARK: - Temperatures Grid
    private var telemetryMetricsGrid: some View {
        HStack(spacing: 12) {
            MetricCard(
                title: "NHIỆT ĐỘ IC",
                value: "\(bleManager.telemetry.temp_c)°C",
                subtitle: bleManager.telemetry.temp_c > 75 ? "⚠️ Cao" : "Bình thường",
                icon: "thermometer.medium",
                accentColor: bleManager.telemetry.temp_c > 75 ? .red : .cyan
            )
            
            MetricCard(
                title: "NHIỆT ĐỘ ĐỘNG CƠ",
                value: "\(bleManager.telemetry.temp_m)°C",
                subtitle: bleManager.telemetry.temp_m > 80 ? "⚠️ Quá nhiệt" : "Bình thường",
                icon: "thermometer.high",
                accentColor: bleManager.telemetry.temp_m > 80 ? .red : .orange
            )
        }
    }
    
    // MARK: - Fault Status Card
    private var faultStatusCard: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Image(systemName: bleManager.telemetry.err == 0 ? "checkmark.shield.fill" : "exclamationmark.shield.fill")
                    .foregroundColor(bleManager.telemetry.err == 0 ? .green : .red)
                Text("CHẨN ĐOÁN HỆ THỐNG VOTOL")
                    .font(.system(size: 12, weight: .bold, design: .monospaced))
                    .foregroundColor(.white)
                Spacer()
                Text("Hex: 0x\(String(bleManager.telemetry.err, radix: 16).uppercased())")
                    .font(.system(size: 11, weight: .medium, design: .monospaced))
                    .foregroundColor(.gray)
            }
            
            Text(bleManager.telemetry.faultSummary)
                .font(.system(size: 13, weight: .medium))
                .foregroundColor(bleManager.telemetry.err == 0 ? .green.opacity(0.9) : .red)
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 16).fill(Color.white.opacity(0.04)))
        .overlay(RoundedRectangle(cornerRadius: 16).stroke(bleManager.telemetry.err == 0 ? Color.green.opacity(0.3) : Color.red.opacity(0.5), lineWidth: 1))
    }
    
    // MARK: - Navigation Test Buttons
    private var navigationTestSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("MÔ PHỎNG DẪN ĐƯỜNG CARPLAY ➔ ESP32 OLED")
                .font(.system(size: 11, weight: .bold, design: .monospaced))
                .foregroundColor(.gray)
            
            HStack(spacing: 10) {
                Button("⬅ Rẽ Trái (200m)") {
                    bleManager.sendNavigationUpdate(icon: "LEFT", distance: "200m", instruction: "Nguyen Hue")
                }
                .buttonStyle(TestButtonStyle(color: .blue))
                
                Button("⬆ Đi Thẳng (1.5km)") {
                    bleManager.sendNavigationUpdate(icon: "STRAIGHT", distance: "1.5km", instruction: "Le Loi")
                }
                .buttonStyle(TestButtonStyle(color: .green))
                
                Button("❌ Xóa") {
                    bleManager.clearNavigation()
                }
                .buttonStyle(TestButtonStyle(color: .red))
            }
        }
    }
    
    private func gearColor(_ gear: String) -> Color {
        switch gear.uppercased() {
        case "SPORT", "SUPER": return .red
        case "D", "DRIVE": return .green
        case "ECO": return .cyan
        case "R", "REV": return .purple
        default: return .yellow
        }
    }
}

// MARK: - Subviews & Styles
struct SignalIcon: View {
    let name: String
    let label: String
    let isActive: Bool
    let activeColor: Color
    
    var body: some View {
        VStack(spacing: 4) {
            Image(systemName: name)
                .font(.system(size: 20))
                .foregroundColor(isActive ? activeColor : Color.gray.opacity(0.4))
            Text(label)
                .font(.system(size: 10, weight: .medium))
                .foregroundColor(isActive ? .white : .gray)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 8)
        .background(RoundedRectangle(cornerRadius: 12).fill(Color.white.opacity(isActive ? 0.1 : 0.03)))
    }
}

struct MetricCard: View {
    let title: String
    let value: String
    let subtitle: String
    let icon: String
    let accentColor: Color
    
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Image(systemName: icon)
                    .foregroundColor(accentColor)
                Text(title)
                    .font(.system(size: 10, weight: .bold, design: .monospaced))
                    .foregroundColor(.gray)
            }
            Text(value)
                .font(.system(size: 26, weight: .bold, design: .rounded))
                .foregroundColor(.white)
            Text(subtitle)
                .font(.system(size: 11, weight: .medium))
                .foregroundColor(accentColor)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 16).fill(Color.white.opacity(0.04)))
        .overlay(RoundedRectangle(cornerRadius: 16).stroke(accentColor.opacity(0.3), lineWidth: 1))
    }
}

struct TestButtonStyle: ButtonStyle {
    let color: Color
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 11, weight: .bold))
            .foregroundColor(.white)
            .padding(.horizontal, 10)
            .padding(.vertical, 8)
            .background(RoundedRectangle(cornerRadius: 10).fill(color.opacity(configuration.isPressed ? 0.4 : 0.2)))
            .overlay(RoundedRectangle(cornerRadius: 10).stroke(color, lineWidth: 1))
    }
}
