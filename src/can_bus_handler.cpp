#include "can_bus_handler.h"

CanBusHandler::CanBusHandler()
    : _initialized(false),
      _isConnected(false),
      _messageCount(0),
      _lastMessageMs(0),
      _txPin(PIN_CAN_TX),
      _rxPin(PIN_CAN_RX)
{
}

CanBusHandler::~CanBusHandler() {
    if (_initialized) {
        twai_stop();
        twai_driver_uninstall();
        _initialized = false;
    }
}

bool CanBusHandler::begin(int8_t txPin, int8_t rxPin, uint32_t baud) {
    _txPin = txPin;
    _rxPin = rxPin;

    twai_general_config_t g_config = {
        .mode = TWAI_MODE_LISTEN_ONLY, // Thụ động, an toàn tuyệt đối
        .tx_io = (gpio_num_t)_txPin,
        .rx_io = (gpio_num_t)_rxPin,
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 0,
        .rx_queue_len = 20,
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider = 0
    };

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        Serial.printf("[CAN] ❌ Không thể cài đặt TWAI driver (TX: %d, RX: %d)\n", _txPin, _rxPin);
        return false;
    }

    if (twai_start() != ESP_OK) {
        Serial.println("[CAN] ❌ Không thể khởi động TWAI");
        return false;
    }

    _initialized = true;
    Serial.printf("[CAN] ✅ Đã kích hoạt CAN TWAI (250kbps, LISTEN_ONLY, TX: GPIO %d, RX: GPIO %d) [Chuẩn JAMFOXRS]\n",
                  _txPin, _rxPin);
    return true;
}

void CanBusHandler::update(VotolData &vd, ANTBMSData &bd) {
    if (!_initialized) return;

    twai_message_t msg;
    while (twai_receive(&msg, pdMS_TO_TICKS(2)) == ESP_OK) {
        _messageCount++;
        _lastMessageMs = millis();
        _isConnected = true;

        uint32_t id = msg.identifier;

        // 1. Gói tin điều khiển động cơ: 0x0A010810 (Chuẩn JAMFOXRS)
        if (id == 0x0A010810 && msg.data_length_code >= 8) {
            uint8_t m = msg.data[1];
            int16_t rpmVal = (int16_t)(msg.data[2] | (msg.data[3] << 8));
            float spd = (float)rpmVal * 0.1033f; // Quy đổi vận tốc
            if (spd < 0.0f) spd = 0.0f;

            vd.rpm = rpmVal;
            vd.speedKmh = spd;
            vd.controllerTemp = msg.data[4];
            vd.motorTemp = msg.data[5];

            // Giải mã Cấp số và Nút bấm xe từ mode byte (Chuẩn JAMFOXRS)
            vd.parked = (m == 0x00);
            vd.reverse = (m == 0x50 || m == 0xF0 || m == 0x30 || m == 0xF8);
            vd.brake = (m == 0x72 || m == 0xB2);
            vd.sideStand = (m == 0x78 || m == 0x08 || m == 0xB8);

            if (vd.parked) {
                vd.gear = VotolGear::GEAR_PARK;
            } else if (vd.reverse) {
                vd.gear = VotolGear::GEAR_REVERSE;
            } else if (m == 0xB0 || m == 0xB2 || m == 0xB4 || m == 0xB8) {
                vd.gear = VotolGear::GEAR_SPORT;
            } else {
                vd.gear = VotolGear::GEAR_DRIVE;
            }

            vd.isConnected = true;
            vd.lastReceivedMs = millis();
            vd.validPacketsCount++;
        }

        // 2. Gói tin Điện áp & Dòng điện: 0x0A6D0D09 (Chuẩn JAMFOXRS)
        else if (id == 0x0A6D0D09 && msg.data_length_code >= 8) {
            uint16_t vRaw = (uint16_t)((msg.data[0] << 8) | msg.data[1]);
            int16_t iRaw = (int16_t)((msg.data[2] << 8) | msg.data[3]);
            float volt = (float)vRaw * 0.1f;
            float curr = (float)iRaw * 0.1f;

            vd.voltage = volt;
            vd.current = curr;
            bd.totalVoltage = volt;
            bd.current = curr;
            bd.power = volt * curr;
            bd.isConnected = true;
        }

        // 3. Gói tin Dung lượng Pin SOC & SOH: 0x0A6E0D09 (Chuẩn JAMFOXRS)
        else if (id == 0x0A6E0D09 && msg.data_length_code >= 6) {
            uint16_t socRaw = (uint16_t)((msg.data[0] << 8) | msg.data[1]);
            if (socRaw > 100) socRaw = 100;
            bd.soc = (uint8_t)socRaw;
            bd.isConnected = true;
        }

        // 4. Gói tin Nhiệt độ Cell BMS: 0x0E6C0D09
        else if (id == 0x0E6C0D09 && msg.data_length_code >= 5) {
            bd.temperatures[0] = (float)msg.data[0];
            bd.temperatures[1] = (float)msg.data[1];
        }
    }

    if (_isConnected && (millis() - _lastMessageMs > 5000)) {
        _isConnected = false;
    }
}
