import Foundation

/// Dữ liệu đo lường chi tiết từ mạch sạc xả pin thông minh ANT BMS (140-byte protocol)
public struct AntBmsTelemetry: Codable, Equatable {
    public var isConnected: Bool = false
    public var deviceName: String = ""
    public var macAddress: String = ""
    
    public var totalVoltage: Float = 0.0     // Điện áp tổng Pack Pin (Volts)
    public var current: Float = 0.0          // Dòng điện xả (dương) / nạp (âm) (Amps)
    public var soc: Int = 0                  // Phần trăm dung lượng pin (% SoC)
    public var powerWatts: Float = 0.0       // Công suất tức thời (Watts = V * A)
    public var remainingAh: Float = 0.0      // Dung lượng còn lại (Ah)
    public var cellCount: Int = 0            // Số lượng cell đang hoạt động (ví dụ 16S, 20S, 24S)
    
    public var cellVoltages: [Int] = []      // Mảng điện áp từng cell tính bằng mV (ví dụ [3650, 3648, ...])
    public var deltaMv: Int = 0              // Độ lệch giữa cell cao nhất và thấp nhất (mV)
    public var minCellMv: Int = 0            // Điện áp cell thấp nhất (mV)
    public var maxCellMv: Int = 0            // Điện áp cell cao nhất (mV)
    public var minCellIndex: Int = 0         // Vị trí cell thấp nhất (1-based index)
    public var maxCellIndex: Int = 0         // Vị trí cell cao nhất (1-based index)
    
    public var temp1: Int = 0                // Cảm biến nhiệt độ 1 (°C)
    public var temp2: Int = 0                // Cảm biến nhiệt độ 2 (°C)
    
    public var isChargeMosOn: Bool = true    // Trạng thái khóa sạc (MOS Charge)
    public var isDischargeMosOn: Bool = true // Trạng thái khóa xả (MOS Discharge)
    
    public init() {}
    
    /// Đóng gói chuỗi định dạng đồng bộ gửi sang ESP32 qua BLE: "BMS:vTot:curr:soc:t1:t2:deltaMv:minMv:maxMv"
    public var esp32SyncString: String {
        return String(
            format: "BMS:%.1f:%.1f:%d:%d:%d:%d:%d:%d",
            totalVoltage,
            current,
            soc,
            temp1,
            temp2,
            deltaMv,
            minCellMv,
            maxCellMv
        )
    }
}
