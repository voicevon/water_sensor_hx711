#include <Arduino.h>

#include "config.h"
#include "HX711Sensor.h"
#include "ble_adv.h"
#include "wifi_mqtt.h"
#include "esp_log.h"
#include "Sensor.h"
#include "web_config.h"

// 实例化 3 路 Sensor 状态机（对应 3 路 HX711）
static Sensor s_sensors[3] = {
    Sensor(0), Sensor(1), Sensor(2)
};

// ============================================================
//  顶层调度骨架
//  - setup() : 硬件与通信初始化，依次启动各子系统
//  - loop()  : 维持网络心跳 + 1Hz 定时采样 (HX711) → BLE与MQTT输出
// ============================================================
uint32_t g_mqtt_publish_interval_ms = 1000UL;
unsigned long s_last_mqtt_publish_time = 0;

static unsigned long s_last_send_time   = 0;
static unsigned long s_led_a_off_time   = 0;     // LED A 自动熄灭时间戳（0=常灭）
static bool          s_last_led_b_state = false; // 记录 LED B 上一次状态，避免高频写 GPIO

// ============================================================

void setup() {
    // 彻底关闭 ESP-IDF 底层日志，避免干扰调试输出
    esp_log_level_set("*", ESP_LOG_NONE);

    // 初始化本地调试串口
    Serial.begin(115200);
    delay(500);
    Serial.println("\n====================================");
    Serial.println("ESP32 HX711 Force Sensor Node Start");
    Serial.printf("HX711 Pins: Ch1(DOUT=%d,SCK=%d) Ch2(DOUT=%d,SCK=%d) Ch3(DOUT=%d,SCK=%d)\n",
                  HX711_1_DOUT_PIN, HX711_1_SCK_PIN,
                  HX711_2_DOUT_PIN, HX711_2_SCK_PIN,
                  HX711_3_DOUT_PIN, HX711_3_SCK_PIN);
    Serial.println("====================================");

    // 1. 初始化 LED 引脚
    pinMode(LED_PIN_A, OUTPUT);
    pinMode(LED_PIN_B, OUTPUT);
    digitalWrite(LED_PIN_A, LOW);
    digitalWrite(LED_PIN_B, LOW);

    // 2. 启动 Web 配置服务器（内部会先调 nvs_config_init 加载 NVS）
    web_config_init();

    // 3. 初始化 HX711 传感器（校准参数已由 nvs_config_init 加载）
    bool hx711_ok = HX711_Init_All();
    if (!hx711_ok) {
        Serial.println("[ERROR] HX711 Init Failed! Check wiring. Flashing LED B...");
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_PIN_B, HIGH); delay(100);
            digitalWrite(LED_PIN_B, LOW);  delay(100);
        }
    }

    // 4. 从 NVS 恢复 3 路 Sensor 状态机参数
    for (int i = 0; i < 3; i++) {
        s_sensors[i].setThresholdOffset(get_channel_threshold(i));
        s_sensors[i].setAlgoType((AlgoType)get_algo_type(i));
        s_sensors[i].setVarThreshold(get_var_threshold(i));
        s_sensors[i].setEnvWindow(get_env_window(i));
        s_sensors[i].setEnvDryWindowUp(get_env_dry_up(i));
        s_sensors[i].setEnvDryWindowDown(get_env_dry_down(i));
        s_sensors[i].setEnvUpperOffset(get_env_upper_offset(i));
        s_sensors[i].setEnvLowerOffset(get_env_lower_offset(i));
    }

    // 5. 启动网络通信与 BLE 广播
    wifi_mqtt_init();
    ble_init();
}

// ============================================================

