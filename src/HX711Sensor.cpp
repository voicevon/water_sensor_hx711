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
static int32_t s_last_raw[3] = { 0, 0, 0 };

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
    Serial.printf("[HX711] Initializing 3 channels (24-bit raw mode)...\n");
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

        // 无校准参数：直接使用 24 位原生原始值
        s_online[ch] = true;
        online_count++;
        Serial.printf("[HX711] Ch%d online (24-bit raw signed mode).\n", ch + 1);
    }

    Serial.printf("[HX711] Init complete. %d/3 channels online.\n", online_count);
    return (online_count > 0);
}

/* ============================================================ */

bool HX711_Read_All(int32_t* out_raw) {
    bool any_ok = false;

    for (int ch = 0; ch < 3; ch++) {
        // 离线通道输出 0 并跳过
        if (!s_online[ch]) {
            out_raw[ch] = 0;
            continue;
        }

        if (!wait_ready(ch)) {
            Serial.printf("[HX711] Ch%d read timeout, using last value.\n", ch + 1);
            out_raw[ch] = s_last_raw[ch];
            continue;
        }

        // 直接取 24 位有符号原生值
        long raw = s_hx711[ch].read();
        int32_t val = (int32_t)raw;
        s_last_raw[ch] = val;
        out_raw[ch]    = val;
        any_ok = true;
    }

    return any_ok;
}

/* ============================================================ */

void HX711_SelfCheckBaseline(void) {
    Serial.printf("[HX711] Self-check: verifying 24-bit baseline health...\n");

    for (int ch = 0; ch < 3; ch++) {
        if (!s_online[ch]) continue;

        int64_t sum = 0;
        int ok_count = 0;
        for (int k = 0; k < HX711_SELFCHECK_SAMPLES; k++) {
            if (wait_ready(ch)) {
                sum += (int64_t)s_hx711[ch].read();
                ok_count++;
            }
        }
        if (ok_count == 0) {
            Serial.printf("[HX711] Ch%d self-check: no samples.\n", ch + 1);
            continue;
        }

        int32_t baseline = (int32_t)(sum / ok_count);
        Serial.printf("[HX711] Ch%d 24-bit baseline = %ld (%d samples)\n",
                      ch + 1, (long)baseline, ok_count);

        if (baseline >= 8380000L || baseline <= -8380000L) {
            Serial.printf("[HX711] WARNING: Ch%d baseline %ld is saturated near 24-bit limits!\n",
                          ch + 1, (long)baseline);
            Serial.printf("[HX711] Check sensor wiring or mechanical load.\n");
        }
    }
}

/* ============================================================ */

bool HX711_IsOnline(int ch) {
    if (ch < 0 || ch >= 3) return false;
    return s_online[ch];
}
