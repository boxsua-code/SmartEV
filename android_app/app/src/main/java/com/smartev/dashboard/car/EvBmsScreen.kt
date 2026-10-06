package com.smartev.dashboard.car

import androidx.car.app.CarContext
import androidx.car.app.Screen
import androidx.car.app.model.*
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import com.smartev.dashboard.ble.BleManager
import com.smartev.dashboard.model.BmsData
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/**
 * Màn hình giám sát ANT BMS 32 Cell trên Android Auto
 */
class EvBmsScreen(carContext: CarContext) : Screen(carContext), DefaultLifecycleObserver {

    private val bleManager = BleManager.getInstance(carContext)
    private var bms = BmsData()

    init {
        lifecycle.addObserver(this)
    }

    override fun onCreate(owner: LifecycleOwner) {
        lifecycleScope.launch {
            bleManager.bmsData.collectLatest { data ->
                bms = data
                invalidate()
            }
        }
        bleManager.requestBmsUpdate()
    }

    override fun onGetTemplate(): Template {
        val paneBuilder = Pane.Builder()

        // Hàng 1: Tổng quan Pin
        val overall = String.format("Pin: %d%% | %.1f V | Dòng: %.1f A", bms.soc, bms.voltage, bms.current)
        val temp = "Nhiệt độ Cell: T1: ${bms.temp1}°C | T2: ${bms.temp2}°C"
        paneBuilder.addRow(
            Row.Builder()
                .setTitle(overall)
                .addText(temp)
                .build()
        )

        // Hàng 2: Cân bằng Cell & Chênh lệch (Delta)
        val deltaText = "Độ lệch điện áp (Delta): ${bms.deltaMv} mV"
        val balanceStatus = if (bms.deltaMv <= 25) "Pin cân bằng TỐT" else "Pin lệch áp, cần sạc cân bằng!"
        paneBuilder.addRow(
            Row.Builder()
                .setTitle(deltaText)
                .addText(balanceStatus)
                .build()
        )

        // Hàng 3: Cell Min & Cell Max
        val minMax = "Cell #${bms.cellMinIndex}: ${bms.cellMinVoltage} mV | Cell #${bms.cellMaxIndex}: ${bms.cellMaxVoltage} mV"
        paneBuilder.addRow(
            Row.Builder()
                .setTitle("Cặp Cell Cực trị:")
                .addText(minMax)
                .build()
        )

        // Hàng 4: Thống kê số cell
        val cellCount = if (bms.cells.isNotEmpty()) "${bms.cells.size} Cells đang hoạt động" else "Đang chờ gói tin BMS..."
        paneBuilder.addRow(
            Row.Builder()
                .setTitle("Cấu hình chuỗi:")
                .addText(cellCount)
                .build()
        )

        val actionStrip = ActionStrip.Builder()
            .addAction(
                Action.Builder()
                    .setTitle("Làm mới")
                    .setOnClickListener {
                        bleManager.requestBmsUpdate()
                    }
                    .build()
            )
            .build()

        return PaneTemplate.Builder(paneBuilder.build())
            .setTitle("GIÁM SÁT PIN BMS ANT")
            .setHeaderAction(Action.BACK)
            .setActionStrip(actionStrip)
            .build()
    }
}
