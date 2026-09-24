#pragma once

// ============================================================
//  全局配置文件 — 所有硬件参数、协议常量、网络配置的唯一来源
//  修改引脚、波特率、WiFi 凭据等只需改此文件
// ============================================================

#include <Arduino.h>

#define SENSOR_COUNT    3

// -------- 数据转换参数（N 为 NVS 可配置项，此处为默认值与合法范围） --------
#define HX711_SHIFT_N_DEFAULT   6    // 右移位数 N 默认值（合法 0~8）
#define HX711_SHIFT_N_MIN       0    // N 下限：精度上限（1 LSB = 1 原始计数）
#define HX711_SHIFT_N_MAX       8    // N 上限：精度下限（1 LSB = 256 原始计数，窗口覆盖全量程）

// 启动自检基线合法范围（16 位量程中部附近）
#define HX711_BASELINE_CHECK_LOW   8192
#define HX711_BASELINE_CHECK_HIGH  57344

/**
 * @brief HX711 两步转换：24 位有符号原始值 → 16 位无符号计数
 *        第一步：X = 原始值 + 2^K（32 位有符号中间值，K = N + 15）
 *        第二步：右移 N 位得到 uint16_t
 *        N 越小精度越高（1 LSB = 2^N 原始计数），窗口 ±2^(N+15) 须罩住
 *        实际零偏与负载；N < 8 时越窗结果回绕，由启动自检告警提示。
 * @param raw     24 位有符号原始值（HX711 库 read() 返回值）
 * @param shift_n 右移位数 N（NVS 可配置，合法 0~8）
 */
inline uint16_t hx711_raw_to_u16(int32_t raw, uint8_t shift_n) {
    int32_t x = raw + (1L << (shift_n + 15));
    return (uint16_t)(x >> shift_n);
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

// STA 断线重连：基础间隔起指数退避（20s→40s→80s→160s→320s 封顶）。
// 避免 STA 反复扫描占用射频导致 AP beacon 缺帧（AP 扫不到的直接原因）
#define WIFI_RECONNECT_BASE_MS      20000UL
#define WIFI_RECONNECT_BACKOFF_MAX_SHIFT 4

// -------- MQTT Broker & 设备命名配置 --------
#define FACTORY_DEVICE_NAME     "home"
#define FACTORY_MQTT_BROKER     "voicevon.vicp.io"
#define FACTORY_MQTT_PORT       1883
#define MQTT_CONTROL_TOPIC "water/sensor/start"
#define MQTT_STATUS_TOPIC  "water/sensor/status"
// MQTT 非阻塞重连最小间隔（毫秒）
#define MQTT_RECONNECT_INTERVAL_MS  5000UL
