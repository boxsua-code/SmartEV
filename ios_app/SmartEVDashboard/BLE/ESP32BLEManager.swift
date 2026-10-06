import Foundation
import CoreBluetooth
import Combine

/// Manager xử lý kết nối CoreBluetooth đa thiết bị tới ESP32-S3 Smart Dashboard / JAMFOXRS
public class ESP32BLEManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    
    // MARK: - UUID Constants
    // 1. Nordic UART Service (NUS)
    public static let nusServiceUUID       = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    public static let nusRxCharacteristicID = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
    public static let nusTxCharacteristicID = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
    
    // 2. JAMFOXRS Service
    public static let jamfoxServiceUUID    = CBUUID(string: "4FAF0001-1FB5-459E-8FCC-C5C9C331914B")
    public static let jamfoxTxCharUUID     = CBUUID(string: "BEB5483E-36E1-4688-B7F5-EA07361B26A8")
    
    // MARK: - Published Properties for SwiftUI / CarPlay
    @Published public var isConnected: Bool = false
    @Published public var isScanning: Bool = false
    @Published public var statusMessage: String = "Sẵn sàng kết nối"
    @Published public var deviceName: String = "Chưa kết nối"
    @Published public var rssiValue: Int = 0
    @Published public var telemetry: VehicleTelemetry = VehicleTelemetry()
    @Published public var rawJsonLog: String = ""
    
    // MARK: - Private CoreBluetooth Properties
    private var centralManager: CBCentralManager!
    private var esp32Peripheral: CBPeripheral?
    private var rxCharacteristic: CBCharacteristic?
    private var txCharacteristic: CBCharacteristic?
    private let savedDeviceKey = "SavedESP32DashUUID"
    
    public override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
    public func startScanning() {
        guard centralManager.state == .poweredOn else {
            statusMessage = "Bluetooth chưa được bật"
            return
        }
        isScanning = true
        statusMessage = "Đang quét ESP32-SmartDash..."
        centralManager.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }
    
    public func stopScanning() {
        centralManager.stopScan()
        isScanning = false
    }
    
    public func disconnect() {
        if let peripheral = esp32Peripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
    }
    
    // MARK: - Gửi lệnh qua RX Characteristic
    public func sendCommand(_ command: String) {
        guard let peripheral = esp32Peripheral, let rx = rxCharacteristic else {
            print("[BLE iOS] Chưa kết nối RX ESP32!")
            return
        }
        if let data = (command + "\n").data(using: .utf8) {
            let writeType: CBCharacteristicWriteType = rx.properties.contains(.writeWithoutResponse) ? .withoutResponse : .withResponse
            peripheral.writeValue(data, for: rx, type: writeType)
            print("[BLE iOS TX] Sent: \(command)")
        }
    }
    
    public func syncPhoneTimeToESP32() {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy:MM:dd:HH:mm:ss"
        let timeStr = formatter.string(from: Date())
        sendCommand("TIME:\(timeStr)")
    }
    
    public func sendNavigationUpdate(icon: String, distance: String, instruction: String) {
        sendCommand("NAV:\(icon):\(distance):\(instruction)")
    }
    
    public func clearNavigation() {
        sendCommand("NAV:CLEAR")
    }
    
    public func sendVotolSettings(_ settings: VotolSettings) {
        if let jsonStr = settings.exportToJson() {
            // Nén gọn chuỗi gửi xuống
            let clean = jsonStr.replacingOccurrences(of: "\n", with: "").replacingOccurrences(of: "  ", with: "")
            sendCommand("VOTOL_SET:\(clean)")
        }
    }
    
    public func requestReadVotolSettings() {
        sendCommand("READ_VOTOL")
    }
    
    public func requestResetVotolSettings() {
        sendCommand("RESET_VOTOL")
    }
    
    // MARK: - CBCentralManagerDelegate
    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            statusMessage = "Bluetooth đã bật. Đang kết nối..."
            // Thử kết nối thiết bị đã lưu
            if let savedUUIDString = UserDefaults.standard.string(forKey: savedDeviceKey),
               let uuid = UUID(uuidString: savedUUIDString) {
                let knownPeripherals = centralManager.retrievePeripherals(withIdentifiers: [uuid])
                if let known = knownPeripherals.first {
                    print("[BLE iOS] Tự động kết nối lại thiết bị: \(known.name ?? "ESP32")")
                    esp32Peripheral = known
                    esp32Peripheral?.delegate = self
                    centralManager.connect(known, options: nil)
                    return
                }
            }
            startScanning()
        case .poweredOff:
            statusMessage = "Vui lòng bật Bluetooth trên iPhone"
            isConnected = false
        case .unauthorized:
            statusMessage = "Chưa cấp quyền Bluetooth cho App"
        default:
            statusMessage = "Trạng thái Bluetooth: \(central.state.rawValue)"
        }
    }
    
    public func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        let name = peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? ""
        
        let isTarget = name.contains("SmartDash") || name.contains("ESP32") || name.contains("JAMFOX") || name.contains("Votol")
        
        if isTarget {
            print("[BLE iOS] Phát hiện thiết bị: \(name) (RSSI: \(RSSI))")
            esp32Peripheral = peripheral
            esp32Peripheral?.delegate = self
            rssiValue = RSSI.intValue
            stopScanning()
            
            statusMessage = "Đã tìm thấy \(name). Đang kết nối..."
            centralManager.connect(peripheral, options: nil)
            
            UserDefaults.standard.set(peripheral.identifier.uuidString, forKey: savedDeviceKey)
        }
    }
    
    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        DispatchQueue.main.async {
            self.isConnected = true
            self.deviceName = peripheral.name ?? "ESP32-SmartDash"
            self.statusMessage = "Đã kết nối: \(self.deviceName)"
        }
        print("[BLE iOS] ✅ Đã kết nối thành công với ESP32!")
        
        peripheral.discoverServices([ESP32BLEManager.nusServiceUUID, ESP32BLEManager.jamfoxServiceUUID])
    }
    
    public func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        DispatchQueue.main.async {
            self.isConnected = false
            self.deviceName = "Chưa kết nối"
            self.statusMessage = "Mất kết nối. Đang quét lại..."
        }
        print("[BLE iOS] ⚠️ Đã ngắt kết nối.")
        
        // Tự động quét lại
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) {
            self.startScanning()
        }
    }
    
    // MARK: - CBPeripheralDelegate
    public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
            peripheral.discoverCharacteristics(nil, for: service)
        }
    }
    
    public func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        for char in characteristics {
            if char.uuid == ESP32BLEManager.nusRxCharacteristicID {
                rxCharacteristic = char
                print("[BLE iOS] Đã tìm thấy NUS RX Characteristic")
            } else if char.uuid == ESP32BLEManager.nusTxCharacteristicID || char.uuid == ESP32BLEManager.jamfoxTxCharUUID {
                txCharacteristic = char
                peripheral.setNotifyValue(true, for: char)
                print("[BLE iOS] Đã kích hoạt Notify cho TX Characteristic: \(char.uuid)")
            }
        }
        
        // Tự động đồng bộ giờ RTC từ iPhone sang ESP32 ngay khi kết nối
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.6) {
            self.syncPhoneTimeToESP32()
        }
    }
    
    public func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value, let jsonString = String(data: data, encoding: .utf8) else { return }
        
        DispatchQueue.main.async {
            self.rawJsonLog = jsonString
        }
        parseTelemetryJSON(jsonString)
    }
    
    private func parseTelemetryJSON(_ jsonString: String) {
        guard let jsonData = jsonString.data(using: .utf8) else { return }
        
        // 1. Thử parse theo cấu trúc SmartEV chuẩn
        if let newTelemetry = try? JSONDecoder().decode(VehicleTelemetry.self, from: jsonData) {
            DispatchQueue.main.async {
                self.telemetry = newTelemetry
            }
            return
        }
        
        // 2. Thử parse theo định dạng JAMFOX Fast JSON {"r":..., "s":..., "m":...}
        if let jsonDict = try? JSONSerialization.jsonObject(with: jsonData) as? [String: Any] {
            var updated = self.telemetry
            if let rpm = jsonDict["r"] as? Int {
                // Tính sơ bộ tốc độ từ RPM nếu thiếu field spd
                if jsonDict["spd"] == nil {
                    let tireCircumferenceM: Float = 1.45 // 1450 mm
                    let kmh = (Float(rpm) * 60.0 * tireCircumferenceM) / 1000.0
                    updated.spd = kmh
                }
            }
            if let spd = jsonDict["spd"] as? Double { updated.spd = Float(spd) }
            if let v = jsonDict["v"] as? Double { updated.v = Float(v) }
            if let a = jsonDict["a"] as? Double { updated.a = Float(a) }
            if let soc = jsonDict["soc"] as? Int { updated.soc = soc }
            if let p = jsonDict["p"] as? Double { updated.p = Float(p) }
            if let g = jsonDict["gear"] as? String { updated.gear = g }
            if let tm = jsonDict["temp_m"] as? Int { updated.temp_m = tm }
            if let tc = jsonDict["temp_c"] as? Int { updated.temp_c = tc }
            if let tl = jsonDict["turn_l"] as? Int { updated.turn_l = tl }
            if let tr = jsonDict["turn_r"] as? Int { updated.turn_r = tr }
            if let bm = jsonDict["beam"] as? Int { updated.beam = bm }
            if let br = jsonDict["brk"] as? Int { updated.brk = br }
            if let st = jsonDict["stand"] as? Int { updated.stand = st }
            
            DispatchQueue.main.async {
                self.telemetry = updated
            }
        }
    }
}
