#include "HX711Sensor.h"
#include "config.h"
#include "nvs_config.h"
#include <HX711.h>

/* ============================================================
 *  HX711 实例（3 路独立传感器）
 * ============================================================ */
static HX711 s_hx711[3];

// 引脚配置：[DOUT, SCK]
static const uint8_t s_dout_pins[3] = { HX711_1_DOUT_PIN, HX711_2_DOUT_PIN, HX711_3_DOUT_PIN };
static const uint8_t s_sck_pins[3]  = { HX711_1_SCK_PIN,  HX711_2_SCK_PIN,  HX711_3_SCK_PIN  };

// 各通道在线状态
static bool s_online[3] = { false, false, false };

// 上一次有效力值（读取失败时保持）
static float s_last_grams[3] = { 0.0f, 0.0f, 0.0f };

// HX711 采集等待超时（毫秒）
static const uint32_t HX711_READY_TIMEOUT_MS = 500;

/* ============================================================
 *  内部辅助：等待 HX711 就绪（非阻塞超时）
 * ============================================================ */
static bool wait_ready(int ch) {
    uint32_t start = millis();
    while (!s_hx711[ch].is_ready()) {
        if (millis() - start > HX711_READY_TIMEOUT_MS) {
            return false;
        }
        delay(1);
    }
    return true;
}

/* ============================================================
 *  公共接口实现
 * ============================================================ */

bool HX711_Init_All(void) {
    Serial.printf("[HX711] Initializing 3 channels...\n");
    Serial.printf("[HX711] Ch1: DOUT=%d SCK=%d | Ch2: DOUT=%d SCK=%d | Ch3: DOUT=%d SCK=%d\n",
                  s_dout_pins[0], s_sck_pins[0],
                  s_dout_pins[1], s_sck_pins[1],
                  s_dout_pins[2], s_sck_pins[2]);

    int online_count = 0;

    for (int ch = 0; ch < 3; ch++) {
        s_hx711[ch].begin(s_dout_pins[ch], s_sck_pins[ch]);

        // 通道已被用户禁用（disable），跳过初始化
        if (!get_hx711_enabled(ch)) {
            Serial.printf("[HX711] Ch%d DISABLED, skipping init.\n", ch + 1);
            s_online[ch] = false;
            continue;
        }

        // 等待 HX711 就绪（最多 500ms）
        if (!wait_ready(ch)) {
            Serial.printf("[HX711] Ch%d not ready (DOUT=%d, SCK=%d), skipping.\n",
                          ch + 1, s_dout_pins[ch], s_sck_pins[ch]);
            s_online[ch] = false;
            continue;
        }

        // 从 NVS 加载校准参数
        float scale  = get_hx711_scale(ch);
        long  tare   = get_hx711_tare(ch);

        s_hx711[ch].set_scale(scale);
        s_hx711[ch].set_offset(tare);

        s_online[ch] = true;
        online_count++;

        Serial.printf("[HX711] Ch%d online. scale=%.4f, tare=%ld\n",
                      ch + 1, scale, tare);
    }

    Serial.printf("[HX711] Init complete. %d/3 channels online.\n", online_count);
    return (online_count > 0);
}

/* ============================================================ */

bool HX711_Read_All(float* out_grams) {
    bool any_ok = false;

    for (int ch = 0; ch < 3; ch++) {
        // 通道已关闭，输出 0 并跳过
        if (!get_hx711_enabled(ch)) {
            out_grams[ch] = 0.0f;
            continue;
        }

        if (!s_online[ch]) {
            out_grams[ch] = 0.0f;
            continue;
        }

        if (!wait_ready(ch)) {
            Serial.printf("[HX711] Ch%d read timeout, using last value.\n", ch + 1);
            out_grams[ch] = s_last_grams[ch];
            continue;
        }

        // get_units() 已应用 scale 和 offset，返回克力
        float val = s_hx711[ch].get_units(1);
        s_last_grams[ch] = val;
        out_grams[ch]    = val;
        any_ok = true;
    }

    return any_ok;
}

/* ============================================================ */

void HX711_Tare(int ch, int times) {
    if (ch < 0 || ch >= 3 || !s_online[ch]) return;

    Serial.printf("[HX711] Taring Ch%d (%d samples)...\n", ch + 1, times);

    if (!wait_ready(ch)) {
        Serial.printf("[HX711] Ch%d not ready for tare.\n", ch + 1);
        return;
    }

    s_hx711[ch].tare(times);
    long new_offset = s_hx711[ch].get_offset();

    // 持久化到 NVS
    nvs_set_hx711_tare(ch, new_offset);

    Serial.printf("[HX711] Ch%d tare done. new_offset=%ld\n", ch + 1, new_offset);
}

/* ============================================================ */

void HX711_SetScale(int ch, float scale) {
    if (ch < 0 || ch >= 3) return;

    s_hx711[ch].set_scale(scale);

    // 持久化到 NVS
    nvs_set_hx711_scale(ch, scale);

    Serial.printf("[HX711] Ch%d scale set to %.4f\n", ch + 1, scale);
}

/* ============================================================ */

bool HX711_IsOnline(int ch) {
    if (ch < 0 || ch >= 3) return false;
    return s_online[ch];
}
