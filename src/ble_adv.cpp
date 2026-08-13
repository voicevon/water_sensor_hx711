#include "ble_adv.h"
#include "config.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// 模块内部静态变量
static BLEAdvertising *s_pAdvertising = nullptr;
static uint8_t         s_seq_num      = 0; // 递增序列号，外部不可见

// ============================================================

void ble_init() {
    BLEDevice::init(BLE_DEVICE_NAME);
    s_pAdvertising = BLEDevice::getAdvertising();
    s_pAdvertising->start();
}

// ============================================================

void ble_update(const uint16_t *sensors, const bool *states) {
    // 1. 组装广播数据对象
    BLEAdvertisementData advData;
    advData.setFlags(0x06);         // General Discoverable Mode, BR/EDR Not Supported
    advData.setName(BLE_DEVICE_NAME);

    // 2. 构建 Manufacturer Specific Data payload
    //    新格式：CID（2B）+ Sensor1~Sensor3（各 2B，大端序）+ StateByte（1B）+ SeqNum（1B）= 10 字节
    std::string mData;
    mData.reserve(2 + SENSOR_COUNT * 2 + 1 + 1);
    mData.push_back((char)BLE_COMPANY_ID_LSB);
    mData.push_back((char)BLE_COMPANY_ID_MSB);

    for (int i = 0; i < SENSOR_COUNT; i++) {
        mData.push_back((char)(sensors[i] >> 8));   // MSB
        mData.push_back((char)(sensors[i] & 0xFF)); // LSB
    }

    // 计算状态字节 (仅 3 个通道)
    uint8_t state_byte = 0;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (states[i]) {
            state_byte |= (1 << i);
        }
    }
    mData.push_back((char)state_byte);

    mData.push_back((char)(s_seq_num++)); // 递增序列号

    advData.setManufacturerData(mData);

    // 3. 停止 → 更新数据 → 重启广播（使数据立即生效）
    s_pAdvertising->stop();
    s_pAdvertising->setAdvertisementData(advData);
    s_pAdvertising->start();

    // 4. 调试日志（已屏蔽，避免干扰 WiFi 调试）
#if 0
    Serial.print("[BLE] Manufacturer Data (HEX): ");
    for (size_t i = 0; i < mData.length(); i++) {
        Serial.printf("%02X ", (uint8_t)mData[i]);
    }
    Serial.println();
    Serial.println("[BLE] Advertising restarted.");
#endif

}
