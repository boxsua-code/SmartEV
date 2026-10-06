package com.smartev.dashboard.ble

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.util.Log
import com.google.gson.Gson
import com.smartev.dashboard.model.BmsData
import com.smartev.dashboard.model.VehicleSettings
import com.smartev.dashboard.model.VehicleTelemetry
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.nio.charset.StandardCharsets
import java.util.*

@SuppressLint("MissingPermission")
class BleManager private constructor(private val context: Context) {

    companion object {
        private const val TAG = "SmartEV_BleManager"
        const val DEVICE_NAME = "ESP32-SmartDash"
        val KNOWN_DEVICE_NAMES = listOf("ESP32-SmartDash", "JAMFOXRS", "Votol_BLE", "Votol_TCH", "SmartEV", "xe_TCH", "SmartDash")

        // 1. Nordic UART Service (NUS) UUIDs (Chuẩn SmartEV)
        val NUS_SERVICE_UUID: UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
        val NUS_RX_CHAR_UUID: UUID = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
        val NUS_TX_CHAR_UUID: UUID = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")

        // 2. JAMFOXRS Service UUIDs (Tương thích ngược 100% với firmware JAMFOXRS)
        val FOX_SERVICE_UUID: UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
        val FOX_TELEMETRY_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
        val FOX_COMMAND_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a9")

        val CCCD_UUID: UUID    = UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")

        @Volatile
        private var INSTANCE: BleManager? = null

        fun getInstance(context: Context): BleManager {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: BleManager(context.applicationContext).also { INSTANCE = it }
            }
        }
    }

    enum class BleState {
        DISCONNECTED,
        SCANNING,
        CONNECTING,
        CONNECTED
    }

    private val bluetoothManager: BluetoothManager? =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager?.adapter

    private var bluetoothGatt: BluetoothGatt? = null
    private var rxCharacteristic: BluetoothGattCharacteristic? = null

    private val _connectionState = MutableStateFlow(BleState.DISCONNECTED)
    val connectionState: StateFlow<BleState> = _connectionState.asStateFlow()

    private val _telemetry = MutableStateFlow(VehicleTelemetry())
    val telemetry: StateFlow<VehicleTelemetry> = _telemetry.asStateFlow()

    private val _bmsData = MutableStateFlow(BmsData())
    val bmsData: StateFlow<BmsData> = _bmsData.asStateFlow()

    private val gson = Gson()
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var rxBuffer = StringBuilder()

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult?) {
            result?.device?.let { device ->
                val name = device.name ?: result.scanRecord?.deviceName
                val isKnown = if (name != null) {
                    KNOWN_DEVICE_NAMES.any { name.contains(it, ignoreCase = true) }
                } else false

                val hasService = result.scanRecord?.serviceUuids?.any {
                    it.uuid == NUS_SERVICE_UUID || it.uuid == FOX_SERVICE_UUID
                } ?: false

                if (isKnown || hasService) {
                    Log.i(TAG, "Tìm thấy ESP32: ${name ?: "Unknown"} [${device.address}]. Đang kết nối...")
                    stopScan()
                    connectToDevice(device)
                }
            }
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e(TAG, "Quét BLE thất bại, mã lỗi: $errorCode")
            _connectionState.value = BleState.DISCONNECTED
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.i(TAG, "Đã kết nối GATT tới ESP32, đang khám phá Services...")
                _connectionState.value = BleState.CONNECTING
                gatt?.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.w(TAG, "Mất kết nối GATT tới ESP32.")
                _connectionState.value = BleState.DISCONNECTED
                bluetoothGatt?.close()
                bluetoothGatt = null
                rxCharacteristic = null
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS && gatt != null) {
                val nusService = gatt.getService(NUS_SERVICE_UUID)
                val foxService = gatt.getService(FOX_SERVICE_UUID)

                if (nusService != null) {
                    rxCharacteristic = nusService.getCharacteristic(NUS_RX_CHAR_UUID)
                    val txCharacteristic = nusService.getCharacteristic(NUS_TX_CHAR_UUID)

                    if (txCharacteristic != null) {
                        gatt.setCharacteristicNotification(txCharacteristic, true)
                        val descriptor = txCharacteristic.getDescriptor(CCCD_UUID)
                        if (descriptor != null) {
                            descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                            gatt.writeDescriptor(descriptor)
                        }
                    }
                    Log.i(TAG, "Đã kích hoạt NUS Service thành công!")
                    _connectionState.value = BleState.CONNECTED
                    syncPhoneTimeToVehicle()
                } else if (foxService != null) {
                    rxCharacteristic = foxService.getCharacteristic(FOX_COMMAND_UUID)
                    val telemetryChar = foxService.getCharacteristic(FOX_TELEMETRY_UUID)

                    if (telemetryChar != null) {
                        gatt.setCharacteristicNotification(telemetryChar, true)
                        val descriptor = telemetryChar.getDescriptor(CCCD_UUID)
                        if (descriptor != null) {
                            descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                            gatt.writeDescriptor(descriptor)
                        }
                    }
                    Log.i(TAG, "Đã kích hoạt JAMFOXRS Service thành công!")
                    _connectionState.value = BleState.CONNECTED
                } else {
                    Log.e(TAG, "Không tìm thấy NUS hoặc JAMFOXRS Service trên ESP32!")
                }
            }
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt?,
            characteristic: BluetoothGattCharacteristic?
        ) {
            characteristic?.value?.let { bytes ->
                val chunk = String(bytes, StandardCharsets.UTF_8)
                parseIncomingBleData(chunk)
            }
        }
    }

    fun startScan() {
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled) {
            Log.e(TAG, "Bluetooth chưa bật!")
            return
        }
        if (_connectionState.value == BleState.SCANNING || _connectionState.value == BleState.CONNECTED) {
            return
        }
        _connectionState.value = BleState.SCANNING
        val scanner = bluetoothAdapter.bluetoothLeScanner
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        scanner.startScan(null, settings, scanCallback)
        Log.i(TAG, "Bắt đầu quét thiết bị $DEVICE_NAME...")
    }

    fun stopScan() {
        if (_connectionState.value == BleState.SCANNING) {
            bluetoothAdapter?.bluetoothLeScanner?.stopScan(scanCallback)
            if (_connectionState.value != BleState.CONNECTED && _connectionState.value != BleState.CONNECTING) {
                _connectionState.value = BleState.DISCONNECTED
            }
        }
    }

    fun disconnect() {
        stopScan()
        bluetoothGatt?.disconnect()
        bluetoothGatt?.close()
        bluetoothGatt = null
        rxCharacteristic = null
        _connectionState.value = BleState.DISCONNECTED
    }

    private fun connectToDevice(device: BluetoothDevice) {
        _connectionState.value = BleState.CONNECTING
        bluetoothGatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    private fun parseIncomingBleData(chunk: String) {
        rxBuffer.append(chunk)
        val content = rxBuffer.toString()
        if (content.contains("\n")) {
            val lines = content.split("\n")
            for (i in 0 until lines.size - 1) {
                val line = lines[i].trim()
                if (line.isNotEmpty()) {
                    processJsonLine(line)
                }
            }
            rxBuffer = StringBuilder(lines.last())
        }
    }

    private fun processJsonLine(jsonStr: String) {
        try {
            if (jsonStr.contains("\"type\":\"bms\"")) {
                val bms = gson.fromJson(jsonStr, BmsData::class.java)
                _bmsData.value = bms
            } else if (jsonStr.contains("\"spd\"")) {
                val telem = gson.fromJson(jsonStr, VehicleTelemetry::class.java)
                _telemetry.value = telem
            } else if (jsonStr.contains("\"r\"") && (jsonStr.contains("\"s\"") || jsonStr.contains("\"type\":\"fast\"") || jsonStr.contains("\"type\":\"full\""))) {
                // Tự động phân tích gói tin Telemetry chuẩn JAMFOXRS
                val obj = org.json.JSONObject(jsonStr)
                val spd = obj.optDouble("s", 0.0).toFloat()
                val v = obj.optDouble("v", 0.0).toFloat()
                val a = obj.optDouble("a", 0.0).toFloat()
                val p = obj.optDouble("p", (v * a).toDouble()).toInt()
                val sc = obj.optInt("sc", 0)
                val mode = obj.optString("m", "D").uppercase(Locale.ROOT)
                val temps = obj.optJSONObject("t")
                val cTemp = temps?.optInt("c", 0) ?: 0
                val mTemp = temps?.optInt("m", 0) ?: 0

                val isParked = if (mode.contains("PARK")) 1 else 0
                val isRev = if (mode.contains("REVERSE")) 1 else 0
                val isBrk = if (mode.contains("BRAKE")) 1 else 0
                val isStand = if (mode.contains("STAND")) 1 else 0

                val cur = _telemetry.value
                _telemetry.value = cur.copy(
                    speedKmh = spd,
                    gear = mode,
                    voltage = v,
                    current = a,
                    powerW = p,
                    soc = sc,
                    controllerTemp = cTemp,
                    motorTemp = mTemp,
                    parked = isParked,
                    reverse = isRev,
                    brake = isBrk,
                    sideStand = isStand
                )

                val cellsJson = obj.optJSONArray("cells")
                if (cellsJson != null && cellsJson.length() > 0) {
                    val cellsList = mutableListOf<Int>()
                    var minVal = 9999
                    var maxVal = 0
                    var minIdx = 0
                    var maxIdx = 0
                    for (i in 0 until cellsJson.length()) {
                        val cVal = cellsJson.optInt(i, 0)
                        cellsList.add(cVal)
                        if (cVal in 1 until minVal) {
                            minVal = cVal
                            minIdx = i + 1
                        }
                        if (cVal > maxVal) {
                            maxVal = cVal
                            maxIdx = i + 1
                        }
                    }
                    val delta = if (maxVal > minVal && minVal != 9999) maxVal - minVal else 0
                    _bmsData.value = BmsData(
                        type = "bms",
                        soc = sc,
                        voltage = v,
                        current = a,
                        temp1 = cTemp,
                        temp2 = mTemp,
                        cells = cellsList,
                        cellMinIndex = minIdx,
                        cellMinVoltage = if (minVal == 9999) 0 else minVal,
                        cellMaxIndex = maxIdx,
                        cellMaxVoltage = maxVal,
                        deltaMv = delta
                    )
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "Lỗi phân tích JSON từ ESP32: ${e.message} | Payload: $jsonStr")
        }
    }

    fun sendCommand(cmd: String): Boolean {
        val gatt = bluetoothGatt ?: return false
        val charac = rxCharacteristic ?: return false
        val bytes = (cmd + "\n").toByteArray(StandardCharsets.UTF_8)
        charac.value = bytes
        charac.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        return gatt.writeCharacteristic(charac)
    }

    fun sendNavigation(turnIcon: String, distance: String, street: String) {
        // Định dạng gửi sang ESP32 OLED: NAV:ICON:DIST:STREET
        val cleanStreet = street.replace(":", " ")
        sendCommand("NAV:$turnIcon:$distance:$cleanStreet")
    }

    fun sendMedia(title: String, artist: String) {
        sendCommand("MEDIA:$title - $artist")
    }

    fun sendVehicleSettings(settings: VehicleSettings) {
        scope.launch {
            sendCommand("SETTING:TIRE:${settings.tireCircumferenceMm}")
            delay(50)
            sendCommand("SETTING:POLES:${settings.polePairs}")
            delay(50)
            sendCommand("SETTING:BUS_A:${settings.busCurrentLimitA}")
            delay(50)
            sendCommand("SETTING:PHASE_A:${settings.phaseCurrentLimitA}")
            delay(50)
            sendCommand("SETTING:OVER_V:${settings.overvoltageV}")
            delay(50)
            sendCommand("SETTING:UNDER_V:${settings.undervoltageV}")
            delay(50)
            sendCommand("SETTING:SPORT_A:${settings.sportCurrentLimitA}")
            delay(50)
            sendCommand("SETTING:FLUX:${settings.sportFluxWeakening}")
            delay(50)
            sendCommand("SETTING:EBS:${settings.ebsRatio}")
            delay(50)
            sendCommand("SETTING:GEAR_RATIO:${settings.gearRatio}")
            delay(50)
            sendCommand("SETTING:BRIGHT:${settings.oledBrightness}")
            delay(50)
            sendCommand("SETTING:SAVE:1")
            Log.i(TAG, "Đã gửi toàn bộ gói cấu hình xe 4 trang Votol xuống ESP32-S3 Flash!")
        }
    }

    fun requestBmsUpdate() {
        sendCommand("REQ:BMS")
    }

    fun sendVotolPoll() {
        sendCommand("POLL_VOTOL")
    }

    fun setVotolBaud(baud: Long) {
        sendCommand("SET_BAUD:$baud")
    }

    fun swapUartPins() {
        sendCommand("SWAP_PINS")
    }

    fun syncPhoneTimeToVehicle() {
        val cal = Calendar.getInstance()
        val y = cal.get(Calendar.YEAR)
        val m = cal.get(Calendar.MONTH) + 1
        val d = cal.get(Calendar.DAY_OF_MONTH)
        val h = cal.get(Calendar.HOUR_OF_DAY)
        val min = cal.get(Calendar.MINUTE)
        val s = cal.get(Calendar.SECOND)
        val timeCmd = String.format(Locale.US, "TIME:%04d:%02d:%02d:%02d:%02d:%02d", y, m, d, h, min, s)
        scope.launch {
            delay(500) // Đợi 500ms để kênh BLE Notification & RX ổn định
            sendCommand(timeCmd)
            Log.i(TAG, "Đã tự động gửi thời gian chuẩn từ điện thoại xuống ESP32 RTC: $timeCmd")
        }
    }

    fun syncBmsDataToEsp32(bms: BmsData) {
        _bmsData.value = bms
        val cmd = String.format(
            Locale.US,
            "BMS:%.1f:%.1f:%d:%d:%d:%d:%d:%d",
            bms.voltage,
            bms.current,
            bms.soc,
            bms.temp1,
            bms.temp2,
            bms.deltaMv,
            bms.cellMinVoltage,
            bms.cellMaxVoltage
        )
        sendCommand(cmd)
    }
}
