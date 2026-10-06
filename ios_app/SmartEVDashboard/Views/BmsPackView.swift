import SwiftUI

/// Tab 2: Quản lý & Giám sát chi tiết Pack Pin ANT BMS qua Bluetooth BLE
struct BmsPackView: View {
    @ObservedObject var bmsManager: AntBmsBleManager
    @State private var showScannerSheet: Bool = false
    
    // Layout 2 cột cho các cell pin
    private let cellColumns = [
        GridItem(.flexible(), spacing: 10),
        GridItem(.flexible(), spacing: 10)
    ]
    
    var body: some View {
        ScrollView {
            VStack(spacing: 16) {
                // Header Kết nối Pin
                bmsConnectionHeader
                
                // Thẻ tổng quan Pack Pin (Điện áp, Dòng xả, Dung lượng SoC, Ah)
                bmsOverviewCards
                
                // Thẻ sức khỏe cân bằng cell (Delta mV, Min Cell, Max Cell)
                cellBalanceHealthCard
                
                // Danh sách điện áp từng cell pin (1 -> 32 cells)
                cellVoltagesSection
            }
            .padding(.horizontal, 16)
            .padding(.top, 8)
            .padding(.bottom, 32)
        }
        .sheet(isPresented: $showScannerSheet) {
            AntBmsScannerSheet(bmsManager: bmsManager, isPresented: $showScannerSheet)
        }
    }
    
    // MARK: - Header Kết nối
    private var bmsConnectionHeader: some View {
        HStack {
            HStack(spacing: 8) {
                Circle()
                    .fill(bmsManager.isConnected ? Color.green : Color.red)
                    .frame(width: 10, height: 10)
                
                VStack(alignment: .leading, spacing: 2) {
                    Text(bmsManager.isConnected ? bmsManager.telemetry.deviceName : "PIN ANT BMS")
                        .font(.system(size: 13, weight: .bold, design: .monospaced))
                        .foregroundColor(.white)
                    Text(bmsManager.statusMessage)
                        .font(.system(size: 11, weight: .medium))
                        .foregroundColor(.gray)
                }
            }
            
            Spacer()
            
            Button(action: {
                if bmsManager.isConnected {
                    bmsManager.disconnect()
                } else {
                    bmsManager.startScanning()
                    showScannerSheet = true
                }
            }) {
                HStack(spacing: 4) {
                    Image(systemName: bmsManager.isConnected ? "xmark.circle" : "magnifyingglass")
                    Text(bmsManager.isConnected ? "NGẮT" : "TÌM PIN ANT")
                }
                .font(.system(size: 11, weight: .bold))
                .foregroundColor(bmsManager.isConnected ? .red : .green)
                .padding(.horizontal, 12)
                .padding(.vertical, 6)
                .background(Capsule().stroke(bmsManager.isConnected ? Color.red.opacity(0.6) : Color.green.opacity(0.8), lineWidth: 1))
            }
        }
        .padding(12)
        .background(RoundedRectangle(cornerRadius: 16).fill(Color.white.opacity(0.04)))
    }
    
    // MARK: - Thẻ tổng quan Pack Pin
    private var bmsOverviewCards: some View {
        VStack(spacing: 12) {
            HStack(spacing: 12) {
                BmsMetricCard(
                    title: "ĐIỆN ÁP PACK",
                    value: String(format: "%.1f V", bmsManager.telemetry.totalVoltage),
                    subtitle: "\(bmsManager.telemetry.cellCount) Cells Pin",
                    icon: "battery.100bolt",
                    accentColor: .green
                )
                
                BmsMetricCard(
                    title: "DÒNG XẢ / NẠP",
                    value: String(format: "%.1f A", bmsManager.telemetry.current),
                    subtitle: bmsManager.telemetry.current > 0 ? "Đang xả" : (bmsManager.telemetry.current < 0 ? "Đang nạp" : "Nghỉ"),
                    icon: "arrow.up.arrow.down.circle.fill",
                    accentColor: bmsManager.telemetry.current > 0 ? .yellow : (bmsManager.telemetry.current < 0 ? .green : .cyan)
                )
            }
            
            HStack(spacing: 12) {
                BmsMetricCard(
                    title: "DUNG LƯỢNG (% SOC)",
                    value: "\(bmsManager.telemetry.soc)%",
                    subtitle: String(format: "%.1f Ah còn lại", bmsManager.telemetry.remainingAh),
                    icon: "chart.bar.fill",
                    accentColor: bmsManager.telemetry.soc > 20 ? .cyan : .red
                )
                
                BmsMetricCard(
                    title: "NHIỆT ĐỘ CELL",
                    value: "\(bmsManager.telemetry.temp1)°C / \(bmsManager.telemetry.temp2)°C",
                    subtitle: "Cảm biến T1 / T2",
                    icon: "thermometer.medium",
                    accentColor: .orange
                )
            }
        }
    }
    
