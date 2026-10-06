#include "ble_nav_manager.h"

// Forward declaration của callbacks
class ServerCallbacks : public BLEServerCallbacks {
public:
    ServerCallbacks(BleNavManager* mgr) : _mgr(mgr) {}
    void onConnect(BLEServer* pServer) override {
        _mgr->onConnect();
    }
    void onDisconnect(BLEServer* pServer) override {
        _mgr->onDisconnect();
    }
private:
    BleNavManager* _mgr;
};

class RxCallbacks : public BLECharacteristicCallbacks {
public:
    RxCallbacks(BleNavManager* mgr) : _mgr(mgr) {}
    void onWrite(BLECharacteristic* pCharacteristic) override {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            String str = String(rxValue.c_str());
            str.trim();
            _mgr->parseIncomingMessage(str);
        }
    }
private:
    BleNavManager* _mgr;
};

class MySecurityCallbacks : public BLESecurityCallbacks {
public:
    uint32_t onPassKeyRequest() override { return 123456; }
    void onPassKeyNotify(uint32_t pass_key) override {}
    bool onSecurityRequest() override { return true; }
    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
        if (cmpl.success) {
            Serial.println("[BLE] ✅ Ghép đôi (Bonding) thành công!");
        } else {
            Serial.printf("[BLE] ⚠️ Ghép đôi hoàn tất, mã: %d\n", cmpl.fail_reason);
        }
    }
    bool onConfirmPIN(uint32_t pin) override { return true; }
};

BleNavManager::BleNavManager()
    : _pServer(nullptr), _pTxCharacteristic(nullptr), _pRxCharacteristic(nullptr),
      _pFoxTelemetryCharacteristic(nullptr), _pFoxCmdCharacteristic(nullptr),
      _deviceConnected(false), _oldDeviceConnected(false), _lastTelemetrySendMs(0) {
    _mutex = xSemaphoreCreateMutex();
}

BleNavManager::~BleNavManager() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
    }
}

void BleNavManager::begin(const char* deviceName) {
    Serial.printf("[BLE] Đang khởi tạo Bluetooth Low Energy: %s...\n", deviceName);

    // 1. Khởi tạo BLE Device & Cấu hình MTU 512
    BLEDevice::init(deviceName);
    BLEDevice::setMTU(512);

    // 2. Cấu hình bảo mật ghép đôi (Pairing / Bonding) trực tiếp từ cài đặt máy (Just Works)
    BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    // 3. Tạo BLE Server
    _pServer = BLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks(this));

    // 4. Tạo BLE Service 1 (Nordic UART Service - SmartEV)
    BLEService* pService = _pServer->createService(BLE_SERVICE_UUID);

    _pTxCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    _pTxCharacteristic->addDescriptor(new BLE2902());

    _pRxCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    _pRxCharacteristic->setCallbacks(new RxCallbacks(this));
    pService->start();

    // 5. Tạo BLE Service 2 (Tương thích 100% với firmware và App)
    BLEService* pFoxService = _pServer->createService(BLE_JAMFOX_SERVICE_UUID);

    _pFoxTelemetryCharacteristic = pFoxService->createCharacteristic(
        BLE_JAMFOX_CHAR_TELEMETRY,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE
    );
    _pFoxTelemetryCharacteristic->addDescriptor(new BLE2902());
    _pFoxTelemetryCharacteristic->setCallbacks(new RxCallbacks(this));
    _pFoxTelemetryCharacteristic->setValue("SmartEV BLE Ready");

    _pFoxCmdCharacteristic = pFoxService->createCharacteristic(
        BLE_JAMFOX_CHAR_COMMAND,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE
    );
    _pFoxCmdCharacteristic->addDescriptor(new BLE2902());
    _pFoxCmdCharacteristic->setCallbacks(new RxCallbacks(this));
    _pFoxCmdCharacteristic->setValue("SmartEV CMD Ready");
    pFoxService->start();

    // 6. Cấu hình Quảng bá (Advertising) chuẩn BLE không tràn bộ đệm 31 bytes
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->addServiceUUID(BLE_JAMFOX_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // Tối ưu kết nối iPhone/Android (7.5ms)
    pAdvertising->setMaxPreferred(0x12); // Tối ưu kết nối iPhone/Android (22.5ms)

    // Primary Advertisement: Chứa Cờ & TÊN THIẾT BỊ ĐẦY ĐỦ (đảm bảo điện thoại quét thấy ngay tên)
    BLEAdvertisementData advData;
    advData.setName(deviceName);
    advData.setFlags(0x06);
    pAdvertising->setAdvertisementData(advData);

    // Scan Response: Chứa Service UUIDs
    BLEAdvertisementData scanRespData;
    scanRespData.setCompleteServices(BLEUUID(BLE_SERVICE_UUID));
    pAdvertising->setScanResponseData(scanRespData);

    BLEDevice::startAdvertising();

    Serial.printf("[BLE] BLE GATT Server đang phát quảng bá! Sẵn sàng kết nối qua tên: %s\n", deviceName);
    Serial.printf("[BLE] Hỗ trợ 2 Service UUID: (%s) & (%s)\n", BLE_SERVICE_UUID, BLE_JAMFOX_SERVICE_UUID);
}

