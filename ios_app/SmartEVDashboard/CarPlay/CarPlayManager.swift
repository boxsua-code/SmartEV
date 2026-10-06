import Foundation
import CarPlay
import UIKit

/// Manager for building and updating Apple CarPlay UI Templates
public class CarPlayManager: NSObject {
    
    public static let shared = CarPlayManager()
    private var interfaceController: CPInterfaceController?
    private var bleManager: ESP32BLEManager?
    
    // CarPlay Templates
    private var gridTemplate: CPGridTemplate?
    private var listTemplate: CPListTemplate?
    
    public func setup(interfaceController: CPInterfaceController, bleManager: ESP32BLEManager) {
        self.interfaceController = interfaceController
        self.bleManager = bleManager
        
        let rootTemplate = createDashboardGridTemplate()
        self.interfaceController?.setRootTemplate(rootTemplate, animated: true, completion: nil)
    }
    
    // MARK: - Create CarPlay Grid Template (Main Cockpit View)
    public func createDashboardGridTemplate() -> CPGridTemplate {
        let currentTelemetry = bleManager?.telemetry ?? VehicleTelemetry()
        
        // Grid Item 1: Tốc độ (Speed)
        let speedItem = CPGridButton(
            titleVariants: ["\(Int(currentTelemetry.spd)) km/h", "Tốc độ: \(Int(currentTelemetry.spd)) km/h"],
            image: UIImage(systemName: "gauge.with.dots.needle.bottom.50percent") ?? UIImage(),
            handler: { _ in }
        )
        
        // Grid Item 2: Pin SOC (Battery %)
        let batteryItem = CPGridButton(
            titleVariants: ["Pin: \(currentTelemetry.soc)%", "\(currentTelemetry.soc)% (\(currentTelemetry.estimatedRangeKm) km)"],
            image: UIImage(systemName: "battery.100bolt") ?? UIImage(),
            handler: { _ in }
        )
        
        // Grid Item 3: Cấp số (Gear)
        let gearItem = CPGridButton(
            titleVariants: ["Số: \(currentTelemetry.gear)", "Cấp số: \(currentTelemetry.gear)"],
            image: UIImage(systemName: "gearshape.fill") ?? UIImage(),
            handler: { _ in }
        )
        
        // Grid Item 4: Điện áp (Voltage)
        let voltageItem = CPGridButton(
            titleVariants: ["\(String(format: "%.1f", currentTelemetry.v)) V", "Áp: \(String(format: "%.1f", currentTelemetry.v)) V"],
            image: UIImage(systemName: "bolt.fill") ?? UIImage(),
            handler: { _ in }
        )
        
        // Grid Item 5: Dòng điện & Công suất (Power)
        let powerItem = CPGridButton(
            titleVariants: ["\(Int(currentTelemetry.p)) W", "Công suất: \(Int(currentTelemetry.p)) W"],
            image: UIImage(systemName: "waveform.path.ecg") ?? UIImage(),
            handler: { _ in }
        )
        
        // Grid Item 6: Nhiệt độ IC & Động cơ (Temps)
        let tempItem = CPGridButton(
            titleVariants: ["Nhiệt: \(currentTelemetry.temp_c)°C/\(currentTelemetry.temp_m)°C", "IC: \(currentTelemetry.temp_c)°C - Động cơ: \(currentTelemetry.temp_m)°C"],
            image: UIImage(systemName: "thermometer.medium") ?? UIImage(),
            handler: { _ in }
        )
        
        let grid = CPGridTemplate(title: "Smart EV Dashboard", gridButtons: [speedItem, batteryItem, gearItem, voltageItem, powerItem, tempItem])
        self.gridTemplate = grid
        return grid
    }
    
    // MARK: - Update CarPlay Dashboard in Real-time
    public func updateCarPlayDashboard(with telemetry: VehicleTelemetry) {
        guard let grid = gridTemplate else { return }
        
        let speedItem = CPGridButton(
            titleVariants: ["\(Int(telemetry.spd)) km/h", "Tốc độ: \(Int(telemetry.spd)) km/h"],
            image: UIImage(systemName: "gauge.with.dots.needle.bottom.50percent") ?? UIImage(),
            handler: { _ in }
        )
        
        let batteryItem = CPGridButton(
            titleVariants: ["Pin: \(telemetry.soc)%", "\(telemetry.soc)% (~\(telemetry.estimatedRangeKm) km)"],
            image: UIImage(systemName: telemetry.soc > 20 ? "battery.100bolt" : "battery.25") ?? UIImage(),
            handler: { _ in }
        )
        
        let gearItem = CPGridButton(
            titleVariants: ["Số: \(telemetry.gear)", "Cấp số: \(telemetry.gear)"],
            image: UIImage(systemName: "gearshape.fill") ?? UIImage(),
            handler: { _ in }
        )
        
        let voltageItem = CPGridButton(
            titleVariants: ["\(String(format: "%.1f", telemetry.v)) V", "Áp pin: \(String(format: "%.1f", telemetry.v)) V"],
            image: UIImage(systemName: "bolt.fill") ?? UIImage(),
            handler: { _ in }
        )
        
        let powerItem = CPGridButton(
            titleVariants: ["\(Int(telemetry.p)) W", "Công suất: \(Int(telemetry.p)) W (\(String(format: "%.1f", telemetry.a))A)"],
            image: UIImage(systemName: "waveform.path.ecg") ?? UIImage(),
            handler: { _ in }
        )
        
        let tempItem = CPGridButton(
            titleVariants: ["IC: \(telemetry.temp_c)°C | Động cơ: \(telemetry.temp_m)°C"],
            image: UIImage(systemName: "thermometer.medium") ?? UIImage(),
            handler: { _ in }
        )
        
        grid.updateGridButtons([speedItem, batteryItem, gearItem, voltageItem, powerItem, tempItem])
    }
}
