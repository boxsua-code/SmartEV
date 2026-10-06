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
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.GridLayoutManager
import com.smartev.dashboard.ble.BleManager
import com.smartev.dashboard.databinding.ActivityMainBinding
import com.smartev.dashboard.model.VehicleSettings
import com.smartev.dashboard.ui.BmsCellAdapter
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private lateinit var bleManager: BleManager
    private val bmsAdapter = BmsCellAdapter()

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

        setupUI()
        setupListeners()
        observeData()
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
    }

    private fun saveVehicleSettings() {
        val tire = binding.etTireSize.text.toString().toIntOrNull() ?: 472
        val poles = binding.etPolePairs.text.toString().toIntOrNull() ?: 5
        val busA = binding.etBusCurrent.text.toString().toIntOrNull() ?: 150
        val phaseA = binding.etPhaseCurrent.text.toString().toIntOrNull() ?: 350

        val regen = when {
            binding.rbRegen0.isChecked -> 0
            binding.rbRegen1.isChecked -> 1
            binding.rbRegen2.isChecked -> 2
            binding.rbRegen3.isChecked -> 3
            else -> 1
        }

        val mode = when {
            binding.rbModeEco.isChecked -> 0
            binding.rbModeNormal.isChecked -> 1
            binding.rbModeSport.isChecked -> 2
            else -> 1
        }

        val settings = VehicleSettings(
            tireCircumferenceMm = tire,
            polePairs = poles,
            busCurrentLimitA = busA,
            phaseCurrentLimitA = phaseA,
            regenBrakeLevel = regen,
            driveMode = mode
        )

        bleManager.sendVehicleSettings(settings)
        Toast.makeText(this, getString(R.string.msg_settings_saved), Toast.LENGTH_LONG).show()
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
    }

    private fun checkPermissionsAndConnect() {
        val permissions = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions.add(Manifest.permission.BLUETOOTH_SCAN)
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
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
