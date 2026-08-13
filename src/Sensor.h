#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>

/**
 * @brief 传感器有水/无水状态枚举
 */
enum class SensorState {
    NO_WATER,   // 无水状态
    HAS_WATER   // 有水状态
};

/**
 * @brief 算法选择枚举
 *   DYNAMIC  — 施密特双向迟滞 + 滑动均值滤波（默认）
 *   DISCRETE — 离散方差算法（基准线均值 + 平方差平滑 + 阈值触发）
 *   ENVELOPE — 包络范围算法（窗口内 max/min 差值 + 自适应 dry_baseline）
 */
enum class AlgoType : uint8_t {
    DYNAMIC  = 0,
    DISCRETE = 1,
    ENVELOPE = 2
};

// ---- 包络算法可配置参数上限 ----
static const int ENV_BUF_MAX = 120;  // 包络窗口最大值（Q3：最大 120 点）

/**
 * @brief 传感器通道控制类
 * 独立维护各通道的滤波、慢速环境基准线追踪以及施密特双向触发器状态机。
 * 现在支持三套算法：DYNAMIC / DISCRETE / ENVELOPE，可在运行时通过 NVS 切换。
 */
class Sensor {
public:
    // 状态跳转回调：参数包含传感器ID和改变后的状态
    typedef void (*StateChangeCallback)(int sensorId, SensorState newState);

    Sensor(int id, int thresholdOffset = 50);

    /**
     * @brief 喂入最新的传感器原始电容值，返回更新后的通道状态
     */
    void pushRaw(uint16_t value);

    // ---- 通用 getter ----
    int getId() const { return _id; }
    uint16_t getRaw() const { return _rawValue; }
    uint16_t getFiltered() const { return _filteredValue; }
    uint16_t getBaseline() const { return _baselineValue; }
    uint16_t getThreshold() const;
    SensorState getState() const { return _lastState; }
    bool isDetected() const { return _lastState == SensorState::HAS_WATER; }

    // ---- 动态阈值算法参数 ----
    void setThresholdOffset(int offset) { _thresholdOffset = offset; }
    int getThresholdOffset() const { return _thresholdOffset; }

    // ---- 算法类型 ----
    void setAlgoType(AlgoType type);
    AlgoType getAlgoType() const { return _algoType; }

    // ---- 离散方差算法参数 ----
    void setVarThreshold(int threshold) { _varThreshold = threshold; }
    int getVarThreshold() const { return _varThreshold; }

    // ---- 包络算法参数 ----
    void setEnvWindow(int w);             // 1 ~ ENV_BUF_MAX
    void setEnvDryWindowUp(int w)   { _envDryWindowUp   = w; }
    void setEnvDryWindowDown(int w) { _envDryWindowDown = w; }
    void setEnvUpperOffset(int o)   { _envUpperOffset   = o; }
    void setEnvLowerOffset(int o)   { _envLowerOffset   = o; }

    int getEnvWindow()      const { return _envWindow; }
    int getEnvDryWindowUp() const { return _envDryWindowUp; }
    int getEnvDryWindowDown()const{ return _envDryWindowDown; }
    int getEnvUpperOffset() const { return _envUpperOffset; }
    int getEnvLowerOffset() const { return _envLowerOffset; }

    // 注册状态变化回调
    void onStateChange(StateChangeCallback cb);

    // 重置通道状态及滤波数据
    void reset();

private:
    int _id;
    AlgoType _algoType = AlgoType::DYNAMIC;

    // ==========================================
    //  算法一：动态施密特阈值（DYNAMIC）
    // ==========================================
    int _thresholdOffset;
    uint16_t _rawValue     = 0;
    uint16_t _filteredValue = 0;
    uint16_t _baselineValue = 0;
    SensorState _lastState = SensorState::NO_WATER;
    unsigned long _hasWaterStartTime = 0;
    StateChangeCallback _stateChangeCb = nullptr;

    // 滑动滤波器环形缓冲区
    static const int MA_WINDOW       = 50;
    static const int BASELINE_WINDOW = 200;

    uint16_t _maBuf[MA_WINDOW];
    int      _maHead  = 0;
    int      _maCount = 0;
    uint32_t _maSum   = 0;

    uint16_t _baseBuf[BASELINE_WINDOW];
    int      _baseHead  = 0;
    int      _baseCount = 0;
    uint32_t _baseSum   = 0;

    uint16_t _pushFilter(uint16_t value);
    uint16_t _pushBaseline(uint16_t value);

    // ==========================================
    //  算法二：离散方差（DISCRETE）
    // ==========================================
    int _varThreshold = 5000;  // 方差触发阈值（默认 5000，对应 Python 默认值）

    static const int DISCRETE_BASELINE_WINDOW = 200;
    static const int DISCRETE_VARIANCE_WINDOW = 30;

    uint16_t _dBaseBuf[DISCRETE_BASELINE_WINDOW];
    int      _dBaseHead  = 0;
    int      _dBaseCount = 0;
    uint32_t _dBaseSum   = 0;

    uint32_t _dVarBuf[DISCRETE_VARIANCE_WINDOW];
    int      _dVarHead  = 0;
    int      _dVarCount = 0;
    uint64_t _dVarSum   = 0;

    uint16_t _dBaselineValue   = 0;
    uint32_t _dVarianceSmoothed = 0;

    void _runDiscrete(uint16_t value);

    // ==========================================
    //  算法三：包络范围（ENVELOPE）
    // ==========================================
    int _envWindow        = 30;
    int _envDryWindowUp   = 1000;
    int _envDryWindowDown = 1000;
    int _envUpperOffset   = 500;
    int _envLowerOffset   = 300;

    uint16_t _envBuf[ENV_BUF_MAX];
    int      _envHead  = 0;
    int      _envCount = 0;

    float _dryBaseline    = -1.0f;   // < 0 表示"未初始化"（对应 Python Fix #6 的 None）
    uint16_t _envUpper    = 0;
    uint16_t _envLower    = 0;

    void _runEnvelope(uint16_t value);

    // ==========================================
    //  公共内部实现
    // ==========================================
    void _runDynamic(uint16_t value);
    void _notifyStateChange(SensorState newState);
};

#endif // SENSOR_H