void BleNavManager::onConnect() {
    _deviceConnected = true;
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        _data.isConnected = true;
        xSemaphoreGive(_mutex);
    }
    Serial.println("\n[BLE] >>> Điện thoại đã KẾT NỐI thành công qua Bluetooth BLE! <<<");
}

void BleNavManager::onDisconnect() {
    _deviceConnected = false;
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        _data.isConnected = false;
        xSemaphoreGive(_mutex);
    }
    Serial.println("\n[BLE] <<< Điện thoại đã NGẮT KẾT NỐI. Đang tự động phát quảng bá lại... >>>");
    
    // Tự động phát quảng bá lại để sẵn sàng cho kết nối tiếp theo
    if (_pServer) {
        _pServer->startAdvertising();
    }
}

bool BleNavManager::getSnapshot(BleNavData &out) {
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        out = _data;
        xSemaphoreGive(_mutex);
        return true;
    }
    return false;
}

NavIconType BleNavManager::parseIconType(const String &str) {
    String s = str;
    s.toUpperCase();
    s.trim();

    if (s == "STRAIGHT" || s == "1" || s == "UP") return NavIconType::STRAIGHT;
    if (s == "LEFT" || s == "TURN_LEFT" || s == "2") return NavIconType::TURN_LEFT;
    if (s == "RIGHT" || s == "TURN_RIGHT" || s == "3") return NavIconType::TURN_RIGHT;
    if (s == "SLIGHT_LEFT" || s == "4") return NavIconType::SLIGHT_LEFT;
    if (s == "SLIGHT_RIGHT" || s == "5") return NavIconType::SLIGHT_RIGHT;
    if (s == "UTURN" || s == "U_TURN" || s == "6") return NavIconType::U_TURN;
    if (s == "DEST" || s == "DESTINATION" || s == "7") return NavIconType::DESTINATION;
    return NavIconType::NONE;
}

