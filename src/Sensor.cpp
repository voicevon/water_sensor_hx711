#include "Sensor.h"
#include "config.h"
#include <algorithm> // std::max / std::min

// ============================================================
//  构造 & 重置
// ============================================================

Sensor::Sensor(int id, int thresholdOffset)
    : _id(id), _thresholdOffset(thresholdOffset) {
    reset();
}

void Sensor::reset() {
    // ---- 公共状态 ----
    _rawValue        = 0;
    _filteredValue   = 0;
    _baselineValue   = 0;
    _lastState       = SensorState::NO_WATER;
    _hasWaterStartTime = 0;

    // ---- 算法一：DYNAMIC 滑动窗口 ----
    _maHead  = 0;
    _maCount = 0;
    _maSum   = 0;
    for (int i = 0; i < MA_WINDOW; i++) _maBuf[i] = 0;

    _baseHead  = 0;
    _baseCount = 0;
    _baseSum   = 0;
    for (int i = 0; i < BASELINE_WINDOW; i++) _baseBuf[i] = 0;

    // ---- 算法二：DISCRETE 窗口 ----
    _dBaseHead  = 0;
    _dBaseCount = 0;
    _dBaseSum   = 0;
    for (int i = 0; i < DISCRETE_BASELINE_WINDOW; i++) _dBaseBuf[i] = 0;

    _dVarHead  = 0;
    _dVarCount = 0;
    _dVarSum   = 0;
    for (int i = 0; i < DISCRETE_VARIANCE_WINDOW; i++) _dVarBuf[i] = 0;

    _dBaselineValue    = 0;
    _dVarianceSmoothed = 0;

    // ---- 算法三：ENVELOPE 窗口 ----
    _envHead  = 0;
    _envCount = 0;
    for (int i = 0; i < ENV_BUF_MAX; i++) _envBuf[i] = 0;
    _dryBaseline = -1.0f;  // Fix #6 等价：< 0 表示未初始化
    _envUpper    = 0;
    _envLower    = 0;
}

// ============================================================
//  算法切换（切换时重置数据，避免跨算法状态污染）
// ============================================================

void Sensor::setAlgoType(AlgoType type) {
    if (_algoType != type) {
        _algoType = type;
        reset();
    }
}

void Sensor::setEnvWindow(int w) {
    if (w < 1)           w = 1;
    if (w > ENV_BUF_MAX) w = ENV_BUF_MAX;
    _envWindow = w;
}

// ============================================================
//  算法一内部：DYNAMIC 辅助函数
// ============================================================

int32_t Sensor::_pushFilter(int32_t value) {
    if (_maCount < MA_WINDOW) {
        _maBuf[_maHead] = value;
        _maSum += value;
        _maCount++;
    } else {
        _maSum -= _maBuf[_maHead];
        _maBuf[_maHead] = value;
        _maSum += value;
    }
    _maHead = (_maHead + 1) % MA_WINDOW;
    return (int32_t)(_maSum / _maCount);
}

int32_t Sensor::_pushBaseline(int32_t value) {
    if (_baseCount < BASELINE_WINDOW) {
        _baseBuf[_baseHead] = value;
        _baseSum += value;
        _baseCount++;
    } else {
        _baseSum -= _baseBuf[_baseHead];
        _baseBuf[_baseHead] = value;
        _baseSum += value;
    }
    _baseHead = (_baseHead + 1) % BASELINE_WINDOW;
    return (int32_t)(_baseSum / _baseCount);
}

int32_t Sensor::getThreshold() const {
    // DYNAMIC 模式下有意义；其他模式下复用此字段作展示
    int32_t thresh;
    if (_lastState == SensorState::NO_WATER) {
        thresh = _baselineValue + _thresholdOffset;
    } else {
        thresh = _baselineValue - _thresholdOffset;
    }
    return thresh;
}

