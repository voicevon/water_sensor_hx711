#pragma once

// ============================================================
//  网络通信层（WiFi + MQTT）
//  职责：WiFi 初始化、MQTT 非阻塞重连、JSON Payload 发布
// ============================================================

#include "config.h"

/**
 * @brief 初始化 WiFi 连接与 MQTT Broker 配置
 *        WiFi 连接采用最多 20 次 × 500ms 的阻塞等待，超时后继续启动并在后台重试。
 *        网络凭据与 Broker 地址由 config.h 中的宏定义决定。
 */
void wifi_mqtt_init();

/**
 * @brief 在主循环中维持 WiFi / MQTT 心跳（非阻塞）
 *        - 若 WiFi 已连接且 MQTT 断开，则尝试非阻塞重连（5s 节流）
 *        - 若 MQTT 已连接，则调用 mqttClient.loop() 处理订阅消息与保活包
 *
 * @param current_time  millis() 当前时间戳，避免重复调用
 */
void wifi_mqtt_loop(unsigned long current_time);

/**
 * @brief 将传感器数据组装为 JSON 字符串并发布到 MQTT Topic
 *
 * JSON 格式：
 *   {"name":"dongzhan", "sensor1":<val1>, "sensor2":<val2>, "sensor3":<val3>, "sensor4":<val4>}
 *
 * 仅在 MQTT 已连接时发布，断线时静默跳过。
 *
 * @param sensors 传感器数据数组指针（大小为 SENSOR_COUNT）
 * @param stateByte 传感器有水状态字节 (按位存储)
 * @return bool 是否成功发布了数据
 */
bool mqtt_publish(const uint16_t *sensors, uint8_t stateByte);

/**
 * @brief 获取 MQTT 客户端当前的连接状态
 * 
 * @return bool 如果已连接返回 true，否则返回 false
 */
bool mqtt_is_connected();

/**
 * @brief 获取 WiFi 当前的连接状态
 * 
 * @return bool 如果已连接返回 true，否则返回 false
 */
bool wifi_is_connected();
