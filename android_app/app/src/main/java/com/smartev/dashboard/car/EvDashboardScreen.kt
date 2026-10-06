package com.smartev.dashboard.car

import androidx.car.app.CarContext
import androidx.car.app.Screen
import androidx.car.app.model.*
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import com.smartev.dashboard.ble.BleManager
import com.smartev.dashboard.model.VehicleTelemetry
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/**
 * Màn hình Digital Cockpit xe điện hiển thị trực tiếp trên Android Auto
 */
class EvDashboardScreen(carContext: CarContext) : Screen(carContext), DefaultLifecycleObserver {

    private val bleManager = BleManager.getInstance(carContext)
    private var telemetry = VehicleTelemetry()
    private var bleState = BleManager.BleState.DISCONNECTED

    init {
        lifecycle.addObserver(this)
    }

    override fun onCreate(owner: LifecycleOwner) {
        // Lắng nghe dữ liệu Telemetry từ ESP32 gửi lên
        lifecycleScope.launch {
            bleManager.telemetry.collectLatest { data ->
                telemetry = data
                invalidate()
            }
        }
        lifecycleScope.launch {
            bleManager.connectionState.collectLatest { state ->
                bleState = state
                invalidate()
            }
        }
    }

    override fun onGetTemplate(): Template {
        val paneBuilder = Pane.Builder()

        // Hàng 1: Tốc độ xe & Cấp số & Cảm biến
        val speedText = String.format("%.1f km/h", telemetry.speedKmh)
        val statusBadges = mutableListOf<String>()
        if (telemetry.parked == 1) statusBadges.add("🅿️ P")
        if (telemetry.reverse == 1) statusBadges.add("🔙 R")
        if (telemetry.brake == 1) statusBadges.add("🛑 PHANH")
        if (telemetry.sideStand == 1) statusBadges.add("⚠️ CHÂN CHỐNG")
        if (telemetry.regen == 1) statusBadges.add("⚡ REGEN")
        val badgeStr = if (statusBadges.isNotEmpty()) " | " + statusBadges.joinToString(" • ") else ""
        val gearStatus = "Cấp số: [${telemetry.gear}]$badgeStr"

        val speedRow = Row.Builder()
            .setTitle("VẬN TỐC HIỆN TẠI: $speedText")
            .addText(gearStatus)
            .build()
        paneBuilder.addRow(speedRow)

        // Hàng 2: Pin & Nguồn điện
        val batteryInfo = String.format("Pin: %d%% | %.1f V | %.1f A", telemetry.soc, telemetry.voltage, telemetry.current)
        val powerKw = telemetry.powerW / 1000f
        val powerInfo = String.format("Công suất tiêu thụ: %.2f kW", powerKw)
        val batteryRow = Row.Builder()
            .setTitle(batteryInfo)
            .addText(powerInfo)
            .build()
        paneBuilder.addRow(batteryRow)

        // Hàng 3: Tín hiệu nút bấm & Nhiệt độ
        val turnStatus = when {
            telemetry.turnLeft == 1 && telemetry.turnRight == 1 -> "⚠️ CẢNH BÁO HAZARD"
            telemetry.turnLeft == 1 -> "⬅️ XI-NHAN TRÁI"
            telemetry.turnRight == 1 -> "➡️ XI-NHAN PHẢI"
            else -> "Xi-nhan: Tắt"
        }
        val beamStatus = if (telemetry.highBeam == 1) "Đèn PHA [BẬT]" else "Đèn Cos"
        val tempInfo = "Động cơ: ${telemetry.motorTemp}°C | IC: ${telemetry.controllerTemp}°C"
        val signalRow = Row.Builder()
            .setTitle("$turnStatus | $beamStatus")
            .addText(tempInfo)
            .build()
        paneBuilder.addRow(signalRow)

        // Hàng 4: Cảnh báo lỗi Votol (nếu có)
        if (telemetry.errorCode != 0) {
            val errRow = Row.Builder()
                .setTitle("CẢNH BÁO LỖI: ${telemetry.errorMsg ?: "Mã #${telemetry.errorCode}"}")
                .addText("Vui lòng kiểm tra động cơ hoặc IC điều tốc!")
                .build()
            paneBuilder.addRow(errRow)
        }

        // Action Buttons dưới cùng
        val actionStripBuilder = ActionStrip.Builder()
            .addAction(
                Action.Builder()
                    .setTitle("Chi tiết BMS")
                    .setOnClickListener {
                        screenManager.push(EvBmsScreen(carContext))
                    }
                    .build()
            )
            .addAction(
                Action.Builder()
                    .setTitle(if (bleState == BleManager.BleState.CONNECTED) "Đã nối xe" else "Quét BLE")
                    .setOnClickListener {
                        if (bleState != BleManager.BleState.CONNECTED) {
                            bleManager.startScan()
                        }
                    }
                    .build()
            )

        val headerAction = Action.APP_ICON

        return PaneTemplate.Builder(paneBuilder.build())
            .setTitle("SMART EV COCKPIT")
            .setHeaderAction(headerAction)
            .setActionStrip(actionStripBuilder.build())
            .build()
    }
}
