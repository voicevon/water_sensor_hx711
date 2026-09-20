#include "wifi_mqtt.h"
#include "config.h"
#include "web_config.h"
#include "nvs_config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <atomic>

// ============================================================
//  RAII 互斥锁辅助类（保证多任务访问 PubSubClient 线程安全）
// ============================================================
class MqttLock {
public:
    MqttLock(SemaphoreHandle_t mutex) : _mutex(mutex), _locked(false) {
        if (_mutex) {
            _locked = (xSemaphoreTake(_mutex, pdMS_TO_TICKS(3000)) == pdTRUE);
            if (!_locked) {
                Serial.println("[MQTT] WARN: MqttLock timeout (3s), skipping operation.");
            }
        }
    }
    ~MqttLock() {
        if (_mutex && _locked) {
            xSemaphoreGive(_mutex);
        }
    }
    bool locked() const { return _locked; }
private:
    SemaphoreHandle_t _mutex;
    bool _locked;
};

// 模块内部网络对象与并发控制
static WiFiClient           s_espClient;
static PubSubClient         s_mqttClient(s_espClient);
static SemaphoreHandle_t    s_mqttMutex = NULL;

static IPAddress            s_resolved_broker_ip = IPAddress(0, 0, 0, 0);
static std::atomic<bool>    s_mqtt_connecting(false);
static unsigned long        s_last_dns_resolve_ms = 0;
static unsigned long        s_last_mqtt_reconnect_attempt = 0;
static unsigned long        s_last_wifi_reconnect_attempt = 0;

static bool                 s_mqtt_send_enabled = false;

// ============================================================
//  标准 DNS 解析（支持花生壳等 DDNS 动态域名）
// ============================================================
static IPAddress resolve_broker_ip() {
    IPAddress resolvedIP;
    String broker = get_mqtt_broker();
    
    // 1. 若配置本身为有效 IP 地址，直接转换返回
    if (resolvedIP.fromString(broker.c_str())) {
        return resolvedIP;
    }
    
    // 2. 域名 DNS 解析（标准 DNS 查询）
    if (WiFi.hostByName(broker.c_str(), resolvedIP)) {
        Serial.printf("[DNS] Successfully resolved %s to %s via standard DNS\n", 
                      broker.c_str(), resolvedIP.toString().c_str());
        return resolvedIP;
    } else {
        Serial.printf("[DNS] Standard DNS failed for %s\n", broker.c_str());
        return IPAddress(0, 0, 0, 0);
    }
}