void BleNavManager::parseIncomingMessage(const String &msg) {
    Serial.printf("[BLE RX] Nhận gói tin: %s\n", msg.c_str());

    if (!_mutex || xSemaphoreTake(_mutex, pdMS_TO_TICKS(20)) != pdTRUE) return;

    if (msg.startsWith("NAV:CLEAR")) {
        _data.hasNav = false;
        _data.icon = NavIconType::NONE;
        memset(_data.distance, 0, sizeof(_data.distance));
        memset(_data.instruction, 0, sizeof(_data.instruction));
        Serial.println("[BLE NAV] Đã xóa lộ trình chỉ đường.");
    } else if (msg.startsWith("NAV:")) {
        // Định dạng: NAV:<ICON>:<DIST>:<STREET>
        // Ví dụ: NAV:LEFT:250m:Nguyen Hue
        int firstColon = msg.indexOf(':', 4);
        if (firstColon > 0) {
            String iconStr = msg.substring(4, firstColon);
            int secondColon = msg.indexOf(':', firstColon + 1);
            if (secondColon > 0) {
                String distStr = msg.substring(firstColon + 1, secondColon);
                String streetStr = msg.substring(secondColon + 1);

                _data.hasNav = true;
                _data.icon = parseIconType(iconStr);
                strncpy(_data.distance, distStr.c_str(), sizeof(_data.distance) - 1);
                strncpy(_data.instruction, streetStr.c_str(), sizeof(_data.instruction) - 1);
                _data.lastNavUpdateMs = millis();

                Serial.printf("[BLE NAV] Đã cập nhật chỉ đường: %s [%s] - %s\n",
                              _data.getIconName(), _data.distance, _data.instruction);
            }
        }
    } else if (msg.startsWith("MEDIA:")) {
        // Định dạng: MEDIA:<TITLE>:<ARTIST>:<STATUS>
        // Ví dụ: MEDIA:Waiting For You:MONO:PLAY
        int firstColon = msg.indexOf(':', 6);
        if (firstColon > 0) {
            String titleStr = msg.substring(6, firstColon);
            int secondColon = msg.indexOf(':', firstColon + 1);
            if (secondColon > 0) {
                String artistStr = msg.substring(firstColon + 1, secondColon);
                String statusStr = msg.substring(secondColon + 1);
                statusStr.toUpperCase();

                strncpy(_data.mediaTitle, titleStr.c_str(), sizeof(_data.mediaTitle) - 1);
                strncpy(_data.mediaArtist, artistStr.c_str(), sizeof(_data.mediaArtist) - 1);
                _data.isPlaying = (statusStr == "PLAY" || statusStr == "1");

                Serial.printf("[BLE MEDIA] %s: %s - %s\n",
                              _data.isPlaying ? "Đang phát" : "Tạm dừng",
                              _data.mediaTitle, _data.mediaArtist);
            }
        }
    } else if (msg.startsWith("CALL:CLEAR")) {
        _data.hasIncomingCall = false;
        memset(_data.incomingCall, 0, sizeof(_data.incomingCall));
        Serial.println("[BLE CALL] Đã kết thúc cuộc gọi.");
    } else if (msg.startsWith("CALL:")) {
        // Định dạng: CALL:<CALLER_INFO>
        String callerStr = msg.substring(5);
        _data.hasIncomingCall = true;
        _data.callPopupStartTime = millis();
        strncpy(_data.incomingCall, callerStr.c_str(), sizeof(_data.incomingCall) - 1);
        Serial.printf("[BLE CALL] Cuộc gọi đến từ: %s\n", _data.incomingCall);
    } else if (msg.startsWith("TIME:")) {
        // Định dạng nhận từ điện thoại: TIME:YYYY:MM:DD:hh:mm:ss
        int y = 2026, m = 1, d = 1, h = 0, min = 0, s = 0;
        int count = sscanf(msg.c_str(), "TIME:%d:%d:%d:%d:%d:%d", &y, &m, &d, &h, &min, &s);
        if (count == 6 && _timeSyncCallback) {
            _timeSyncCallback(y, m, d, h, min, s);
            Serial.printf("[RTC SYNC] ✅ Đã tự động cập nhật giờ RTC từ điện thoại: %02d:%02d:%02d %02d/%02d/%04d\n",
                          h, min, s, d, m, y);
        }
    } else if (msg.indexOf("\"cmd\":\"set_time\"") >= 0) {
        // Hỗ trợ định dạng JSON: {"cmd":"set_time","time":"12:30:45","date":"06/10/2026","dayofweek":2}
        int h = 0, min = 0, s = 0, d = 1, m = 1, y = 2026;
        int tPos = msg.indexOf("\"time\":\"");
        if (tPos >= 0) {
            sscanf(msg.substring(tPos + 8).c_str(), "%d:%d:%d", &h, &min, &s);
        }
        int dPos = msg.indexOf("\"date\":\"");
        if (dPos >= 0) {
            sscanf(msg.substring(dPos + 8).c_str(), "%d/%d/%d", &d, &m, &y);
        }
        if (_timeSyncCallback) {
            _timeSyncCallback(y, m, d, h, min, s);
            Serial.printf("[RTC SYNC] ✅ Đã nạp giờ chuẩn: %02d:%02d:%02d %02d/%02d/%04d\n",
                          h, min, s, d, m, y);
        }
    } else if (msg.indexOf("\"cmd\":\"wifi\"") >= 0) {
        String resp = "{\"status\":\"ok\",\"cmd\":\"wifi\"}";
        if (_pFoxCmdCharacteristic) {
            _pFoxCmdCharacteristic->setValue((uint8_t*)resp.c_str(), resp.length());
            _pFoxCmdCharacteristic->notify();
        }
    } else if (msg.startsWith("CMD:POLL_VOTOL")) {
        Serial.println("[BLE CMD] Nhận lệnh từ điện thoại: GỬI LỆNH THĂM DÒ VOTOL (SHOW)!");
        if (_onPollVotolCallback) _onPollVotolCallback();
    } else if (msg.startsWith("CMD:SET_BAUD:")) {
        uint32_t b = msg.substring(13).toInt();
        if (b > 0) {
            Serial.printf("[BLE CMD] Nhận lệnh từ điện thoại: ĐỔI BAUDRATE UART1 SANG %d!\n", b);
            if (_onSetBaudCallback) _onSetBaudCallback(b);
        }
    } else if (msg.startsWith("CMD:SWAP_UART")) {
        Serial.println("[BLE CMD] Nhận lệnh từ điện thoại: ĐẢO CHÉO CHÂN RX ⮂ TX PHẦN MỀM!");
        if (_onSwapUartCallback) _onSwapUartCallback();
    } else if (msg.startsWith("BMS:")) {
        // Đồng bộ dữ liệu Pin ANT BMS từ điện thoại: BMS:voltage:current:soc:temp1:temp2:deltaMv:minMv:maxMv
        float v = 0, a = 0;
        int soc = 0, t1 = 0, t2 = 0, delta = 0, minV = 0, maxV = 0;
        int count = sscanf(msg.c_str(), "BMS:%f:%f:%d:%d:%d:%d:%d:%d", &v, &a, &soc, &t1, &t2, &delta, &minV, &maxV);
        if (count >= 3) {
            ANTBMSData bmsSync;
            memset(&bmsSync, 0, sizeof(bmsSync));
            bmsSync.isConnected = true;
            bmsSync.totalVoltage = v;
            bmsSync.current = a;
            bmsSync.power = v * a;
            bmsSync.soc = (uint8_t)soc;
            bmsSync.temperatures[2] = (float)t1;
            bmsSync.temperatures[3] = (float)t2;
            bmsSync.deltaCellVoltage = delta * 0.001f;
            bmsSync.minCellVoltage = minV * 0.001f;
            bmsSync.maxCellVoltage = maxV * 0.001f;
            bmsSync.lastReceivedMs = millis();
            bmsSync.validPacketsCount++;

            if (_onBmsSyncCallback) {
                _onBmsSyncCallback(bmsSync);
            }
            Serial.printf("[BLE SYNC BMS] ✅ Đã nhận dữ liệu Pin ANT từ điện thoại: %.1fV, %.1fA, %d%%, Delta %dmV\n",
                          v, a, soc, delta);
        }
    }

    xSemaphoreGive(_mutex);
}

