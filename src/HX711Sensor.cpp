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

// 上一次有效读数（读取失败时保持）
static uint16_t s_last_u16[3] = { 0, 0, 0 };

// HX711 采集等待超时（毫秒）
static const uint32_t HX711_READY_TIMEOUT_MS = 500;

// 启动自检采样次数
static const int HX711_SELFCHECK_SAMPLES = 5;

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

        // 等待 HX711 就绪（最多 500ms）；未接线/损坏的通道自动标记离线
        if (!wait_ready(ch)) {
            Serial.printf("[HX711] Ch%d not ready (DOUT=%d, SCK=%d), skipping.\n",
                          ch + 1, s_dout_pins[ch], s_sck_pins[ch]);
            s_online[ch] = false;
            continue;
        }

        // 无校准参数：直接使用原始值，不做去皮与克数换算
        s_online[ch] = true;
        online_count++;
        Serial.printf("[HX711] Ch%d online (raw mode, no tare/scale).\n", ch + 1);
    }

    Serial.printf("[HX711] Init complete. %d/3 channels online. shift N=%d\n",
                  online_count, get_shift_n());
    return (online_count > 0);
}

/* ============================================================ */

bool HX711_Read_All(uint16_t* out_u16) {
    bool any_ok = false;
    uint8_t shift_n = get_shift_n();

    for (int ch = 0; ch < 3; ch++) {
        // 离线通道输出 0 并跳过
        if (!s_online[ch]) {
            out_u16[ch] = 0;
            continue;
        }

        if (!wait_ready(ch)) {
            Serial.printf("[HX711] Ch%d read timeout, using last value.\n", ch + 1);
            out_u16[ch] = s_last_u16[ch];
            continue;
        }

        // 直接取 24 位原始值，立即按两步公式转换为 uint16_t
        long raw = s_hx711[ch].read();
        uint16_t val = hx711_raw_to_u16((int32_t)raw, shift_n);
        s_last_u16[ch] = val;
        out_u16[ch]    = val;
        any_ok = true;
    }

    return any_ok;
}

/* ============================================================ */

void HX711_SelfCheckBaseline(void) {
    uint8_t shift_n = get_shift_n();
    Serial.printf("[HX711] Self-check: baseline valid range %d~%d (N=%d)\n",
                  HX711_BASELINE_CHECK_LOW, HX711_BASELINE_CHECK_HIGH, shift_n);

    for (int ch = 0; ch < 3; ch++) {
        if (!s_online[ch]) continue;

        uint32_t sum = 0;
        int ok_count = 0;
        for (int k = 0; k < HX711_SELFCHECK_SAMPLES; k++) {
            if (wait_ready(ch)) {
                sum += (uint32_t)hx711_raw_to_u16((int32_t)s_hx711[ch].read(), shift_n);
                ok_count++;
            }
        }
        if (ok_count == 0) {
            Serial.printf("[HX711] Ch%d self-check: no samples.\n", ch + 1);
            continue;
        }

        uint16_t baseline = (uint16_t)(sum / ok_count);
        Serial.printf("[HX711] Ch%d baseline = %u (%d samples)\n",
                      ch + 1, baseline, ok_count);

        if (baseline < HX711_BASELINE_CHECK_LOW || baseline > HX711_BASELINE_CHECK_HIGH) {
            Serial.printf("[HX711] WARNING: Ch%d baseline %u out of range [%d, %d]!\n",
                          ch + 1, baseline, HX711_BASELINE_CHECK_LOW, HX711_BASELINE_CHECK_HIGH);
            Serial.printf("[HX711] Current N=%d does not match actual zero offset. "
                          "Adjust N in web config and reboot.\n", shift_n);
        }
    }
}

/* ============================================================ */

bool HX711_IsOnline(int ch) {
    if (ch < 0 || ch >= 3) return false;
    return s_online[ch];
}
