import Foundation

/// Swift Struct mapping real-time JSON Telemetry sent from ESP32-S3 over BLE
public struct VehicleTelemetry: Codable, Equatable {
    public var spd: Float          // Speed in km/h
    public var gear: String        // Gear: P, ECO, D, SPORT, R
    public var v: Float             // Voltage (Volts)
    public var a: Float             // Current (Amps)
    public var soc: Int             // Battery State of Charge (%)
    public var p: Float             // Power (Watts)
    public var temp_m: Int          // Motor Temp (°C)
    public var temp_c: Int          // Controller Temp (°C)
    public var turn_l: Int          // Left Turn Signal (0/1)
    public var turn_r: Int          // Right Turn Signal (0/1)
    public var beam: Int            // Headlight High Beam (0/1)
    public var err: UInt32          // 32-bit Votol Fault Code
    public var brk: Int             // Brake active (0/1)
    public var stand: Int           // Side stand down (0/1)
    public var rgn: Int             // Regen brake active (0/1)
    public var rev: Int             // Reverse active (0/1)
    public var park: Int            // Parked active (0/1)
    
    public init(
        spd: Float = 0.0,
        gear: String = "P",
        v: Float = 0.0,
        a: Float = 0.0,
        soc: Int = 0,
        p: Float = 0.0,
        temp_m: Int = 0,
        temp_c: Int = 0,
        turn_l: Int = 0,
        turn_r: Int = 0,
        beam: Int = 0,
        err: UInt32 = 0,
        brk: Int = 0,
        stand: Int = 0,
        rgn: Int = 0,
        rev: Int = 0,
        park: Int = 0
    ) {
        self.spd = spd
        self.gear = gear
        self.v = v
        self.a = a
        self.soc = soc
        self.p = p
        self.temp_m = temp_m
        self.temp_c = temp_c
        self.turn_l = turn_l
        self.turn_r = turn_r
        self.beam = beam
        self.err = err
        self.brk = brk
        self.stand = stand
        self.rgn = rgn
        self.rev = rev
        self.park = park
    }
    
    /// Estimated remaining range in km (based on average 25 Wh/km)
    public var estimatedRangeKm: Int {
        let capacityWh: Float = 72.0 * 30.0 // Default 72V 30Ah = 2160Wh
        let remainingWh = capacityWh * (Float(soc) / 100.0)
        return Int(remainingWh / 25.0)
    }
    
    /// Fault summary text
    public var faultSummary: String {
        if err == 0 { return "Hệ thống bình thường (OK)" }
        var faults: [String] = []
        if (err & 0x00000001) != 0 { faults.append("Bóp phanh") }
        if (err & 0x00000004) != 0 { faults.append("Tụt áp pin") }
        if (err & 0x00000008) != 0 { faults.append("Lỗi tay ga") }
        if (err & 0x00001000) != 0 { faults.append("Quá nhiệt IC") }
        if (err & 0x08000000) != 0 { faults.append("Quá nhiệt động cơ") }
        return faults.joined(separator: ", ")
    }
}