// ============================================================
//  算法一：DYNAMIC 运行逻辑（含 Fix A & Fix B）
// ============================================================

void Sensor::_runDynamic(int32_t value) {
    _filteredValue = _pushFilter(value);
    _baselineValue = _pushBaseline(_filteredValue);

    // 1. 看门狗（WDT）检查
    if (_lastState == SensorState::HAS_WATER) {
        // Fix B：仅在起始时间为 0 时初始化，不重复赋值
        if (_hasWaterStartTime == 0) {
            _hasWaterStartTime = millis();
        }
        if (millis() - _hasWaterStartTime >= WATER_WATCHDOG_TIMEOUT_MS) {
            Serial.printf("[WDT] 物理通道 %d 持续有水达到 5 小时，强制复位为 DRY！当前值：%ld，基准：%ld\n",
                          _id, (long)_filteredValue, (long)_baselineValue);
            _lastState = SensorState::NO_WATER;
            _hasWaterStartTime = 0;
            _notifyStateChange(SensorState::NO_WATER);
            // Fix A：WDT 触发后立即 return，阻止状态机在同一帧内重新评估并回弹
            return;
        }
    } else {
        _hasWaterStartTime = 0;
    }

    // 2. 施密特双向迟滞状态机
    int32_t curThreshold = getThreshold();
    SensorState nextState = _lastState;

    if (_lastState == SensorState::NO_WATER) {
        // Q2=A：对齐 Python 端——支持负偏移（负偏移时信号下降触发有水）
        if (_thresholdOffset >= 0) {
            nextState = (_filteredValue > curThreshold) ? SensorState::HAS_WATER : SensorState::NO_WATER;
        } else {
            nextState = (_filteredValue < curThreshold) ? SensorState::HAS_WATER : SensorState::NO_WATER;
        }
    } else {
        if (_thresholdOffset >= 0) {
            nextState = (_filteredValue < curThreshold) ? SensorState::NO_WATER : SensorState::HAS_WATER;
        } else {
            nextState = (_filteredValue > curThreshold) ? SensorState::NO_WATER : SensorState::HAS_WATER;
        }
    }

    if (nextState != _lastState) {
        _lastState = nextState;
        _hasWaterStartTime = (nextState == SensorState::HAS_WATER) ? millis() : 0;
        _notifyStateChange(nextState);
    }
}

// ============================================================
//  算法二：DISCRETE（离散方差）运行逻辑
//  对应 Python DiscreteVarianceAlgorithm，O(1) 增量 sum 实现
// ============================================================

void Sensor::_runDiscrete(int32_t value) {
    // 1. 基准线均值（DISCRETE_BASELINE_WINDOW = 200）
    if (_dBaseCount < DISCRETE_BASELINE_WINDOW) {
        _dBaseBuf[_dBaseHead] = value;
        _dBaseSum += value;
        _dBaseCount++;
    } else {
        _dBaseSum -= _dBaseBuf[_dBaseHead];
        _dBaseBuf[_dBaseHead] = value;
        _dBaseSum += value;
    }
    _dBaseHead = (_dBaseHead + 1) % DISCRETE_BASELINE_WINDOW;
    _dBaselineValue = (int32_t)(_dBaseSum / _dBaseCount);

    // 2. 计算平方差 (使用 64 位防平方溢出)
    int64_t diff         = (int64_t)value - (int64_t)_dBaselineValue;
    uint64_t squaredDiff = (uint64_t)(diff * diff);

    // 3. 方差平滑（DISCRETE_VARIANCE_WINDOW = 30）
    if (_dVarCount < DISCRETE_VARIANCE_WINDOW) {
        _dVarBuf[_dVarHead] = squaredDiff;
        _dVarSum += squaredDiff;
        _dVarCount++;
    } else {
        _dVarSum -= _dVarBuf[_dVarHead];
        _dVarBuf[_dVarHead] = squaredDiff;
        _dVarSum += squaredDiff;
    }
    _dVarHead = (_dVarHead + 1) % DISCRETE_VARIANCE_WINDOW;
    _dVarianceSmoothed = _dVarSum / _dVarCount;

    // 4. 阈值判定（无迟滞，与 Python 一致）
    SensorState nextState = (_dVarianceSmoothed > (uint64_t)_varThreshold)
                            ? SensorState::HAS_WATER
                            : SensorState::NO_WATER;

    // 5. 将中间值映射到 filtered/baseline/threshold 供 web 展示
    _filteredValue  = (int32_t)(_dVarianceSmoothed > 2147483647ULL ? 2147483647 : _dVarianceSmoothed);
    _baselineValue  = _dBaselineValue;

    if (nextState != _lastState) {
        _lastState = nextState;
        _notifyStateChange(nextState);
    }
}

