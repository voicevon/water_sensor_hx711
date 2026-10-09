#pragma once

#include <Arduino.h>

// ============================================================
//  3路 HX711 实时数据缓存结构（24 位有符号原生 ADC 计数）
// ============================================================
struct SensorDataCache {
    int32_t raw_val;    // 24 位原生原始计数
    int32_t filtered;   // 滑动平均滤波后值
    int32_t baseline;   // 慢速基准值
    int32_t threshold;  // 当前触发阈值
    bool    detected;   // 是否触发检测
};

/**
 * @brief 更新指定通道的实时传感器数据，供网页 /api/data 查询
 * @param idx 通道索引（0-2）
 */
void data_cache_update_sensor(int idx, int32_t raw_val, int32_t filtered,
                               int32_t baseline, int32_t threshold, bool detected);

/**
 * @brief 获取指定物理通道的数据缓存（内部服务层使用）
 */
const SensorDataCache& data_cache_get_sensor(int idx);
