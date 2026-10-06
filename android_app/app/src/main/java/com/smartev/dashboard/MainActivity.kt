package com.smartev.dashboard

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.view.View
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import android.widget.ProgressBar
import android.widget.TextView
import com.google.android.material.button.MaterialButton
import com.smartev.dashboard.ble.AntBmsBleManager
import com.smartev.dashboard.ble.BleManager
import com.smartev.dashboard.databinding.ActivityMainBinding
import com.smartev.dashboard.model.VehicleSettings
import com.smartev.dashboard.ui.AntDeviceAdapter
import com.smartev.dashboard.ui.BmsCellAdapter
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private lateinit var bleManager: BleManager
    private lateinit var antBmsManager: AntBmsBleManager
    private val bmsAdapter = BmsCellAdapter()
    private var antScanDialog: AlertDialog? = null

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val allGranted = permissions.entries.all { it.value }
        if (allGranted) {
            bleManager.startScan()
        } else {
            Toast.makeText(this, "Cần cấp quyền Bluetooth để kết nối với xe!", Toast.LENGTH_SHORT).show()
        }
    }

    private var currentVotolBaud: Long = 9600

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        bleManager = BleManager.getInstance(this)
        antBmsManager = AntBmsBleManager.getInstance(this)

        setupUI()
        setupListeners()
        observeData()

        // Tự động kết nối lại Pin ANT BMS nếu đã lưu MAC trước đó
        antBmsManager.connectLastDevice()
    }

    private fun setupUI() {
        // Cấu hình Grid 4 cột cho 32 cell pin BMS
        binding.rvBmsCells.layoutManager = GridLayoutManager(this, 4)
        binding.rvBmsCells.adapter = bmsAdapter

        // Bottom Navigation switching
        binding.bottomNav.setOnItemSelectedListener { item ->
            binding.viewCockpit.visibility = View.GONE
            binding.viewBms.visibility = View.GONE
            binding.viewSettings.visibility = View.GONE
            binding.viewAuto.visibility = View.GONE

            when (item.itemId) {
                R.id.nav_cockpit -> binding.viewCockpit.visibility = View.VISIBLE
                R.id.nav_bms -> {
                    binding.viewBms.visibility = View.VISIBLE
                    bleManager.requestBmsUpdate()
                }
                R.id.nav_settings -> binding.viewSettings.visibility = View.VISIBLE
                R.id.nav_auto -> binding.viewAuto.visibility = View.VISIBLE
            }
            true
        }

        // Khởi tạo hệ thống 4 Trang Cài Đặt Votol
        setupVotolTabs()
    }

    private fun setupListeners() {
        // Nút Kết nối / Quét BLE
        binding.btnBleAction.setOnClickListener {
            when (bleManager.connectionState.value) {
                BleManager.BleState.CONNECTED -> bleManager.disconnect()
                BleManager.BleState.SCANNING -> bleManager.stopScan()
                BleManager.BleState.DISCONNECTED -> checkPermissionsAndConnect()
                BleManager.BleState.CONNECTING -> bleManager.disconnect()
            }
        }

        // Các nút Chẩn đoán UART Votol trực tiếp
        binding.btnVotolPoll.setOnClickListener {
            bleManager.sendVotolPoll()
            Toast.makeText(this, "Đã gửi lệnh SHOW thăm dò Votol!", Toast.LENGTH_SHORT).show()
        }

        binding.btnToggleBaud.setOnClickListener {
            val nextBaud = if (currentVotolBaud == 9600L) 115200L else 9600L
            bleManager.setVotolBaud(nextBaud)
            Toast.makeText(this, "Đã yêu cầu ESP32 đổi Baud sang $nextBaud bps", Toast.LENGTH_SHORT).show()
        }

        binding.btnSwapPins.setOnClickListener {
            bleManager.swapUartPins()
            Toast.makeText(this, "Đã yêu cầu ESP32 đảo chéo chân RX ⮂ TX!", Toast.LENGTH_SHORT).show()
        }

        // Chọn giao thức BMS (Pin)
        binding.rgBmsProtocol.setOnCheckedChangeListener { _, checkedId ->
            when (checkedId) {
                R.id.rbBmsBle -> {
                    binding.tvBmsProtocolDesc.text = "Đang chọn: Bluetooth BLE (Mặc định - Cách ly nhiễu và điện áp cao hoàn toàn)"
                    Toast.makeText(this, "Chế độ BMS: Bluetooth BLE", Toast.LENGTH_SHORT).show()
                }
                R.id.rbBmsCan -> {
                    binding.tvBmsProtocolDesc.text = "Đang chọn: Mạng CAN Bus (250 kbps - Cần mạch Transceiver SN65HVD230 nối chân CAN)"
                    Toast.makeText(this, "Chế độ BMS: Mạng CAN Bus (250 kbps)", Toast.LENGTH_SHORT).show()
                }
                R.id.rbBmsUart -> {
                    binding.tvBmsProtocolDesc.text = "Đang chọn: Dây UART (19200 baud) trên cặp chân chờ GPIO 16 RX / 15 TX"
                    Toast.makeText(this, "Chế độ BMS: Dây UART (GPIO 16/15)", Toast.LENGTH_SHORT).show()
                }
            }
        }

        // Nút Quét tìm kiếm Pin ANT BMS
        binding.btnScanAntBms.setOnClickListener {
            checkPermissionsAndScanAntBms()
        }

        // Nút Ngắt kết nối Pin ANT BMS
        binding.btnDisconnectAntBms.setOnClickListener {
            antBmsManager.disconnect()
            Toast.makeText(this, "Đã ngắt kết nối Pin ANT BMS!", Toast.LENGTH_SHORT).show()
        }

        // Nút cập nhật BMS
        binding.btnRefreshBms.setOnClickListener {
            bleManager.requestBmsUpdate()
            Toast.makeText(this, "Đang yêu cầu dữ liệu BMS...", Toast.LENGTH_SHORT).show()
        }

        // Nút Lưu cài đặt xe vào Flash NVS của ESP32-S3
        binding.btnSaveSettings.setOnClickListener {
            saveVehicleSettings()
        }

        // Nút Cấp quyền Notification cho Google Maps Turn-by-Turn
        binding.btnGrantNotification.setOnClickListener {
            startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))
        }

        // Nút mở trực tiếp Cài đặt Android Auto trên điện thoại
        binding.btnOpenAutoSettings.setOnClickListener {
            openAndroidAutoSettings()
        }
    }

    private fun showAntBmsScanDialog() {
        antScanDialog?.dismiss()

        val dialogView = layoutInflater.inflate(R.layout.dialog_scan_ant_bms, null)
        val rvDevices = dialogView.findViewById<RecyclerView>(R.id.rvDevices)
        val pbScanning = dialogView.findViewById<ProgressBar>(R.id.pbScanning)
        val tvEmpty = dialogView.findViewById<TextView>(R.id.tvEmptyDevices)
        val btnRescan = dialogView.findViewById<MaterialButton>(R.id.btnRescan)
        val btnClose = dialogView.findViewById<MaterialButton>(R.id.btnCloseDialog)

        val adapter = AntDeviceAdapter { device ->
            Toast.makeText(this, "Đang kết nối tới ${device.name}...", Toast.LENGTH_SHORT).show()
            antBmsManager.connectDevice(device.address)
            antScanDialog?.dismiss()
        }

        rvDevices.layoutManager = LinearLayoutManager(this)
        rvDevices.adapter = adapter

        val dialog = AlertDialog.Builder(this)
            .setView(dialogView)
            .setCancelable(true)
            .create()

        antScanDialog = dialog

        btnRescan.setOnClickListener {
            antBmsManager.startScan()
        }

        btnClose.setOnClickListener {
            antBmsManager.stopScan()
            dialog.dismiss()
        }

        dialog.setOnDismissListener {
            antBmsManager.stopScan()
        }

        // Lắng nghe danh sách thiết bị
        lifecycleScope.launch {
            antBmsManager.discoveredDevices.collectLatest { list ->
                if (list.isEmpty()) {
                    tvEmpty.visibility = View.VISIBLE
                    rvDevices.visibility = View.GONE
                } else {
                    tvEmpty.visibility = View.GONE
                    rvDevices.visibility = View.VISIBLE
                    adapter.updateDevices(list)
                }
            }
        }

        // Lắng nghe trạng thái quét
        lifecycleScope.launch {
            antBmsManager.connectionState.collectLatest { state ->
                pbScanning.visibility = if (state == AntBmsBleManager.AntBleState.SCANNING) View.VISIBLE else View.INVISIBLE
                btnRescan.isEnabled = (state != AntBmsBleManager.AntBleState.SCANNING)
            }
        }

        antBmsManager.startScan()
        dialog.show()
    }

    private fun checkPermissionsAndScanAntBms() {
        val permissions = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions.add(Manifest.permission.BLUETOOTH_SCAN)
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
            permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        } else {
            permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        val neededPermissions = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (neededPermissions.isEmpty()) {
            showAntBmsScanDialog()
        } else {
            requestPermissionLauncher.launch(neededPermissions.toTypedArray())
        }
    }

    private fun openAndroidAutoSettings() {
        val intent = Intent("com.google.android.gms.car.settings.PROJECTION_SETTINGS")
        try {
            startActivity(intent)
        } catch (e: Exception) {
            try {
                val gearheadIntent = Intent().apply {
                    component = android.content.ComponentName(
                        "com.google.android.projection.gearhead",
                        "com.google.android.projection.gearhead.settings.SettingsActivity"
                    )
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                }
                startActivity(gearheadIntent)
            } catch (e2: Exception) {
                try {
                    startActivity(Intent(Settings.ACTION_SETTINGS))
                    Toast.makeText(this, "Vui lòng tìm 'Android Auto' trong Cài đặt của máy!", Toast.LENGTH_LONG).show()
                } catch (e3: Exception) {
                    Toast.makeText(this, "Không thể mở cài đặt Android Auto", Toast.LENGTH_SHORT).show()
                }
            }
        }
    }

    private fun setupVotolTabs() {
        fun selectTab(pageIndex: Int) {
            binding.layoutVotolPage1.visibility = if (pageIndex == 1) View.VISIBLE else View.GONE
            binding.layoutVotolPage2.visibility = if (pageIndex == 2) View.VISIBLE else View.GONE
            binding.layoutVotolPage3.visibility = if (pageIndex == 3) View.VISIBLE else View.GONE
            binding.layoutVotolPage4.visibility = if (pageIndex == 4) View.VISIBLE else View.GONE

            val activeColor = getColor(R.color.ev_cyan)
            val inactiveColor = getColor(R.color.card_bg)
            val activeText = getColor(R.color.bg_dark)
            val inactiveText = getColor(R.color.text_gray)

            binding.btnTabVotol1.setBackgroundColor(if (pageIndex == 1) activeColor else inactiveColor)
            binding.btnTabVotol1.setTextColor(if (pageIndex == 1) activeText else inactiveText)

            binding.btnTabVotol2.setBackgroundColor(if (pageIndex == 2) activeColor else inactiveColor)
            binding.btnTabVotol2.setTextColor(if (pageIndex == 2) activeText else inactiveText)

            binding.btnTabVotol3.setBackgroundColor(if (pageIndex == 3) activeColor else inactiveColor)
            binding.btnTabVotol3.setTextColor(if (pageIndex == 3) activeText else inactiveText)

            binding.btnTabVotol4.setBackgroundColor(if (pageIndex == 4) activeColor else inactiveColor)
            binding.btnTabVotol4.setTextColor(if (pageIndex == 4) activeText else inactiveText)
        }

        binding.btnTabVotol1.setOnClickListener { selectTab(1) }
        binding.btnTabVotol2.setOnClickListener { selectTab(2) }
        binding.btnTabVotol3.setOnClickListener { selectTab(3) }
        binding.btnTabVotol4.setOnClickListener { selectTab(4) }

        selectTab(1)

        // Nút Đọc thông số từ IC
        binding.btnReadSettings.setOnClickListener {
            Toast.makeText(this, "📥 Đang đọc thông số từ IC Votol / ESP32 Flash...", Toast.LENGTH_SHORT).show()
            bleManager.sendCommand("CMD:READ_SETTINGS")
        }

        // Nút Khôi phục mặc định EM-150 / QS Motor
        binding.btnResetSettings.setOnClickListener {
            populateVotolUi(VehicleSettings())
            Toast.makeText(this, "🔄 Đã nạp thông số mặc định EM-150 / QS Motor!", Toast.LENGTH_SHORT).show()
        }
    }

    private fun populateVotolUi(s: VehicleSettings) {
        // Page 1: Basic & Throttle
        binding.etVotolModel.setText(s.votolModel)
        binding.etOvervoltage.setText(s.overvoltageV.toString())
        binding.etUndervoltage.setText(s.undervoltageV.toString())
        binding.etSoftUndervoltage.setText(s.softUndervoltageV.toString())
        binding.etUndervoltageVariation.setText(s.undervoltageVariationV.toString())
        binding.etBusCurrent.setText(s.busCurrentLimitA.toString())
        binding.etPhaseCurrent.setText(s.phaseCurrentLimitA.toString())
        binding.etThrottleLowProtect.setText(s.throttleLowProtectV.toString())
        binding.etThrottleStart.setText(s.throttleStartV.toString())
        binding.etThrottleEnd.setText(s.throttleEndV.toString())
        binding.etThrottleHighProtect.setText(s.throttleHighProtectV.toString())
        binding.etStartTorque.setText(s.startTorque.toString())
        binding.etCombinativeTorque.setText(s.combinativeTorque.toString())
        binding.etRateOfRise.setText(s.rateOfRise.toString())
        binding.etRateOfDecline.setText(s.rateOfDecline.toString())

        // Page 2: Modes & Speed
        binding.etSportCurrent.setText(s.sportCurrentLimitA.toString())
        binding.etSportFlux.setText(s.sportFluxWeakening.toString())
        binding.cbSportAutoLogout.isChecked = s.sportAutoLogout
        binding.etSportLogoutTime.setText(s.sportLogoutTimeS.toString())
        binding.etSportRecoveryTime.setText(s.sportRecoveryTimeS.toString())
        binding.etLowSpeedRatio.setText(s.lowSpeedRatio.toString())
        binding.etLowCurrentRatio.setText(s.lowCurrentRatio.toString())
        binding.etMidSpeedRatio.setText(s.midSpeedRatio.toString())
        binding.etMidCurrentRatio.setText(s.midCurrentRatio.toString())
        binding.etHighSpeedRatio.setText(s.highSpeedRatio.toString())
        binding.etHighCurrentRatio.setText(s.highCurrentRatio.toString())
        binding.rbButton3Speed.isChecked = (s.threeSpeedType == 0)
        binding.rbSwitch3Speed.isChecked = (s.threeSpeedType == 1)
        binding.cbSoftStart.isChecked = s.softStartEnable

        // Page 3: Motor & Sensors
        binding.etPolePairs.setText(s.polePairs.toString())
        binding.etHallShiftAngle.setText(s.hallShiftAngle.toString())
        binding.cbExchangeHall.isChecked = s.exchangeHallYellowBlue
        binding.cbExchangePhase.isChecked = s.exchangePhaseBlueGreen
        binding.rbSurfaceMount.isChecked = (s.motorType == 0)
        binding.rbVType.isChecked = (s.motorType == 1)
        binding.etReverseSpeedLimit.setText(s.reverseSpeedLimitRatio.toString())
        binding.etEbsRatio.setText(s.ebsRatio.toString())
        binding.cbLowBrake.isChecked = s.lowBrakeEnable
        binding.cbSecureBoot.isChecked = s.secureBoot
        binding.cbCruise.isChecked = s.cruiseControl

        // Page 4: Vehicle Specs & OLED
        binding.etTireSize.setText(s.tireCircumferenceMm.toString())
        binding.etGearRatio.setText(s.gearRatio.toString())
        binding.etOledBrightness.setText(s.oledBrightness.toString())
    }

    private fun collectVotolUi(): VehicleSettings {
        return VehicleSettings(
            // Page 1
            votolModel = binding.etVotolModel.text?.toString()?.ifEmpty { "EM-150" } ?: "EM-150",
            overvoltageV = binding.etOvervoltage.text?.toString()?.toFloatOrNull() ?: 88.0f,
            undervoltageV = binding.etUndervoltage.text?.toString()?.toFloatOrNull() ?: 60.0f,
            softUndervoltageV = binding.etSoftUndervoltage.text?.toString()?.toFloatOrNull() ?: 62.0f,
            undervoltageVariationV = binding.etUndervoltageVariation.text?.toString()?.toFloatOrNull() ?: 2.0f,
            busCurrentLimitA = binding.etBusCurrent.text?.toString()?.toIntOrNull() ?: 150,
            phaseCurrentLimitA = binding.etPhaseCurrent.text?.toString()?.toIntOrNull() ?: 350,
            throttleLowProtectV = binding.etThrottleLowProtect.text?.toString()?.toFloatOrNull() ?: 0.8f,
            throttleStartV = binding.etThrottleStart.text?.toString()?.toFloatOrNull() ?: 1.15f,
            throttleEndV = binding.etThrottleEnd.text?.toString()?.toFloatOrNull() ?: 3.80f,
            throttleHighProtectV = binding.etThrottleHighProtect.text?.toString()?.toFloatOrNull() ?: 4.5f,
            startTorque = binding.etStartTorque.text?.toString()?.toIntOrNull() ?: 50,
            combinativeTorque = binding.etCombinativeTorque.text?.toString()?.toIntOrNull() ?: 80,
            rateOfRise = binding.etRateOfRise.text?.toString()?.toIntOrNull() ?: 10,
            rateOfDecline = binding.etRateOfDecline.text?.toString()?.toIntOrNull() ?: 15,

            // Page 2
            sportCurrentLimitA = binding.etSportCurrent.text?.toString()?.toIntOrNull() ?: 180,
            sportFluxWeakening = binding.etSportFlux.text?.toString()?.toIntOrNull() ?: 50,
            sportAutoLogout = binding.cbSportAutoLogout.isChecked,
            sportLogoutTimeS = binding.etSportLogoutTime.text?.toString()?.toIntOrNull() ?: 15,
            sportRecoveryTimeS = binding.etSportRecoveryTime.text?.toString()?.toIntOrNull() ?: 10,
            lowSpeedRatio = binding.etLowSpeedRatio.text?.toString()?.toIntOrNull() ?: 45,
            lowCurrentRatio = binding.etLowCurrentRatio.text?.toString()?.toIntOrNull() ?: 50,
            midSpeedRatio = binding.etMidSpeedRatio.text?.toString()?.toIntOrNull() ?: 75,
            midCurrentRatio = binding.etMidCurrentRatio.text?.toString()?.toIntOrNull() ?: 75,
            highSpeedRatio = binding.etHighSpeedRatio.text?.toString()?.toIntOrNull() ?: 100,
            highCurrentRatio = binding.etHighCurrentRatio.text?.toString()?.toIntOrNull() ?: 100,
            threeSpeedType = if (binding.rbButton3Speed.isChecked) 0 else 1,
            softStartEnable = binding.cbSoftStart.isChecked,

            // Page 3
            polePairs = binding.etPolePairs.text?.toString()?.toIntOrNull() ?: 5,
            hallShiftAngle = binding.etHallShiftAngle.text?.toString()?.toIntOrNull() ?: -60,
            exchangeHallYellowBlue = binding.cbExchangeHall.isChecked,
            exchangePhaseBlueGreen = binding.cbExchangePhase.isChecked,
            motorType = if (binding.rbSurfaceMount.isChecked) 0 else 1,
            reverseSpeedLimitRatio = binding.etReverseSpeedLimit.text?.toString()?.toIntOrNull() ?: 25,
            ebsRatio = binding.etEbsRatio.text?.toString()?.toIntOrNull() ?: 30,
            lowBrakeEnable = binding.cbLowBrake.isChecked,
            secureBoot = binding.cbSecureBoot.isChecked,
            cruiseControl = binding.cbCruise.isChecked,

            // Page 4
            tireCircumferenceMm = binding.etTireSize.text?.toString()?.toIntOrNull() ?: 472,
            gearRatio = binding.etGearRatio.text?.toString()?.toFloatOrNull() ?: 1.0f,
            oledBrightness = binding.etOledBrightness.text?.toString()?.toIntOrNull() ?: 100
        )
    }

    private fun saveVehicleSettings() {
        val settings = collectVotolUi()
        bleManager.sendVehicleSettings(settings)
        Toast.makeText(this, "💾 Đã gửi và lưu toàn bộ 4 trang thông số Votol xuống IC!", Toast.LENGTH_LONG).show()
    }

    private fun observeData() {
        // Lắng nghe trạng thái BLE
        lifecycleScope.launch {
            bleManager.connectionState.collectLatest { state ->
                when (state) {
                    BleManager.BleState.DISCONNECTED -> {
                        binding.tvBleStatus.text = getString(R.string.status_disconnected)
                        binding.tvBleStatus.setTextColor(getColor(R.color.ev_red))
                        binding.btnBleAction.text = getString(R.string.btn_connect)
                    }
                    BleManager.BleState.SCANNING -> {
                        binding.tvBleStatus.text = getString(R.string.status_scanning)
                        binding.tvBleStatus.setTextColor(getColor(R.color.ev_orange))
                        binding.btnBleAction.text = "DỪNG TÌM"
                    }
                    BleManager.BleState.CONNECTING -> {
                        binding.tvBleStatus.text = "ĐANG KẾT NỐI XE..."
                        binding.tvBleStatus.setTextColor(getColor(R.color.ev_cyan))
                        binding.btnBleAction.text = "HỦY"
                    }
                    BleManager.BleState.CONNECTED -> {
                        binding.tvBleStatus.text = getString(R.string.status_connected)
                        binding.tvBleStatus.setTextColor(getColor(R.color.ev_green))
                        binding.btnBleAction.text = getString(R.string.btn_disconnect)
                    }
                }
            }
        }

        // Lắng nghe Telemetry (Tốc độ, Pin, Đèn, Xi-nhan)
        lifecycleScope.launch {
            bleManager.telemetry.collectLatest { t ->
                binding.tvSpeed.text = String.format("%.1f", t.speedKmh)
                val statusExtras = mutableListOf<String>()
                if (t.parked == 1) statusExtras.add("P")
                if (t.reverse == 1) statusExtras.add("R")
                if (t.brake == 1) statusExtras.add("PHANH")
                if (t.sideStand == 1) statusExtras.add("CHỐNG")
                if (t.regen == 1) statusExtras.add("REGEN")
                val gearDisplay = if (statusExtras.isNotEmpty()) "${t.gear} (${statusExtras.joinToString(",")})" else t.gear
                binding.tvGear.text = gearDisplay
                binding.tvSoc.text = "${t.soc}%"
                binding.pbSoc.progress = t.soc
                binding.tvVoltage.text = String.format("%.1f V", t.voltage)
                binding.tvCurrent.text = String.format("%.1f A", t.current)

                val powerKw = t.powerW / 1000f
                binding.tvPower.text = String.format("%.2f kW", powerKw)

                binding.tvMotorTemp.text = "Nhiệt động cơ: ${t.motorTemp}°C"
                binding.tvControllerTemp.text = "Nhiệt IC Votol: ${t.controllerTemp}°C"

                // Cảnh báo lỗi Votol
                if (t.errorCode != 0) {
                    binding.tvVotolError.text = "CẢNH BÁO LỖI: ${t.errorMsg ?: "Mã #${t.errorCode}"}"
                    binding.tvVotolError.setTextColor(getColor(R.color.ev_red))
                } else {
                    binding.tvVotolError.text = "Trạng thái xe: BÌNH THƯỜNG"
                    binding.tvVotolError.setTextColor(getColor(R.color.ev_green))
                }

                // Cập nhật Đèn & Xi-nhan Realtime
                binding.indicatorTurnLeft.setBackgroundResource(
                    if (t.turnLeft == 1) R.drawable.bg_signal_turn else R.drawable.bg_signal_off
                )
                binding.indicatorTurnRight.setBackgroundResource(
                    if (t.turnRight == 1) R.drawable.bg_signal_turn else R.drawable.bg_signal_off
                )
                binding.indicatorHighBeam.setBackgroundResource(
                    if (t.highBeam == 1) R.drawable.bg_signal_beam else R.drawable.bg_signal_off
                )

                // Cập nhật Bảng Chẩn đoán UART Votol Live
                currentVotolBaud = t.currentBaud
                binding.tvUartStats.text = "TX Đã gửi: ${t.votolTxBytes} Bytes | RX Đã nhận: ${t.votolRxBytes} Bytes"
                val swapStr = if (t.pinsSwapped == 1) "ĐÃ ĐẢO (RX17/TX18)" else "CHUẨN (RX18/TX17)"
                binding.tvUartBaud.text = "Baud: ${t.currentBaud} bps | Chân: $swapStr"
                binding.tvUartHex.text = "Chuỗi Hex nhận gần nhất: [ ${t.votolLastHex} ]"
            }
        }

        // Lắng nghe BMS 32 Cell
        lifecycleScope.launch {
            bleManager.bmsData.collectLatest { bms ->
                binding.tvBmsSummary.text = String.format(
                    "Dung lượng: %d%% | Tổng: %.1f V | Dòng: %.1f A",
                    bms.soc, bms.voltage, bms.current
                )
                binding.tvBmsDelta.text = "Chênh lệch Delta: ${bms.deltaMv} mV"
                binding.tvBmsMinMax.text = String.format(
                    "Min: #%d (%d mV) | Max: #%d (%d mV)",
                    bms.cellMinIndex, bms.cellMinVoltage, bms.cellMaxIndex, bms.cellMaxVoltage
                )
                binding.tvBmsTemp.text = "Nhiệt độ Pin: T1: ${bms.temp1}°C | T2: ${bms.temp2}°C"

                if (bms.cells.isNotEmpty()) {
                    bmsAdapter.updateCells(bms.cells, bms.cellMinIndex, bms.cellMaxIndex)
                }
            }
        }

        // Lắng nghe trạng thái kết nối Pin ANT BMS
        lifecycleScope.launch {
            antBmsManager.connectionState.collectLatest { state ->
                when (state) {
                    AntBmsBleManager.AntBleState.DISCONNECTED -> {
                        binding.tvAntBleStatus.text = "Trạng thái: Chưa kết nối pin ANT"
                        binding.tvAntBleStatus.setTextColor(getColor(R.color.ev_red))
                        binding.btnScanAntBms.text = "🔍 TÌM KIẾM BLE PIN ANT"
                    }
                    AntBmsBleManager.AntBleState.SCANNING -> {
                        binding.tvAntBleStatus.text = "Trạng thái: Đang tìm kiếm sóng Pin ANT..."
                        binding.tvAntBleStatus.setTextColor(getColor(R.color.ev_orange))
                        binding.btnScanAntBms.text = "DỪNG TÌM"
                    }
                    AntBmsBleManager.AntBleState.CONNECTING -> {
                        binding.tvAntBleStatus.text = "Trạng thái: Đang kết nối Pin ANT..."
                        binding.tvAntBleStatus.setTextColor(getColor(R.color.ev_cyan))
                        binding.btnScanAntBms.text = "HỦY"
                    }
                    AntBmsBleManager.AntBleState.CONNECTED -> {
                        binding.tvAntBleStatus.text = "Trạng thái: ĐÃ KẾT NỐI PIN ANT THÀNH CÔNG"
                        binding.tvAntBleStatus.setTextColor(getColor(R.color.ev_green))
                        binding.btnScanAntBms.text = "ĐÃ KẾT NỐI"
                    }
                }
            }
        }

        // Lắng nghe tên thiết bị Pin ANT BMS
        lifecycleScope.launch {
            antBmsManager.connectedDeviceName.collectLatest { name ->
                if (!name.isNullOrEmpty()) {
                    binding.tvAntBleDeviceName.text = "Thiết bị: $name"
                    binding.tvAntBleDeviceName.setTextColor(getColor(R.color.text_white))
                } else {
                    val saved = antBmsManager.getLastSavedDevice()
                    if (saved.first != null) {
                        binding.tvAntBleDeviceName.text = "Thiết bị đã lưu: ${saved.second ?: saved.first}"
                    } else {
                        binding.tvAntBleDeviceName.text = "Thiết bị: Chưa chọn thiết bị pin"
                    }
                    binding.tvAntBleDeviceName.setTextColor(getColor(R.color.text_gray))
                }
            }
        }

        // Lắng nghe dữ liệu Pin ANT BMS qua BLE
        lifecycleScope.launch {
            antBmsManager.bmsData.collectLatest { bms ->
                if (bms.cells.isNotEmpty() || bms.voltage > 10f) {
                    binding.tvBmsSummary.text = String.format(
                        "Dung lượng: %d%% | Tổng: %.1f V | Dòng: %.1f A",
                        bms.soc, bms.voltage, bms.current
                    )
                    binding.tvBmsDelta.text = "Chênh lệch Delta: ${bms.deltaMv} mV"
                    binding.tvBmsMinMax.text = String.format(
                        "Min: #%d (%d mV) | Max: #%d (%d mV)",
                        bms.cellMinIndex, bms.cellMinVoltage, bms.cellMaxIndex, bms.cellMaxVoltage
                    )
                    binding.tvBmsTemp.text = "Nhiệt độ Pin: T1: ${bms.temp1}°C | T2: ${bms.temp2}°C"

                    if (bms.cells.isNotEmpty()) {
                        bmsAdapter.updateCells(bms.cells, bms.cellMinIndex, bms.cellMaxIndex)
                    }
                }
            }
        }
    }

    private fun checkPermissionsAndConnect() {
        val permissions = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions.add(Manifest.permission.BLUETOOTH_SCAN)
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
            permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        } else {
            permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissions.add(Manifest.permission.POST_NOTIFICATIONS)
        }

        val neededPermissions = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (neededPermissions.isEmpty()) {
            bleManager.startScan()
        } else {
            requestPermissionLauncher.launch(neededPermissions.toTypedArray())
        }
    }
}
