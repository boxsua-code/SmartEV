package com.smartev.dashboard.model

import com.google.gson.annotations.SerializedName

/**
 * Dữ liệu Telemetry trực tiếp từ ESP32-S3 (Votol IC + Tín hiệu đèn/xi-nhan)
 */
data class VehicleTelemetry(
    @SerializedName("spd") val speedKmh: Float = 0f,
    @SerializedName("gear") val gear: String = "P",
    @SerializedName("v") val voltage: Float = 0f,
    @SerializedName("a") val current: Float = 0f,
    @SerializedName("soc") val soc: Int = 0,
    @SerializedName("p") val powerW: Int = 0,
    @SerializedName("temp_m") val motorTemp: Int = 0,
    @SerializedName("temp_c") val controllerTemp: Int = 0,
    @SerializedName("turn_l") val turnLeft: Int = 0,
    @SerializedName("turn_r") val turnRight: Int = 0,
    @SerializedName("beam") val highBeam: Int = 0,
    @SerializedName("err") val errorCode: Int = 0,
    @SerializedName("err_msg") val errorMsg: String? = null,
    // Tín hiệu cảm biến nút bấm & IC Votol
    @SerializedName("brk") val brake: Int = 0,
    @SerializedName("stand") val sideStand: Int = 0,
    @SerializedName("rgn") val regen: Int = 0,
    @SerializedName("rev") val reverse: Int = 0,
    @SerializedName("park") val parked: Int = 0,
    // Thông số Chẩn đoán UART Votol trực tiếp
    @SerializedName("v_tx") val votolTxBytes: Long = 0,
    @SerializedName("v_rx") val votolRxBytes: Long = 0,
    @SerializedName("v_hex") val votolLastHex: String = "--",
    @SerializedName("baud") val currentBaud: Long = 9600,
    @SerializedName("swap") val pinsSwapped: Int = 0
)

/**
 * Dữ liệu ANT BMS 32 Cell Pin
 */
data class BmsData(
    @SerializedName("type") val type: String = "bms",
    @SerializedName("soc") val soc: Int = 0,
    @SerializedName("v") val voltage: Float = 0f,
    @SerializedName("a") val current: Float = 0f,
    @SerializedName("t1") val temp1: Int = 0,
    @SerializedName("t2") val temp2: Int = 0,
    @SerializedName("cells") val cells: List<Int> = emptyList(),
    @SerializedName("c_min") val cellMinIndex: Int = 0,
    @SerializedName("v_min") val cellMinVoltage: Int = 0,
    @SerializedName("c_max") val cellMaxIndex: Int = 0,
    @SerializedName("v_max") val cellMaxVoltage: Int = 0,
    @SerializedName("delta") val deltaMv: Int = 0,
    @SerializedName("ah") val remainingAh: Float = 0f
)

/**
 * Cài đặt chuyên sâu xe điện lưu trong ESP32 NVS Flash
 */
data class VehicleSettings(
    var tireCircumferenceMm: Int = 472, // Lốp 120/70-12 (~472mm bán kính/chu vi tùy chỉnh)
    var polePairs: Int = 5,             // Số cặp cực động cơ QS / Yuma
    var busCurrentLimitA: Int = 150,    // Giới hạn dòng bình ắc quy / pin
    var phaseCurrentLimitA: Int = 350,  // Giới hạn dòng pha cực đại
    var regenBrakeLevel: Int = 1,       // 0: Tắt, 1: Nhẹ, 2: Vừa, 3: Mạnh
    var speedLimitMode1: Int = 45,      // % giới hạn số 1 (Eco)
    var speedLimitMode2: Int = 75,      // % giới hạn số 2 (Normal)
    var speedLimitMode3: Int = 100,     // % giới hạn số 3 (Sport)
    var driveMode: Int = 1              // 0: Eco, 1: Normal, 2: Sport
)

/**
 * Thông tin Turn-by-Turn từ Google Maps
 */
data class NavTurnInfo(
    val iconType: String,      // "STRAIGHT", "LEFT", "RIGHT", "SLIGHT_LEFT", "SLIGHT_RIGHT", "UTURN", "DEST"
    val distanceText: String,  // "150m", "1.2km"
    val streetName: String     // "Đường Nguyễn Huệ"
)