void loop() {
    unsigned long now = millis();

    // 驱动 Web Server
    web_config_loop();

    // 维持 WiFi / MQTT 心跳（非阻塞重连）
    wifi_mqtt_loop(now);

    // LED A: MQTT 发送指示灯。每发送成功一次点亮 100ms 后关闭。
    if (s_led_a_off_time > 0) {
        if (now >= s_led_a_off_time) {
            s_led_a_off_time = 0;
            digitalWrite(LED_PIN_A, LOW);
        } else {
            digitalWrite(LED_PIN_A, HIGH);
        }
    } else {
        digitalWrite(LED_PIN_A, LOW);
    }

    // LED B 状态指示灯：
    // - WiFi 未连接：常灭
    // - WiFi 已连接，MQTT 未连接：2s 周期中闪
    // - WiFi+MQTT 均已连接：1s 周期快闪
    bool led_b_state = false;
    if (wifi_is_connected()) {
        if (mqtt_is_connected()) {
            led_b_state = ((now % 1000) < 500);
        } else {
            led_b_state = ((now % 2000) < 1000);
        }
    }

    if (led_b_state != s_last_led_b_state) {
        s_last_led_b_state = led_b_state;
        digitalWrite(LED_PIN_B, led_b_state ? HIGH : LOW);
    }

    // 1Hz 非阻塞定时器
    if (now - s_last_send_time >= SEND_INTERVAL_MS) {
        s_last_send_time = now;

        // 1. 读取 3 路 HX711 力值
        float all_grams[3] = { 0.0f, 0.0f, 0.0f };
        bool read_ok = HX711_Read_All(all_grams);

        if (!read_ok) {
            Serial.println("[main] Warning: HX711 Read all channels failed!");
        } else {
            // 2. 将力值馈入 3 路 Sensor 状态机，并同步给 Web config 缓存
            for (int i = 0; i < 3; i++) {
                // 实时应用网页端配置
                s_sensors[i].setThresholdOffset(get_channel_threshold(i));
                s_sensors[i].setAlgoType((AlgoType)get_algo_type(i));
                s_sensors[i].setVarThreshold(get_var_threshold(i));
                s_sensors[i].setEnvWindow(get_env_window(i));
                s_sensors[i].setEnvDryWindowUp(get_env_dry_up(i));
                s_sensors[i].setEnvDryWindowDown(get_env_dry_down(i));
                s_sensors[i].setEnvUpperOffset(get_env_upper_offset(i));
                s_sensors[i].setEnvLowerOffset(get_env_lower_offset(i));

                // convert_to_force: 克力 → uint16_t（0.1g 单位）
                uint16_t raw_val = convert_to_force(all_grams[i]);
                s_sensors[i].pushRaw(raw_val);

                web_config_update_sensor(i,
                    all_grams[i],
                    s_sensors[i].getFiltered(),
                    s_sensors[i].getBaseline(),
                    s_sensors[i].getThreshold(),
                    s_sensors[i].isDetected());
            }
        }

        if (read_ok) {
            // 3. 组装 BLE 与 MQTT 输出数组
            uint16_t out_sensors[SENSOR_COUNT] = { 0, 0, 0 };
            bool     out_states[SENSOR_COUNT]  = { false, false, false };

            for (int i = 0; i < SENSOR_COUNT; i++) {
                out_sensors[i] = convert_to_force(all_grams[i]);
                out_states[i]  = s_sensors[i].isDetected();
            }

            // 计算状态字节（低 3 位对应 3 路触发状态）
            uint8_t state_byte = 0;
            for (int i = 0; i < SENSOR_COUNT; i++) {
                if (out_states[i]) state_byte |= (1 << i);
            }

            // 4. 并行输出至 BLE 与 MQTT
            ble_update(out_sensors, out_states);

            if (now - s_last_mqtt_publish_time >= g_mqtt_publish_interval_ms) {
                s_last_mqtt_publish_time = now;
                if (mqtt_publish(out_sensors, state_byte)) {
                    s_led_a_off_time = now + 100;
                    digitalWrite(LED_PIN_A, HIGH);
                }
            }

            // 5. 本地串口诊断日志
            Serial.println("----------------------------------------");
            Serial.println("CH  RAW(g)    RAW_U16  FILTERED BASELINE THRESHOLD  STATE");
            for (int i = 0; i < 3; i++) {
                Serial.printf("  %d  %8.2f  %-7u  %-8u %-8u %-9u  %s\n",
                              i,
                              all_grams[i],
                              convert_to_force(all_grams[i]),
                              s_sensors[i].getFiltered(),
                              s_sensors[i].getBaseline(),
                              s_sensors[i].getThreshold(),
                              s_sensors[i].isDetected() ? "TRIGGERED" : "IDLE");
            }
        }
    }
}
