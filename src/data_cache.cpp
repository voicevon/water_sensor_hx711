#include "data_cache.h"

// 3路 HX711 力传感器数据缓存（内部私有）
static SensorDataCache s_sensor_cache[3] = {0};

void data_cache_update_sensor(int idx, float raw_val, uint16_t filtered,
                               uint16_t baseline, uint16_t threshold, bool detected) {
    if (idx < 0 || idx >= 3) return;
    s_sensor_cache[idx].raw_val   = raw_val;
    s_sensor_cache[idx].filtered  = filtered;
    s_sensor_cache[idx].baseline  = baseline;
    s_sensor_cache[idx].threshold = threshold;
    s_sensor_cache[idx].detected  = detected;
}

const SensorDataCache& data_cache_get_sensor(int idx) {
    // 越界访问时返回 [0] 的引用（安全回退）
    if (idx < 0 || idx >= 3) idx = 0;
    return s_sensor_cache[idx];
}