    // MARK: - Thẻ Cân Bằng Cell (Delta mV)
    private var cellBalanceHealthCard: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Image(systemName: "gauge.with.needle")
                    .foregroundColor(deltaColor(bmsManager.telemetry.deltaMv))
                Text("CÂN BẰNG PACK PIN (CELL BALANCE)")
                    .font(.system(size: 11, weight: .bold, design: .monospaced))
                    .foregroundColor(.white)
                Spacer()
                Text("Δ \(bmsManager.telemetry.deltaMv) mV")
                    .font(.system(size: 14, weight: .black, design: .rounded))
                    .foregroundColor(deltaColor(bmsManager.telemetry.deltaMv))
            }
            
            Divider().background(Color.gray.opacity(0.3))
            
            HStack(spacing: 16) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("CELL THẤP NHẤT (MIN)")
                        .font(.system(size: 9, weight: .bold, design: .monospaced))
                        .foregroundColor(.gray)
                    Text("Cell #\(bmsManager.telemetry.minCellIndex): \(bmsManager.telemetry.minCellMv) mV")
                        .font(.system(size: 12, weight: .bold, design: .rounded))
                        .foregroundColor(.orange)
                }
                
                Spacer()
                
                VStack(alignment: .trailing, spacing: 2) {
                    Text("CELL CAO NHẤT (MAX)")
                        .font(.system(size: 9, weight: .bold, design: .monospaced))
                        .foregroundColor(.gray)
                    Text("Cell #\(bmsManager.telemetry.maxCellIndex): \(bmsManager.telemetry.maxCellMv) mV")
                        .font(.system(size: 12, weight: .bold, design: .rounded))
                        .foregroundColor(.green)
                }
            }
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 16).fill(Color.white.opacity(0.04)))
        .overlay(RoundedRectangle(cornerRadius: 16).stroke(deltaColor(bmsManager.telemetry.deltaMv).opacity(0.4), lineWidth: 1))
    }
    
    // MARK: - Danh sách 32 Cell Pin
    private var cellVoltagesSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("CHI TIẾT ĐIỆN ÁP TỪNG CELL")
                    .font(.system(size: 11, weight: .bold, design: .monospaced))
                    .foregroundColor(.gray)
                Spacer()
                Text("\(bmsManager.telemetry.cellVoltages.count) Cells")
                    .font(.system(size: 11, weight: .semibold))
                    .foregroundColor(.cyan)
            }
            
            if bmsManager.telemetry.cellVoltages.isEmpty {
                VStack(spacing: 8) {
                    Image(systemName: "battery.0")
                        .font(.system(size: 32))
                        .foregroundColor(.gray.opacity(0.5))
                    Text("Chưa có dữ liệu Cell Pin. Hãy kết nối với Pin ANT BMS qua Bluetooth.")
                        .font(.system(size: 12, weight: .medium))
                        .foregroundColor(.gray)
                        .multilineTextAlignment(.center)
                }
                .frame(maxWidth: .infinity)
                .padding(24)
                .background(RoundedRectangle(cornerRadius: 14).fill(Color.white.opacity(0.02)))
            } else {
                LazyVGrid(columns: cellColumns, spacing: 10) {
                    ForEach(Array(bmsManager.telemetry.cellVoltages.enumerated()), id: \.offset) { index, mv in
                        let cellNum = index + 1
                        let isMin = cellNum == bmsManager.telemetry.minCellIndex
                        let isMax = cellNum == bmsManager.telemetry.maxCellIndex
                        
                        CellItemView(
                            cellNumber: cellNum,
                            mv: mv,
                            isMin: isMin,
                            isMax: isMax
                        )
                    }
                }
            }
        }
    }
    
    private func deltaColor(_ delta: Int) -> Color {
        if delta <= 15 { return .green }
        if delta <= 35 { return .yellow }
        return .red
    }
}

// MARK: - Component Cell Item View
struct CellItemView: View {
    let cellNumber: Int
    let mv: Int
    let isMin: Bool
    let isMax: Bool
    
