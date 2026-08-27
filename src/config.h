#pragma once

// ============================================================
//  全局配置文件 — 所有硬件参数、协议常量、网络配置的唯一来源
//  修改引脚、波特率、WiFi 凭据等只需改此文件
// ============================================================

#include <Arduino.h>

#define SENSOR_COUNT    3

/**
 * @brief 将 HX711 读取的克力值转换为 uint16_t（保留一位小数，乘以 10）
 *        量程：0 ~ 6553.5 g（uint16_t 满值 65535 对应 6553.5 g）
 *        负值（传感器被反向拉力）截断为 0
 */
inline uint16_t convert_to_force(float gram_val) {
    float val = gram_val * 10.0f; // 0.1g 分辨率
    if (val < 0.0f)     val = 0.0f;
    if (val > 65535.0f) val = 65535.0f;
    return (uint16_t)val;
}

// -------- HX711 引脚定义（3 路独立 HX711） --------
// 每路 HX711 需要 DOUT（数据）和 SCK（时钟）两根线
#define HX711_1_DOUT_PIN  23  // 对应 HX711 #1 DOUT
#define HX711_1_SCK_PIN   22  // 对应 HX711 #1 SCK
#define HX711_2_DOUT_PIN  32  // 对应 HX711 #2 DOUT
#define HX711_2_SCK_PIN   33  // 对应 HX711 #2 SCK
#define HX711_3_DOUT_PIN  26  // 对应 HX711 #3 DOUT
#define HX711_3_SCK_PIN   27  // 对应 HX711 #3 SCK

// -------- LED 状态指示灯引脚 --------
#define LED_PIN_B        2
#define LED_PIN_A       15

// -------- 采样与发送定时周期 (1 Hz) --------
#define SEND_INTERVAL_MS  1000UL

// -------- 状态看门狗超时时间 (5 小时 = 18000000 毫秒) --------
#define WATER_WATCHDOG_TIMEOUT_MS (5 * 3600 * 1000UL)

// -------- BLE 广播参数 --------
#define BLE_DEVICE_NAME    "FengBLE"
// Company ID: 0xFFFF（厂商测试标识），BLE 小端序：LSB 在前
#define BLE_COMPANY_ID_LSB 0xFF
#define BLE_COMPANY_ID_MSB 0xFF

// -------- WiFi 网络配置 --------
#define FACTORY_WIFI_SSID       "Perfect"
#define FACTORY_WIFI_PASSWORD   "12344321"

#define FACTORY_WIFI_AP_SSID    "AP_HX711"
#define FACTORY_WIFI_AP_PASSWORD "12344321"

// -------- MQTT Broker & 设备命名配置 --------
#define FACTORY_DEVICE_NAME     "home"
#define FACTORY_MQTT_BROKER     "voicevon.vicp.io"
#define FACTORY_MQTT_PORT       1883
#define MQTT_CONTROL_TOPIC "water/sensor/start"
#define MQTT_STATUS_TOPIC  "water/sensor/status"
// MQTT 非阻塞重连最小间隔（毫秒）
#define MQTT_RECONNECT_INTERVAL_MS  5000UL