void BleNavManager::sendTelemetry(const VotolData &vd, const ANTBMSData &bd, const SignalState &sig) {
    if (!_deviceConnected) return;

    // Chuỗi JSON chi tiết kèm thông số Chẩn đoán UART, nút bấm & tín hiệu xe gửi lên App
    char jsonBuf[384];
    const char* hexStr = (strlen(vd.lastRawHex) > 0) ? vd.lastRawHex : "--";
    snprintf(jsonBuf, sizeof(jsonBuf),
             "{\"spd\":%.1f,\"gear\":\"%s\",\"v\":%.1f,\"a\":%.1f,\"soc\":%d,\"p\":%.0f,"
             "\"temp_m\":%d,\"temp_c\":%d,\"turn_l\":%d,\"turn_r\":%d,\"beam\":%d,\"err\":%u,"
             "\"brk\":%d,\"stand\":%d,\"rgn\":%d,\"rev\":%d,\"park\":%d,"
             "\"v_tx\":%u,\"v_rx\":%u,\"v_bps\":%u,\"v_ok\":%u,\"v_hex\":\"%s\",\"baud\":%u,\"swap\":%d}",
             vd.speedKmh, vd.getGearString(),
             bd.isConnected ? bd.totalVoltage : vd.voltage,
             bd.isConnected ? bd.current : vd.current,
             bd.isConnected ? bd.soc : 0,
             bd.isConnected ? bd.power : (vd.voltage * vd.current),
             (int)vd.motorTemp,
             (int)vd.controllerTemp,
             sig.turnLeft ? 1 : 0,
             sig.turnRight ? 1 : 0,
             sig.headlight ? 1 : 0,
             vd.faultCode,
             vd.brake ? 1 : 0,
             vd.sideStand ? 1 : 0,
             vd.regen ? 1 : 0,
             vd.reverse ? 1 : 0,
             vd.parked ? 1 : 0,
             vd.totalBytesSent,
             vd.totalBytesReceived,
             vd.bytesPerSecond,
             vd.validPacketsCount,
             hexStr,
             vd.currentBaudRate,
             vd.pinsSwapped ? 1 : 0);

    if (_pTxCharacteristic) {
        _pTxCharacteristic->setValue((uint8_t*)jsonBuf, strlen(jsonBuf));
        _pTxCharacteristic->notify();
    }

    if (_pFoxTelemetryCharacteristic) {
        static uint32_t s_fullSendTimer = 0;
        static unsigned long s_hb = 0;
        s_hb++;
        const char* modeStr = vd.parked ? "PARK" : (vd.reverse ? "REVERSE" : (vd.brake ? "BRAKE" : "READY"));
        uint8_t mb = vd.parked ? 0x00 : (vd.reverse ? 0x50 : (vd.brake ? 0x72 : 0x70));
        float vVal = bd.isConnected ? bd.totalVoltage : vd.voltage;
        float aVal = bd.isConnected ? bd.current : vd.current;
        float pVal = bd.isConnected ? bd.power : (vd.voltage * vd.current);
        int socVal = bd.isConnected ? bd.soc : 0;

        uint32_t now = millis();
        if (now - s_fullSendTimer > 2000) {
            s_fullSendTimer = now;
            char cellsStr[180] = "";
            int cpos = 0;
            for (int i = 0; i < 24; i++) {
                int mv = (bd.isConnected && bd.cellVoltages[i] > 0.5f) ? (int)(bd.cellVoltages[i] * 1000.0f) : 0;
                cpos += snprintf(cellsStr + cpos, sizeof(cellsStr) - cpos, "%d%s", mv, (i < 23) ? "," : "");
            }
            char fullBuf[512];
            snprintf(fullBuf, sizeof(fullBuf),
                     "{\"r\":%d,\"s\":%d,\"m\":\"%s\",\"mb\":%u,\"gs\":%u,\"v\":%.1f,\"a\":%.1f,\"p\":%.0f,\"sc\":%d,\"wf\":0,"
                     "\"t\":{\"c\":%d,\"m\":%d,\"b\":%d},\"cells\":[%s],\"cd\":%d,\"cr\":0,"
                     "\"h\":{\"soh\":100,\"cyc\":0,\"rc\":0.0,\"fc\":0.0},"
                     "\"cvs\":{\"hi\":%u,\"hiC\":1,\"lo\":%u,\"loC\":1,\"av\":%u},"
                     "\"ts\":{\"max\":%u,\"maxC\":1,\"min\":%u,\"minC\":1},"
                     "\"b\":{\"md\":0,\"st\":0,\"cells\":[]},"
                     "\"ct\":[%d,%d,%d,%d,%d],"
                     "\"chr\":{\"v\":0.0,\"a\":0.0,\"on\":0,\"ori\":0,\"status\":0},"
                     "\"age\":0,\"hb\":%lu,\"type\":\"full\"}\n",
                     (int)vd.rpm, (int)(vd.speedKmh + 0.5f), modeStr,
                     mb, (unsigned int)vd.gear, vVal, aVal, pVal, socVal,
                     (int)vd.controllerTemp, (int)vd.motorTemp,
                     bd.isConnected ? (int)bd.temperatures[0] : (int)vd.controllerTemp,
                     cellsStr, (int)(bd.deltaCellVoltage * 1000.0f),
                     (unsigned int)(bd.maxCellVoltage * 1000.0f),
                     (unsigned int)(bd.minCellVoltage * 1000.0f),
                     (unsigned int)(vVal > 0 ? (vVal * 1000.0f / 20.0f) : 0),
                     (unsigned int)vd.controllerTemp, (unsigned int)vd.controllerTemp,
                     (int)bd.temperatures[0], (int)bd.temperatures[1], (int)bd.temperatures[2],
                     (int)bd.temperatures[3], (int)bd.temperatures[4],
                     s_hb);
            _pFoxTelemetryCharacteristic->setValue((uint8_t*)fullBuf, strlen(fullBuf));
            _pFoxTelemetryCharacteristic->notify();
        } else {
            char fastBuf[256];
            snprintf(fastBuf, sizeof(fastBuf),
                     "{\"r\":%d,\"s\":%d,\"m\":\"%s\",\"mb\":%u,\"gs\":%u,\"v\":%.1f,\"a\":%.1f,\"p\":%.0f,\"sc\":%d,\"wf\":0,"
                     "\"t\":{\"c\":%d,\"m\":%d,\"b\":%d},\"cr\":0,\"hb\":%lu,\"type\":\"fast\"}\n",
                     (int)vd.rpm, (int)(vd.speedKmh + 0.5f), modeStr,
                     mb, (unsigned int)vd.gear, vVal, aVal, pVal, socVal,
                     (int)vd.controllerTemp, (int)vd.motorTemp,
                     bd.isConnected ? (int)bd.temperatures[0] : (int)vd.controllerTemp,
                     s_hb);
            _pFoxTelemetryCharacteristic->setValue((uint8_t*)fastBuf, strlen(fastBuf));
            _pFoxTelemetryCharacteristic->notify();
        }
    }
}