    var body: some View {
        HStack(spacing: 8) {
            Text(String(format: "%02d", cellNumber))
                .font(.system(size: 11, weight: .black, design: .monospaced))
                .foregroundColor(isMin ? .orange : (isMax ? .green : .cyan))
                .frame(width: 22)
            
            VStack(alignment: .leading, spacing: 3) {
                HStack {
                    Text(String(format: "%.3f V", Float(mv) / 1000.0))
                        .font(.system(size: 13, weight: .bold, design: .monospaced))
                        .foregroundColor(.white)
                    Spacer()
                    if isMin {
                        Text("MIN")
                            .font(.system(size: 8, weight: .heavy))
                            .foregroundColor(.white)
                            .padding(.horizontal, 4)
                            .padding(.vertical, 1)
                            .background(Capsule().fill(Color.orange))
                    } else if isMax {
                        Text("MAX")
                            .font(.system(size: 8, weight: .heavy))
                            .foregroundColor(.white)
                            .padding(.horizontal, 4)
                            .padding(.vertical, 1)
                            .background(Capsule().fill(Color.green))
                    }
                }
                
                // Mini bar
                GeometryReader { geo in
                    let fillRatio = max(0, min(1.0, CGFloat(mv - 2800) / CGFloat(4200 - 2800)))
                    ZStack(alignment: .leading) {
                        Capsule().fill(Color.gray.opacity(0.2))
                        Capsule().fill(isMin ? Color.orange : (isMax ? Color.green : Color.cyan.opacity(0.8)))
                            .frame(width: geo.size.width * fillRatio)
                    }
                }
                .frame(height: 4)
            }
        }
        .padding(10)
        .background(RoundedRectangle(cornerRadius: 12).fill(Color.white.opacity(0.04)))
        .overlay(RoundedRectangle(cornerRadius: 12).stroke(isMin ? Color.orange.opacity(0.5) : (isMax ? Color.green.opacity(0.5) : Color.white.opacity(0.08)), lineWidth: 1))
    }
}

// MARK: - Thẻ Metric BMS
struct BmsMetricCard: View {
    let title: String
    let value: String
    let subtitle: String
    let icon: String
    let accentColor: Color
    
    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Image(systemName: icon)
                    .font(.system(size: 13))
                    .foregroundColor(accentColor)
                Text(title)
                    .font(.system(size: 10, weight: .bold, design: .monospaced))
                    .foregroundColor(.gray)
            }
            Text(value)
                .font(.system(size: 22, weight: .bold, design: .rounded))
                .foregroundColor(.white)
            Text(subtitle)
                .font(.system(size: 11, weight: .medium))
                .foregroundColor(accentColor)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(RoundedRectangle(cornerRadius: 14).fill(Color.white.opacity(0.04)))
        .overlay(RoundedRectangle(cornerRadius: 14).stroke(accentColor.opacity(0.25), lineWidth: 1))
    }
}

// MARK: - Sheet Quét Thiết Bị BLE ANT BMS
struct AntBmsScannerSheet: View {
    @ObservedObject var bmsManager: AntBmsBleManager
    @Binding var isPresented: Bool
    
    var body: some View {
        NavigationView {
            ZStack {
                Color(red: 0.05, green: 0.07, blue: 0.12).ignoresSafeArea()
                
                VStack {
                    if bmsManager.isScanning {
                        HStack(spacing: 8) {
                            ProgressView()
                                .progressViewStyle(CircularProgressViewStyle(tint: .green))
                            Text("Đang quét sóng Bluetooth BLE...")
                                .font(.system(size: 12, weight: .medium))
                                .foregroundColor(.gray)
                        }
                        .padding(.vertical, 8)
                    }
                    
                    if bmsManager.discoveredDevices.isEmpty {
                        VStack(spacing: 12) {
                            Image(systemName: "antenna.radiowaves.left.and.right")
                                .font(.system(size: 40))
                                .foregroundColor(.gray.opacity(0.5))
                            Text("Chưa tìm thấy thiết bị ANT BMS nào xung quanh.\nVui lòng đảm bảo Bluetooth trên Pin ANT đã bật.")
                                .font(.system(size: 13, weight: .medium))
                                .foregroundColor(.gray)
                                .multilineTextAlignment(.center)
                        }
                        .frame(maxHeight: .infinity)
                    } else {
                        List(bmsManager.discoveredDevices) { device in
                            Button(action: {
                                bmsManager.connectToDevice(device)
                                isPresented = false
                            }) {
                                HStack {
                                    VStack(alignment: .leading, spacing: 4) {
                                        Text(device.name)
                                            .font(.system(size: 14, weight: .bold))
                                            .foregroundColor(.white)
                                        Text(device.id.uuidString)
                                            .font(.system(size: 10, design: .monospaced))
                                            .foregroundColor(.gray)
                                    }
                                    Spacer()
                                    HStack(spacing: 4) {
                                        Image(systemName: "wifi")
                                        Text("\(device.rssi) dBm")
                                    }
                                    .font(.system(size: 11, weight: .bold))
                                    .foregroundColor(.green)
                                }
                                .padding(.vertical, 6)
                            }
                            .listRowBackground(Color.white.opacity(0.04))
                        }
                        .listStyle(PlainListStyle())
                    }
                }
            }
            .navigationTitle("TÌM KIẾM PIN ANT BMS")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button("Đóng") {
                        bmsManager.stopScanning()
                        isPresented = false
                    }
                    .foregroundColor(.gray)
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Quét lại") {
                        bmsManager.startScanning()
                    }
                    .foregroundColor(.green)
                }
            }
        }
    }
}
