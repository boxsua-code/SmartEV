#include "can_bus_handler.h"
#include <cmath>

// Lookup table tính SoC chuẩn từ BMS CAN
static const uint16_t socToBmsTable[101] = {
  0, 60,70,80,90,95,105,115,125,135,140,150,160,170,180,185,195,205,215,225,
  230,240,250,260,270,275,285,295,305,315,320,330,340,350,360,365,375,385,395,405,
  410,420,430,440,450,455,465,475,485,495,500,510,520,530,540,550,555,565,575,585,
  590,600,610,620,630,635,645,655,665,675,680,690,700,710,720,725,735,745,755,765,
  770,780,790,800,810,815,825,835,845,855,860,870,880,890,900,905,915,925,935,945,950
};

static float getSoCFromLookup(uint16_t raw) {
    if (raw >= socToBmsTable[100]) return 100.0f;
    if (raw <= socToBmsTable[0]) return 0.0f;
    for (int i = 0; i < 100; i++) {
        if (raw >= socToBmsTable[i] && raw <= socToBmsTable[i + 1]) {
            float range = (float)(socToBmsTable[i + 1] - socToBmsTable[i]);
            float delta = (float)(raw - socToBmsTable[i]);
            if (range == 0) return (float)i;
            return (float)i + (delta / range);
        }
    }
    return 0.0f;
}

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

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)_txPin, (gpio_num_t)_rxPin, TWAI_MODE_LISTEN_ONLY);
    g_config.rx_queue_len = 20;

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        Serial.printf("[CAN] Khong the cai dat TWAI driver (TX: %d, RX: %d)\n", _txPin, _rxPin);
        return false;
    }

    if (twai_start() != ESP_OK) {
        Serial.println("[CAN] Khong the khoi dong TWAI");
        return false;
    }

    _initialized = true;
    Serial.printf("[CAN] Da kich hoat TWAI (250kbps, LISTEN_ONLY, TX: GPIO %d, RX: GPIO %d)\n",
                  _txPin, _rxPin);
    return true;
}

void CanBusHandler::update(VotolData &vd, ANTBMSData &bd) {
    if (!_initialized) return;

    twai_message_t msg;
    while (twai_receive(&msg, pdMS_TO_TICKS(0)) == ESP_OK) {
        _messageCount++;
        _lastMessageMs = millis();
        _isConnected = true;

        uint32_t id = msg.identifier;

        // 1. Controller basic: 0x0A010810
        if (id == 0x0A010810 && msg.data_length_code >= 8) {
            uint8_t m = msg.data[1];
            int16_t rpmVal = (int16_t)(msg.data[2] | (msg.data[3] << 8));
            float spd = (float)rpmVal * 0.0891f; // Hệ số chuyển đổi RPM sang km/h chuẩn
            if (spd < 0.0f) spd = 0.0f;

            vd.rpm = rpmVal;
            vd.speedKmh = spd;
            vd.controllerTemp = msg.data[4];
            vd.motorTemp = msg.data[5];

            // Cập nhật các trạng thái điều khiển
            vd.parked    = (m == 0x00);
            vd.reverse   = (m == 0x50 || m == 0xF0 || m == 0x30 || m == 0xF8);
            vd.brake     = (m == 0x72 || m == 0xB2);
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

        // 2. Điện áp & Dòng điện: 0x0A6D0D09
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
            bd.lastReceivedMs = millis();
        }

        // 3. Dung lượng Pin SOC & SOH: 0x0A6E0D09
        else if (id == 0x0A6E0D09 && msg.data_length_code >= 6) {
            uint16_t socRaw = (uint16_t)((msg.data[0] << 8) | msg.data[1]);
            int socVal = (int)getSoCFromLookup(socRaw);
            if (socVal > 100) socVal = 100;
            if (socVal < 0) socVal = 0;
            bd.soc = (uint8_t)socVal;
            bd.isConnected = true;
            bd.lastReceivedMs = millis();
        }

        // 4. Nhiệt độ Cell BMS: 0x0E6C0D09
        else if (id == 0x0E6C0D09 && msg.data_length_code >= 5) {
            bd.temperatures[0] = (float)msg.data[0];
            bd.temperatures[1] = (float)msg.data[1];
            bd.temperatures[2] = (float)msg.data[2];
            bd.temperatures[3] = (float)msg.data[3];
            bd.temperatures[4] = (float)msg.data[4];
        }

        // 5. Thống kê Min/Max/Delta Cell: 0x0A6F0D09
        else if (id == 0x0A6F0D09 && msg.data_length_code >= 8) {
            uint16_t hi = (uint16_t)((msg.data[0] << 8) | msg.data[1]);
            uint16_t lo = (uint16_t)((msg.data[3] << 8) | msg.data[4]);
            bd.maxCellVoltage = hi * 0.001f;
            bd.minCellVoltage = lo * 0.001f;
            bd.deltaCellVoltage = (hi > lo) ? ((hi - lo) * 0.001f) : 0.0f;
        }

        // 6. Điện áp từng cell pin: 0x0E640D09 - 0x0E690D09
        else if ((id & 0xFFF0FFFF) == 0x0E600D09) {
            int baseIndex = -1;
            switch (id) {
                case 0x0E640D09: baseIndex = 0;  break;
                case 0x0E650D09: baseIndex = 4;  break;
                case 0x0E660D09: baseIndex = 8;  break;
                case 0x0E670D09: baseIndex = 12; break;
                case 0x0E680D09: baseIndex = 16; break;
                case 0x0E690D09: baseIndex = 20; break;
                default: break;
            }
            if (baseIndex >= 0) {
                for (int i = 0; i < 4 && (baseIndex + i) < 32; i++) {
                    int off = i * 2;
                    if (off + 1 < msg.data_length_code) {
                        uint16_t cellMv = (uint16_t)((msg.data[off] << 8) | msg.data[off + 1]);
                        bd.cellVoltages[baseIndex + i] = cellMv * 0.001f;
                    }
                }
                if (bd.cellCount < (uint8_t)(baseIndex + 4)) {
                    bd.cellCount = (uint8_t)(baseIndex + 4);
                }
            }
        }
    }

    if (_isConnected && (millis() - _lastMessageMs > 5000)) {
        _isConnected = false;
    }
}
