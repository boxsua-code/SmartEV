import Foundation
import CoreBluetooth
import Combine

public struct DiscoveredAntDevice: Identifiable, Equatable {
    public let id: UUID
    public let name: String
    public let rssi: Int
    public let peripheral: CBPeripheral
    
    public static func == (lhs: DiscoveredAntDevice, rhs: DiscoveredAntDevice) -> Bool {
        return lhs.id == rhs.id
    }
}

/// Trình quản lý kết nối Bluetooth BLE độc lập cho mạch sạc xả pin ANT BMS
public class AntBmsBleManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    
    // MARK: - UUID Constants cho ANT BMS / JBD / JK
    public static let bmsServiceUUID = CBUUID(string: "0000FFE0-0000-1000-8000-00805F9B34FB")
    public static let bmsNotifyCharUUID = CBUUID(string: "0000FFE1-0000-1000-8000-00805F9B34FB")
    
    // MARK: - Published Properties
    @Published public var isConnected: Bool = false
    @Published public var isScanning: Bool = false
    @Published public var statusMessage: String = "Sẵn sàng kết nối Pin ANT"
    @Published public var telemetry: AntBmsTelemetry = AntBmsTelemetry()
    @Published public var discoveredDevices: [DiscoveredAntDevice] = []
    
    // Callback chuyển tiếp chuỗi đồng bộ sang ESP32
    public var onBmsDataSync: ((String) -> Void)?
    
    // MARK: - Private Properties
    private var centralManager: CBCentralManager!
    private var bmsPeripheral: CBPeripheral?
    private var rawDataBuffer = Data()
    private let savedDeviceKey = "SavedAntBmsUUID"
    
    public override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
    public func startScanning() {
        guard centralManager.state == .poweredOn else {
            statusMessage = "Bluetooth chưa được bật"
            return
        }
        discoveredDevices.removeAll()
        isScanning = true
        statusMessage = "Đang quét tìm Pin ANT BMS xung quanh..."
        
        // Quét tất cả thiết bị hoặc theo Service FFE0
        centralManager.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }
    
    public func stopScanning() {
        centralManager.stopScan()
        isScanning = false
    }
    
    public func connectToDevice(_ device: DiscoveredAntDevice) {
        stopScanning()
        bmsPeripheral = device.peripheral
        bmsPeripheral?.delegate = self
        statusMessage = "Đang kết nối tới \(device.name)..."
        centralManager.connect(device.peripheral, options: nil)
        
        // Lưu lại để tự động kết nối sau này
        UserDefaults.standard.set(device.id.uuidString, forKey: savedDeviceKey)
    }
    
    public func disconnect() {
        if let peripheral = bmsPeripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
    }
    
    // MARK: - CBCentralManagerDelegate
    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            // Tự động kết nối lại thiết bị ANT BMS đã lưu trước đó nếu có
            if let savedUUIDString = UserDefaults.standard.string(forKey: savedDeviceKey),
               let uuid = UUID(uuidString: savedUUIDString) {
                let knownPeripherals = centralManager.retrievePeripherals(withIdentifiers: [uuid])
                if let known = knownPeripherals.first {
                    print("[ANT BMS BLE] Tìm thấy thiết bị đã lưu: \(known.name ?? "ANT BMS"). Đang kết nối lại...")
                    bmsPeripheral = known
                    bmsPeripheral?.delegate = self
                    centralManager.connect(known, options: nil)
                    return
                }
            }
        }
    }
    
    public func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        let name = peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? ""
        
        // Lọc thiết bị ANT BMS, VB, JK, ANT
        let isBmsName = name.contains("ANT") || name.contains("VB") || name.contains("JK") || name.contains("BMS") || name.contains("Smart")
        
        if isBmsName || !name.isEmpty {
            if !discoveredDevices.contains(where: { $0.id == peripheral.identifier }) {
                let device = DiscoveredAntDevice(
                    id: peripheral.identifier,
                    name: name.isEmpty ? "Thiết bị BLE (\(peripheral.identifier.uuidString.prefix(6)))" : name,
                    rssi: RSSI.intValue,
                    peripheral: peripheral
                )
                DispatchQueue.main.async {
                    self.discoveredDevices.append(device)
                }
            }
        }
    }
    
    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        DispatchQueue.main.async {
            self.isConnected = true
            self.telemetry.isConnected = true
            self.telemetry.deviceName = peripheral.name ?? "ANT-BMS"
            self.telemetry.macAddress = peripheral.identifier.uuidString
            self.statusMessage = "Đã kết nối Pin ANT BMS: \(peripheral.name ?? "")"
        }
        print("[ANT BMS BLE] ✅ Đã kết nối thành công!")
        peripheral.discoverServices(nil)
    }
    
    public func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        DispatchQueue.main.async {
            self.isConnected = false
            self.telemetry.isConnected = false
            self.statusMessage = "Mất kết nối Pin ANT BMS. Đang kết nối lại..."
        }
        print("[ANT BMS BLE] ⚠️ Mất kết nối.")
        
        // Tự động thử kết nối lại
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
            self.centralManager.connect(peripheral, options: nil)
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
            // Bật notification cho characteristic Notify/Indicate
            if char.properties.contains(.notify) || char.properties.contains(.indicate) {
                peripheral.setNotifyValue(true, for: char)
                print("[ANT BMS BLE] Đã bật Notify cho Characteristic: \(char.uuid)")
            }
        }
    }
    
    public func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value, !data.isEmpty else { return }
        
        rawDataBuffer.append(data)
        
        // Xử lý gói tin ANT BMS chuẩn 140 Bytes (Header 0xAA 0x55)
        while rawDataBuffer.count >= 140 {
            // Tìm header 0xAA 0x55
            if let headerIndex = findAntHeader(in: rawDataBuffer) {
                if headerIndex > 0 {
                    rawDataBuffer.removeSubrange(0..<headerIndex)
                }
                
                if rawDataBuffer.count >= 140 {
                    let packet = rawDataBuffer.subdata(in: 0..<140)
                    parseAntBms140Bytes(packet)
                    rawDataBuffer.removeSubrange(0..<140)
                } else {
                    break
                }
            } else {
                // Không tìm thấy header, giữ lại byte cuối để tránh cắt đôi header
                if rawDataBuffer.count > 1 {
                    rawDataBuffer.removeSubrange(0..<(rawDataBuffer.count - 1))
                }
                break
            }
        }
    }
    
    private func findAntHeader(in data: Data) -> Int? {
        let bytes = [UInt8](data)
        for i in 0..<(bytes.count - 1) {
            if bytes[i] == 0xAA && bytes[i + 1] == 0x55 {
                return i
            }
        }
        return nil
    }
    
    // MARK: - Giải mã gói tin ANT BMS 140 Bytes
    private func parseAntBms140Bytes(_ data: Data) {
        let b = [UInt8](data)
        guard b.count >= 140 else { return }
        
        // 1. Điện áp tổng Pack Pin (Byte 4-5): unit 0.1V
        let rawVolt = (Int(b[4]) << 8) | Int(b[5])
        let vTot = Float(rawVolt) * 0.1
        
        // 2. Điện áp từng Cell Pin (32 cell tối đa, bắt đầu từ Byte 6, mỗi cell 2 bytes mV)
        var cells: [Int] = []
        var minMv = 9999
        var maxMv = 0
        var minIdx = 0
        var maxIdx = 0
        
        for i in 0..<32 {
            let offset = 6 + i * 2
            let cellMv = (Int(b[offset]) << 8) | Int(b[offset + 1])
            if cellMv > 500 && cellMv < 5000 { // Cell hợp lệ
                cells.append(cellMv)
                if cellMv < minMv {
                    minMv = cellMv
                    minIdx = i + 1
                }
                if cellMv > maxMv {
                    maxMv = cellMv
                    maxIdx = i + 1
                }
            }
        }
        
        if minMv == 9999 { minMv = 0 }
        let delta = (maxMv >= minMv && minMv > 0) ? (maxMv - minMv) : 0
        
        // 3. Dòng điện xả/nạp (Bytes 70-73): 4 bytes signed int, unit 0.1A
        let rawCurr = Int32(bigEndian: data.subdata(in: 70..<74).withUnsafeBytes { $0.load(as: Int32.self) })
        let currAmps = Float(rawCurr) * 0.1
        
        // 4. Phần trăm pin (% SoC) (Byte 74)
        let socVal = Int(b[74])
        
        // 5. Dung lượng còn lại (Bytes 79-82): unit Ah
        let rawAh = UInt32(bigEndian: data.subdata(in: 79..<83).withUnsafeBytes { $0.load(as: UInt32.self) })
        let remAh = Float(rawAh) * 0.000001
        
        // 6. Cảm biến nhiệt độ (Bytes 93, 95): raw - 40 (°C)
        let t1 = Int(b[93]) - 40
        let t2 = Int(b[95]) - 40
        
        // 7. Cập nhật Model Telemetry
        DispatchQueue.main.async {
            self.telemetry.totalVoltage = vTot
            self.telemetry.current = currAmps
            self.telemetry.soc = socVal
            self.telemetry.powerWatts = abs(vTot * currAmps)
            self.telemetry.remainingAh = remAh
            self.telemetry.cellCount = cells.count
            self.telemetry.cellVoltages = cells
            self.telemetry.deltaMv = delta
            self.telemetry.minCellMv = minMv
            self.telemetry.maxCellMv = maxMv
            self.telemetry.minCellIndex = minIdx
            self.telemetry.maxCellIndex = maxIdx
            self.telemetry.temp1 = t1
            self.telemetry.temp2 = t2
            
            // Đồng bộ sang ESP32 qua BLE
            self.onBmsDataSync?(self.telemetry.esp32SyncString)
        }
    }
}
