#pragma once

#include <Arduino.h>

// ============================================================
//  3路 HX711 力传感器实时数据缓存结构
// ============================================================
struct SensorDataCache {
    float    raw_val;    // HX711 原始力值（g，克力，浮点）
    uint16_t filtered;   // 滑动平均滤波后值（0.1g 单位 uint16_t）
    uint16_t baseline;   // 慢速基准值
    uint16_t threshold;  // 当前触发阈值
    bool     detected;   // 是否触发检测
};

/**
 * @brief 更新指定通道的实时传感器数据，供网页 /api/data 查询
 * @param idx 通道索引（0-2）
 */
void data_cache_update_sensor(int idx, float raw_val, uint16_t filtered,
                               uint16_t baseline, uint16_t threshold, bool detected);

/**
 * @brief 获取指定物理通道的数据缓存（内部服务层使用）
 */
const SensorDataCache& data_cache_get_sensor(int idx);