void BleNavManager::update(const VotolData &vd, const ANTBMSData &bd, const SignalState &sig) {
    uint32_t now = millis();
    if (_deviceConnected && (now - _lastTelemetrySendMs >= BLE_TELEMETRY_INTERVAL_MS)) {
        _lastTelemetrySendMs = now;
        sendTelemetry(vd, bd, sig);
    }
}

// --- CÁC HÀM MÔ PHỎNG PHỤC VỤ TEST TÍNH NĂNG ---

void BleNavManager::simulateNav(NavIconType icon, const char* dist, const char* instruction) {
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        _data.hasNav = true;
        _data.icon = icon;
        strncpy(_data.distance, dist, sizeof(_data.distance) - 1);
        strncpy(_data.instruction, instruction, sizeof(_data.instruction) - 1);
        _data.lastNavUpdateMs = millis();
        xSemaphoreGive(_mutex);

        Serial.printf("[TEST SIM] Nạp chỉ đường: %s [%s] %s\n",
                      _data.getIconName(), dist, instruction);
    }
}

void BleNavManager::simulateMedia(const char* title, const char* artist, bool isPlaying) {
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        strncpy(_data.mediaTitle, title, sizeof(_data.mediaTitle) - 1);
        strncpy(_data.mediaArtist, artist, sizeof(_data.mediaArtist) - 1);
        _data.isPlaying = isPlaying;
        xSemaphoreGive(_mutex);

        Serial.printf("[TEST SIM] Nạp bài hát: %s - %s (%s)\n",
                      title, artist, isPlaying ? "PLAY" : "PAUSE");
    }
}

void BleNavManager::simulateCall(const char* caller) {
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        _data.hasIncomingCall = true;
        _data.callPopupStartTime = millis();
        strncpy(_data.incomingCall, caller, sizeof(_data.incomingCall) - 1);
        xSemaphoreGive(_mutex);

        Serial.printf("[TEST SIM] Nạp cuộc gọi đến: %s\n", caller);
    }
}

void BleNavManager::clearNav() {
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        _data.hasNav = false;
        _data.icon = NavIconType::NONE;
        memset(_data.distance, 0, sizeof(_data.distance));
        memset(_data.instruction, 0, sizeof(_data.instruction));
        xSemaphoreGive(_mutex);

        Serial.println("[TEST SIM] Đã xóa chỉ đường.");
    }
}

void BleNavManager::clearCall() {
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        _data.hasIncomingCall = false;
        memset(_data.incomingCall, 0, sizeof(_data.incomingCall));
        xSemaphoreGive(_mutex);

        Serial.println("[TEST SIM] Đã tắt cuộc gọi.");
    }
}