// ============================================================
//  MQTT 命令接收回调（JSON 解析）
// ============================================================
static void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("[MQTT RX] Topic: ");
    Serial.println(topic);
    if (strcmp(topic, MQTT_CONTROL_TOPIC) == 0) {
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
//  后台异步 DNS 解析与 MQTT 连接任务（FreeRTOS Task）
// ============================================================
void mqtt_connect_task(void* pvParameters) {
    unsigned long now = millis();
    // 若尚未解析成功过，或距离上次解析超过 5 分钟（注：断线时已将 IP 置零，将立即触发解析）
    if (s_resolved_broker_ip[0] == 0 || (now - s_last_dns_resolve_ms > 300000)) {
        s_last_dns_resolve_ms = now;
        IPAddress tempIP = resolve_broker_ip();
        if (tempIP[0] != 0) {
            s_resolved_broker_ip = tempIP;
            Serial.printf("[MQTT Task] DNS resolved IP: %s\n", tempIP.toString().c_str());
        } else {
            Serial.println("[MQTT Task] DNS resolution failed, will fallback to domain.");
        }
    }

    {
        MqttLock lock(s_mqttMutex);
        if (lock.locked()) {
            // 配置 Broker 地址与端口
            if (s_resolved_broker_ip[0] != 0) {
                s_mqttClient.setServer(s_resolved_broker_ip, get_mqtt_port());
            } else {
                s_mqttClient.setServer(get_mqtt_broker().c_str(), get_mqtt_port());
            }

            // 构造随机 Client ID
            String clientId = "ESP32Client-";
            clientId += String(random(0xffff), HEX);
            Serial.println("[MQTT Task] Attempting connection to Broker...");

            // 执行阻塞连接
            bool success = s_mqttClient.connect(clientId.c_str());
            if (success) {
                Serial.println("[MQTT Task] Connected successfully!");
                // 核心关键：握手成功当场在锁内原子订阅控制主题！
                s_mqttClient.subscribe(MQTT_CONTROL_TOPIC);
                Serial.printf("[MQTT Task] Subscribed to topic: %s\n", MQTT_CONTROL_TOPIC);
            } else {
                Serial.printf("[MQTT Task] Connection failed, state = %d\n", s_mqttClient.state());
                // 连接失败，主动将 IP 清零，下次重连时重新解析 DNS
                s_resolved_broker_ip = IPAddress(0, 0, 0, 0);
                // 主动清理旧 socket，避免底层句柄泄露
                s_espClient.stop();
            }
        }
    }

    s_mqtt_connecting = false;
    vTaskDelete(NULL); // 任务完成，自我销毁
}

// ============================================================
//  网络层初始化
// ============================================================
void wifi_mqtt_init() {
    if (s_mqttMutex == NULL) {
        s_mqttMutex = xSemaphoreCreateMutex();
    }

    s_mqttClient.setCallback(mqtt_callback);

    String target_ssid = get_sta_ssid();
    String target_pass = get_sta_password();
    Serial.printf("\n[WiFi] Target SSID to connect: \"%s\"\n", target_ssid.c_str());

    // 保持 AP_STA 模式，确保 Web 配置后台热点不被关闭
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(target_ssid.c_str(), target_pass.c_str());

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

    // 启动初始 IP 解析与配置
    IPAddress brokerIP = resolve_broker_ip();
    int port = get_mqtt_port();
    String broker = get_mqtt_broker();
    if (brokerIP[0] != 0) {
        s_resolved_broker_ip = brokerIP;
        s_last_dns_resolve_ms = millis();
        s_mqttClient.setServer(brokerIP, port);
        Serial.printf("[MQTT] Server set to resolved IP: %s:%d\n", brokerIP.toString().c_str(), port);
    } else {
        s_mqttClient.setServer(broker.c_str(), port);
        Serial.printf("[MQTT] DNS resolution failed, fallback to domain: %s:%d\n", broker.c_str(), port);
    }
}

// ============================================================
//  主循环中非阻塞维持 WiFi 与 MQTT 心跳
// ============================================================
void wifi_mqtt_loop(unsigned long current_time) {
    // 1. 维护 WiFi 自动重连
    if (WiFi.status() != WL_CONNECTED) {
        if (current_time - s_last_wifi_reconnect_attempt >= 20000UL) {
            s_last_wifi_reconnect_attempt = current_time;
            Serial.println("[WiFi] Disconnected. Reconnecting...");
            WiFi.begin(get_sta_ssid().c_str(), get_sta_password().c_str());
        }
        return;
    } else {
        s_last_wifi_reconnect_attempt = current_time;
    }

    // 2. 关键避让：后台连接任务执行期间，主线程直接退出，绝不触碰 s_mqttClient
    if (s_mqtt_connecting) {
        return;
    }

    // 3. 检查 MQTT 连接状态
    bool connected = false;
    {
        MqttLock lock(s_mqttMutex);
        if (lock.locked()) {
            connected = s_mqttClient.connected();
        }
    }

    if (!connected) {
        // 断线时清空已解析的 IP 缓存，下次重连立即刷新 DDNS 域名解析
        s_resolved_broker_ip = IPAddress(0, 0, 0, 0);

        if (current_time - s_last_mqtt_reconnect_attempt >= MQTT_RECONNECT_INTERVAL_MS) {
            s_last_mqtt_reconnect_attempt = current_time;
            s_mqtt_connecting = true;

            BaseType_t ret = xTaskCreate(mqtt_connect_task, "mqtt_async_conn", 8192, NULL, 1, NULL);
            if (ret != pdPASS) {
                s_mqtt_connecting = false;
                Serial.println("[MQTT] Error: Failed to create MQTT task!");
            }
        }
    } else {
        // 已连接状态，维持 MQTT loop 保活
        MqttLock lock(s_mqttMutex);
        if (lock.locked()) {
            s_mqttClient.loop();
        }
    }
}

// ============================================================
//  发布传感器数据
// ============================================================
bool mqtt_publish(const uint16_t *sensors, uint8_t stateByte) {
    if (!s_mqtt_send_enabled) {
        return false; // 未收到启动命令使能时静默跳过
    }
    if (s_mqtt_connecting) {
        return false; // 后台连接中，主线程避让跳过
    }

    MqttLock lock(s_mqttMutex);
    if (!lock.locked() || !s_mqttClient.connected()) {
        return false; // 断线或锁超时，静默跳过
    }

    char json_buf[256];
    snprintf(json_buf, sizeof(json_buf),
             "{\"name\":\"%s\", \"sensor1\":%u, \"sensor2\":%u, \"sensor3\":%u, \"state\":%u}",
             get_device_name().c_str(), sensors[0], sensors[1], sensors[2], stateByte);

    Serial.printf("[MQTT Publish] Topic: %s, Payload: %s\n", MQTT_STATUS_TOPIC, json_buf);
    return s_mqttClient.publish(MQTT_STATUS_TOPIC, json_buf);
}

// ============================================================
//  状态查询接口
// ============================================================
bool mqtt_is_connected() {
    if (s_mqtt_connecting) {
        return false;
    }
    MqttLock lock(s_mqttMutex);
    return lock.locked() && s_mqttClient.connected();
}

bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}