// ============================================================
//  算法三：ENVELOPE（包络范围）运行逻辑
//  对应 Python EnvelopeRangeAlgorithm，O(envWindow) max/min 遍历
// ============================================================

void Sensor::_runEnvelope(int32_t value) {
    // 1. 写入环形缓冲区
    _envBuf[_envHead] = value;
    _envHead = (_envHead + 1) % _envWindow;
    if (_envCount < _envWindow) _envCount++;

    // 2. O(envWindow) 遍历求 max/min
    int32_t upper = _envBuf[0];
    int32_t lower = _envBuf[0];
    int startIdx = (_envCount < _envWindow) ? 0 : _envHead;
    for (int i = 0; i < _envCount; i++) {
        int32_t v = _envBuf[(startIdx + i) % _envWindow];
        if (v > upper) upper = v;
        if (v < lower) lower = v;
    }
    _envUpper = upper;
    _envLower = lower;

    float diff = (float)(upper - lower);

    // 3. 无水基准线追踪（EMA，仅在无水时更新）
    if (_lastState == SensorState::NO_WATER) {
        if (_dryBaseline < 0.0f) {
            _dryBaseline = diff;
        } else {
            float alpha;
            if (diff > _dryBaseline) {
                alpha = 2.0f / ((float)_envDryWindowUp + 1.0f);
            } else {
                alpha = 2.0f / ((float)_envDryWindowDown + 1.0f);
            }
            _dryBaseline = alpha * diff + (1.0f - alpha) * _dryBaseline;
        }
    }

    // 4. 施密特滞回触发判定
    float dry = (_dryBaseline >= 0.0f) ? _dryBaseline : 0.0f;
    SensorState nextState = _lastState;
    if (_lastState == SensorState::NO_WATER) {
        if (diff > dry + (float)_envUpperOffset) {
            nextState = SensorState::HAS_WATER;
        }
    } else {
        if (diff < dry + (float)_envLowerOffset) {
            nextState = SensorState::NO_WATER;
        }
    }

    // 5. 将包络值映射到 filtered/baseline 字段供 web 展示
    _filteredValue  = _envUpper;
    _baselineValue  = _envLower;
    _rawValue       = value;

    if (nextState != _lastState) {
        _lastState = nextState;
        _notifyStateChange(nextState);
    }
}

// ============================================================
//  公共接口：pushRaw() — 按 AlgoType 分发
// ============================================================

void Sensor::pushRaw(int32_t value) {
    _rawValue = value;

    switch (_algoType) {
        case AlgoType::DISCRETE:
            _runDiscrete(value);
            break;
        case AlgoType::ENVELOPE:
            _runEnvelope(value);
            break;
        case AlgoType::DYNAMIC:
        default:
            _runDynamic(value);
            break;
    }
}

// ============================================================
//  回调注册 & 通知
// ============================================================

void Sensor::onStateChange(StateChangeCallback cb) {
    _stateChangeCb = cb;
}

void Sensor::_notifyStateChange(SensorState newState) {
    if (_stateChangeCb) {
        _stateChangeCb(_id, newState);
    }
}
