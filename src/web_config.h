#pragma once

#include <Arduino.h>

/**
 * @brief 初始化 Web Server 及其 AP 模式，加载 NVS 中的通道配置
 */
void web_config_init();

/**
 * @brief 在主循环中驱动 Web Server 客户端请求
 */
void web_config_loop();

/**
 * @brief 更新指定通道的实时传感器数据，供网页 /api/data 查询
 * @param idx  通道索引（0-2）
 * @param raw_val    HX711 原始克力值（g）
 * @param filtered   滤波后值（uint16_t，0.1g 单位）
 * @param baseline   基准值（uint16_t）
 * @param threshold  触发阈值（uint16_t）
 * @param detected   是否触发检测
 */
void web_config_update_sensor(int idx, float raw_val, uint16_t filtered,
                               uint16_t baseline, uint16_t threshold, bool detected);

/**
 * @brief 从 NVS 获取配置的 STA Wi-Fi SSID
 */
String get_sta_ssid();

/**
 * @brief 从 NVS 获取配置的 STA Wi-Fi 密码
 */
String get_sta_password();

/**
 * @brief 获取指定通道在内存缓存/NVS 中设定的阈值偏移量（ch: 0-2）
 */
int get_channel_threshold(int ch_idx);

/**
 * @brief 从 NVS 获取配置的设备名称 (DEVICE_NAME)
 */
String get_device_name();

/**
 * @brief 从 NVS 获取配置的 MQTT Broker 地址
 */
String get_mqtt_broker();

/**
 * @brief 从 NVS 获取配置的 MQTT Broker 端口
 */
int get_mqtt_port();

/**
 * @brief 获取指定通道的算法类型 (0=DYNAMIC, 1=DISCRETE, 2=ENVELOPE)
 */
int get_algo_type(int ch);

/**
 * @brief 获取离散方差算法的方差触发阈值
 */
int get_var_threshold(int ch);

/**
 * @brief 获取包络算法参数
 */
int get_env_window(int ch);
int get_env_dry_up(int ch);
int get_env_dry_down(int ch);
int get_env_upper_offset(int ch);
int get_env_lower_offset(int ch);
