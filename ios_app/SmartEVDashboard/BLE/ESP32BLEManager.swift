import Foundation
import CoreBluetooth
import Combine

/// Manager handling CoreBluetooth connection to ESP32-S3 Smart Dashboard
public class ESP32BLEManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    
    // MARK: - UUID Constants matching ESP32 config.h
    public static let serviceUUID        = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    public static let rxCharacteristicID = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
    public static let txCharacteristicID = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
    public static let targetDeviceName   = "ESP32-SmartDash"
    
    // MARK: - Published Properties for SwiftUI / CarPlay
    @Published public var isConnected: Bool = false
    @Published public var isScanning: Bool = false
    @Published public var statusMessage: String = "Sẵn sàng kết nối"
    @Published public var telemetry: VehicleTelemetry = VehicleTelemetry()
    
    // MARK: - Private CoreBluetooth Properties
    private var centralManager: CBCentralManager!
    private var esp32Peripheral: CBPeripheral?
    private var rxCharacteristic: CBCharacteristic?
    private var txCharacteristic: CBCharacteristic?
    
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
        statusMessage = "Đang quét thiết bị ESP32-SmartDash..."
        centralManager.scanForPeripherals(withServices: [ESP32BLEManager.serviceUUID], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
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
    
    // MARK: - Send Commands to ESP32 over RX Characteristic
    public func sendCommand(_ command: String) {
        guard let peripheral = esp32Peripheral, let rx = rxCharacteristic else {
            print("[BLE iOS] Chưa kết nối ESP32!")
            return
        }
        if let data = command.data(using: .utf8) {
            peripheral.writeValue(data, for: rx, type: .withoutResponse)
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
    
    // MARK: - CBCentralManagerDelegate
    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            statusMessage = "Bluetooth đã bật. Đang tìm ESP32..."
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
        print("[BLE iOS] Phát hiện thiết bị: \(peripheral.name ?? "Unknown") (RSSI: \(RSSI))")
        
        esp32Peripheral = peripheral
        esp32Peripheral?.delegate = self
        stopScanning()
        
        statusMessage = "Đã tìm thấy ESP32. Đang kết nối..."
        centralManager.connect(peripheral, options: nil)
    }
    
    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        isConnected = true
        statusMessage = "Đã kết nối với ESP32-SmartDash!"
        print("[BLE iOS] ✅ Đã kết nối thành công với ESP32!")
        
        // Khám phá Service
        peripheral.discoverServices([ESP32BLEManager.serviceUUID])
    }
    
    public func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        isConnected = false
        esp32Peripheral = nil
        rxCharacteristic = nil
        txCharacteristic = nil
        statusMessage = "Đã ngắt kết nối. Đang tự động kết nối lại..."
        print("[BLE iOS] ⚠️ Đã ngắt kết nối với ESP32.")
        
        // Tự động quét lại
        startScanning()
    }
    
    // MARK: - CBPeripheralDelegate
    public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services where service.uuid == ESP32BLEManager.serviceUUID {
            print("[BLE iOS] Tìm thấy Service: \(service.uuid)")
            peripheral.discoverCharacteristics([ESP32BLEManager.rxCharacteristicID, ESP32BLEManager.txCharacteristicID], for: service)
        }
    }
    
    public func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        for characteristic in characteristics {
            if characteristic.uuid == ESP32BLEManager.rxCharacteristicID {
                rxCharacteristic = characteristic
                print("[BLE iOS] Đã tìm thấy RX Characteristic")
            } else if characteristic.uuid == ESP32BLEManager.txCharacteristicID {
                txCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                print("[BLE iOS] Đã bật Notification cho TX Characteristic")
            }
        }
        
        // Tự động đồng bộ giờ RTC từ iPhone sang ESP32 ngay khi kết nối
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
            self.syncPhoneTimeToESP32()
        }
    }
    
    public func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == ESP32BLEManager.txCharacteristicID, let data = characteristic.value else { return }
        
        if let jsonString = String(data: data, encoding: .utf8) {
            parseTelemetryJSON(jsonString)
        }
    }
    
    private func parseTelemetryJSON(_ jsonString: String) {
        guard let jsonData = jsonString.data(using: .utf8) else { return }
        do {
            let decoder = JSONDecoder()
            let newTelemetry = try decoder.decode(VehicleTelemetry.self, from: jsonData)
            DispatchQueue.main.async {
                self.telemetry = newTelemetry
            }
        } catch {
            print("[BLE iOS] Lỗi parse JSON telemetry: \(error)")
        }
    }
}
