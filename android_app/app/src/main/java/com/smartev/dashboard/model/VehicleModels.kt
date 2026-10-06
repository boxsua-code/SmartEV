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
 * Cài đặt chuyên sâu thông số IC Votol chuẩn VOTOL-EM-V3 & VotolAIO (4 Trang)
 */
data class VehicleSettings(
    // PAGE 1: Basic & Power Settings
    var votolModel: String = "EM-150",
    var batteryNominalVoltage: Int = 72,
    var overvoltageV: Float = 88.0f,
    var undervoltageV: Float = 60.0f,
    var softUndervoltageV: Float = 62.0f,
    var undervoltageVariationV: Float = 2.0f,
    var busCurrentLimitA: Int = 150,
    var phaseCurrentLimitA: Int = 350,
    var throttleLowProtectV: Float = 0.8f,
    var throttleStartV: Float = 1.15f,
    var throttleEndV: Float = 3.80f,
    var throttleHighProtectV: Float = 4.5f,
    var startTorque: Int = 50,
    var combinativeTorque: Int = 80,
    var rateOfRise: Int = 10,
    var rateOfDecline: Int = 15,

    // PAGE 2: Modes & Speed Settings
    var sportCurrentLimitA: Int = 180,
    var sportFluxWeakening: Int = 50,
    var sportAutoLogout: Boolean = true,
    var sportLogoutTimeS: Int = 15,
    var sportRecoveryTimeS: Int = 10,
    var hhcEnable: Boolean = false,
    var hdcEnable: Boolean = false,
    var hdcLowestSpeed: Int = 15,
    var speedLimitEnable: Boolean = false,
    var speedLimitRatio: Int = 100,
    var fluxWeakeningCompensation: Int = 0,
    var lowSpeedRatio: Int = 45,
    var lowCurrentRatio: Int = 50,
    var midSpeedRatio: Int = 75,
    var midCurrentRatio: Int = 75,
    var highSpeedRatio: Int = 100,
    var highCurrentRatio: Int = 100,
    var midFluxWeakening: Int = 0,
    var highFluxWeakening: Int = 30,
    var threeSpeedType: Int = 0, // 0: Button, 1: Switch
    var defaultGear: Int = 1,     // 0: Low, 1: Mid, 2: High
    var softStartEnable: Boolean = true,
    var softStartGrade: Int = 1,

    // PAGE 3: Motor & Sensor Settings
    var polePairs: Int = 5,
    var exchangeHallYellowBlue: Boolean = false,
    var exchangePhaseBlueGreen: Boolean = false,
    var motorType: Int = 0, // 0: Surface-mount, 1: V-type
    var hallShiftAngle: Int = -60,
    var reverseSpeedLimitRatio: Int = 25,
    var ebsRatio: Int = 30,
    var lowBrakeEnable: Boolean = true,
    var secureBoot: Boolean = true,
    var speedometerType: Int = 0, // 0: One-Lin, 1: Hall
    var movingVehicleBooster: Boolean = false,
    var boosterSpeedRatio: Int = 15,
    var boosterTorque: Int = 20,
    var cruiseControl: Boolean = false,
    var doubleVoltageAutoId: Boolean = false,

    // PAGE 4: Ports & Vehicle Specs
    var tireCircumferenceMm: Int = 472,
    var gearRatio: Float = 1.0f,
    var oledBrightness: Int = 100,
    var regenBrakeLevel: Int = 1,
    var driveMode: Int = 1
)

/**
 * Thông tin Turn-by-Turn từ Google Maps
 */
data class NavTurnInfo(
    val iconType: String,      // "STRAIGHT", "LEFT", "RIGHT", "SLIGHT_LEFT", "SLIGHT_RIGHT", "UTURN", "DEST"
    val distanceText: String,  // "150m", "1.2km"
    val streetName: String     // "Đường Nguyễn Huệ"
)
