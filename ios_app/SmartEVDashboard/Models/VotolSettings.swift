import Foundation

/// Cấu hình chi tiết 4 Trang thông số IC điều tốc VOTOL EM Series (Chuẩn VOTOL-EM-V3 Debugging PC)
public struct VotolSettings: Codable, Equatable {
    
    // MARK: - PAGE 1: NGUỒN PIN & TAY GA (Basic & Throttle)
    public var model: String = "EM-150"
    public var overvoltage: Float = 88.0       // V
    public var undervoltage: Float = 60.0      // V
    public var softUndervoltage: Float = 62.0  // V
    public var variation: Float = 2.0          // V
    public var busbarCurrent: Int = 150        // A
    public var phaseCurrent: Int = 390         // A
    
    // Throttle Voltage Setup (V)
    public var throttleLowProtect: Float = 0.8
    public var throttleStart: Float = 1.1
    public var throttleEnd: Float = 3.6
    public var throttleHighProtect: Float = 4.2
    
    // Start settings
    public var startTorque: Int = 30           // %
    public var combinativeTorque: Int = 50     // %
    public var rateOfRise: Int = 50
    public var rateOfDecline: Int = 50
    
    // MARK: - PAGE 2: CẤP SỐ & CHẾ ĐỘ LÁI (Three-Speed & Sport Mode)
    public var sportCurrentLimit: Int = 180    // A
    public var sportFluxWeakening: Int = 35    // A (Mở tua)
    public var sportLogoutTime: Int = 15       // Giây
    public var sportRecoveryTime: Int = 10     // Giây
    
    // Three-speed limits
    public var speedLowRatio: Int = 60         // %
    public var currentLowRatio: Int = 50       // %
    public var speedMidRatio: Int = 80         // %
    public var currentMidRatio: Int = 80       // %
    public var speedHighRatio: Int = 100       // %
    public var currentHighRatio: Int = 100     // %
    
    public var midFluxWeakening: Int = 10      // A
    public var highFluxWeakening: Int = 30     // A
    public var speedSwitchType: String = "Button" // "Button" (Nút bấm) hoặc "Switch" (Công tắc)
    public var defaultGear: String = "Mid"     // "Low", "Mid", "High"
    public var softStartEnable: Bool = true
    public var softStartLevel: Int = 3         // 1 - 5
    public var hhcEnable: Bool = false         // Khởi hành ngang dốc
    public var hdcEnable: Bool = false         // Hỗ trợ đổ đèo
    public var overallSpeedLimit: Int = 100    // %
    
    // MARK: - PAGE 3: ĐỘNG CƠ & CẢM BIẾN (Motor Setting & Functions)
    public var polePairs: Int = 5              // Số cặp cực (5 cho QS/Yuma)
    public var hallYellowGreenSwap: Bool = false
    public var phaseBlueGreenSwap: Bool = false
    public var motorType: String = "Surface-mount" // "Surface-mount" hoặc "V-type"
    public var hallShiftAngle: Int = 0         // Độ lệch góc
    
    public var reverseSpeedLimit: Int = 30     // %
    public var ebsRatio: Int = 40              // % Phanh điện tử
    public var lowBrakeEnable: Bool = true
    public var secureBootEnable: Bool = true
    
    public var meterSignalType: String = "One-Lin" // "One-Lin", "Hall", "CAN"
    public var movingBoosterEnable: Bool = true   // Trợ lực dắt xe
    public var cruiseControlEnable: Bool = true   // Ga tự động
    public var doubleVoltageEnable: Bool = false  // Nhận diện 2 cấp áp
    
    // MARK: - PAGE 4: CỔNG CHỨC NĂNG & THÔNG SỐ XE (Port Settings & Vehicle Display)
    public var portPD0: String = "Brake (Phanh)"
    public var portPB3: String = "Reverse (Lùi)"
    public var portPA0: String = "Park (Số P)"
    public var portPB2: String = "Anti-Theft (Chống trộm)"
    public var portPC14: String = "Cruise (Ga tự động)"
    public var portPA11: String = "Boost (Tăng tốc)"
    
    public var tireCircumferenceMm: Int = 1450 // Chu vi bánh xe (mm)
    public var gearRatio: Float = 1.0          // Tỉ số truyền
    public var oledBrightness: Int = 100       // Độ sáng OLED xe (10% - 100%)
    
    public init() {}
    
    /// Chuyển cấu hình sang chuỗi lệnh gửi xuống ESP32 / Votol
    public func exportToJson() -> String? {
        let encoder = JSONEncoder()
        encoder.outputFormatting = .prettyPrinted
        if let data = try? encoder.encode(self) {
            return String(data: data, encoding: .utf8)
        }
        return nil
    }
}
