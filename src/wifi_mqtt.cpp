#include "wifi_mqtt.h"
#include "config.h"
#include <ESP32WifiMqttManager.h>
#include "web_config.h"
#include <ArduinoJson.h>

// 模块内部网络对象
static WiFiClient           s_espClient;
static ESP32WifiMqttManager s_netManager(s_espClient);

static bool s_mqtt_send_enabled = false;

static void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("[MQTT RX] Topic: ");
    Serial.println(topic);
    if (strcmp(topic, MQTT_CONTROL_TOPIC) == 0) {
        // 安全拷贝，防溢出 (JSON 长度增大)
        char payload_str[128];
        unsigned int len = length < 127 ? length : 127;
        memcpy(payload_str, payload, len);
        payload_str[len] = '\0';
        
        Serial.print("[MQTT RX] Payload: ");
        Serial.println(payload_str);
        
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, payload_str);
        if (error) {
            Serial.print("[MQTT] JSON parse error: ");
            Serial.println(error.c_str());
            return;
        }
        
        // 1. 优先处理全局 stop 命令
        const char* command = doc["command"];
        if (command && strcmp(command, "stop") == 0) {
            s_mqtt_send_enabled = false;
            Serial.println("[MQTT] Global DEBUG DISABLED");
            return;
        }
        
        // 2. 处理特定名称节点的启动调试逻辑
        const char* name = doc["name"];
        if (name && strcmp(name, get_device_name().c_str()) == 0) {
            s_mqtt_send_enabled = true;
            int interval_sec = doc["interval"] | 1;
            
            extern uint32_t g_mqtt_publish_interval_ms;
            g_mqtt_publish_interval_ms = interval_sec * 1000UL;
            
            extern unsigned long s_last_mqtt_publish_time;
            s_last_mqtt_publish_time = 0; // 重置计时器使其立即发送一次
            
            Serial.printf("[MQTT] DEBUG ENABLED, interval: %d s\n", interval_sec);
        }
    }
}

// ============================================================

void wifi_mqtt_init() {
    Serial.println();
    Serial.print("[WiFi] Connecting to STA SSID: ");
    static String target_ssid;  // 用 static 维持生命周期以传递给 PubSubClient 内部的 c_str() 指针
    static String target_pass;
    target_ssid = get_sta_ssid();
    target_pass = get_sta_password();
    Serial.println(target_ssid);

    NetworkConfig config;
    config.wifiSsid = target_ssid.c_str();
    config.wifiPassword = target_pass.c_str();
    static String target_broker;
    target_broker = get_mqtt_broker();
    config.mqttBroker = target_broker.c_str();
    config.mqttPort = get_mqtt_port();
    config.mqttUsername = nullptr;
    config.mqttPassword = nullptr;
    config.clientIdPrefix = "ESP32Client";
    config.wifiReconnectIntervalMs = 20000;
    config.mqttReconnectIntervalMs = MQTT_RECONNECT_INTERVAL_MS;
    config.useHttpDns = true;

    s_netManager.begin(config);

    s_netManager.setMqttCallback(mqtt_callback);

    s_netManager.onStateChange([](NetworkState state) {
        if (state == STATE_MQTT_CONNECTED) {
            s_netManager.subscribe(MQTT_CONTROL_TOPIC);
        }
    });

    // 阻塞等待连接，最多 20 次 × 500ms = 10s
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.print("[WiFi] Connected. IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println();
        Serial.println("[WiFi] Connect failed. Will retry in background.");
    }
}

// ============================================================

void wifi_mqtt_loop(unsigned long current_time) {
    s_netManager.loop();
}

// ============================================================

bool mqtt_publish(const uint16_t *sensors, uint8_t stateByte) {
    if (!s_mqtt_send_enabled) {
        return false; // 未使能时静默跳过
    }
    if (!s_netManager.isMqttConnected()) {
        return false; // 断线时静默跳过，不阻塞主循环
    }

    char json_buf[256];
    // 统一上报：sensor1 到 sensor3，增加 state 字段
    snprintf(json_buf, sizeof(json_buf),
             "{\"name\":\"%s\", \"sensor1\":%u, \"sensor2\":%u, \"sensor3\":%u, \"state\":%u}",
             get_device_name().c_str(), sensors[0], sensors[1], sensors[2], stateByte);

    Serial.printf("[MQTT Publish] Topic: %s, Payload: %s\n", MQTT_STATUS_TOPIC, json_buf);

    return s_netManager.publish(MQTT_STATUS_TOPIC, json_buf);
}

bool mqtt_is_connected() {
    return s_netManager.isMqttConnected();
}

bool wifi_is_connected() {
    return s_netManager.isWifiConnected();
}
