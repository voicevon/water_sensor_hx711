#include "web_config.h"
#include "nvs_config.h"
#include "data_cache.h"
#include "index_html.h"
#include "config.h"
#include "wifi_mqtt.h"
#include "HX711Sensor.h"
#include <WiFi.h>
#include <WebServer.h>

// ============================================================
//  Web 服务器实例（服务层私有）
// ============================================================
static WebServer s_server(80);

// ============================================================
//  辅助函数：打印当前 WiFi 模式状态
// ============================================================
static void print_wifi_status(const char* label) {
    wifi_mode_t mode = WiFi.getMode();
    const char* mode_str = "UNKNOWN";
    if (mode == WIFI_OFF)         mode_str = "OFF";
    else if (mode == WIFI_STA)    mode_str = "STA";
    else if (mode == WIFI_AP)     mode_str = "AP";
    else if (mode == WIFI_AP_STA) mode_str = "AP_STA";

    Serial.printf("[%s] Mode: %s, AP IP: %s, STA IP: %s, Status: %d\n",
                  label, mode_str,
                  WiFi.softAPIP().toString().c_str(),
                  WiFi.localIP().toString().c_str(),
                  WiFi.status());
}

// ============================================================
//  REST API 处理函数
// ============================================================

// GET /api/data — 返回 3 路实时力传感数据
static void handle_get_data() {
    String json = "{\"sensors\":[";
    for (int i = 0; i < 3; i++) {
        const SensorDataCache& s = data_cache_get_sensor(i);
        json += "{";
        json += "\"raw_val\":"   + String(convert_to_force(s.raw_val)) + ",";
        json += "\"raw_gram\":"  + String(s.raw_val, 2) + ",";     // 克力原始值（保留2位小数）
        json += "\"filtered\":"  + String(s.filtered) + ",";
        json += "\"baseline\":"  + String(s.baseline) + ",";
        json += "\"threshold\":" + String(s.threshold) + ",";
        json += "\"detected\":"  + String(s.detected ? "true" : "false") + ",";
        json += "\"offset\":"    + String(get_channel_threshold(i)) + ",";
        json += "\"algo_type\":" + String(get_algo_type(i)) + ",";
        json += "\"var_thr\":"   + String(get_var_threshold(i)) + ",";
        json += "\"env_win\":"   + String(get_env_window(i)) + ",";
        json += "\"env_dry_up\":"+ String(get_env_dry_up(i)) + ",";
        json += "\"env_dry_down\":"+ String(get_env_dry_down(i)) + ",";
        json += "\"env_up\":"    + String(get_env_upper_offset(i)) + ",";
        json += "\"env_lo\":"    + String(get_env_lower_offset(i));
        json += "}";
        if (i < 2) json += ",";
    }
    json += "],";
    json += "\"wifi_connected\":" + String(wifi_is_connected() ? "true" : "false") + ",";
    json += "\"mqtt_connected\":" + String(mqtt_is_connected() ? "true" : "false");
    json += "}";
    s_server.send(200, "application/json", json);
}

// GET /api/sysconfig — 返回网络与系统配置
static void handle_get_sysconfig() {
    String json = "{";
    json += "\"ssid\":\""   + get_sta_ssid() + "\",";
    json += "\"pass\":\""   + get_sta_password() + "\",";
    json += "\"name\":\""   + get_device_name() + "\",";
    json += "\"broker\":\"" + get_mqtt_broker() + "\",";
    json += "\"port\":"     + String(get_mqtt_port());
    json += "}";
    s_server.send(200, "application/json", json);
}

// POST /api/sysconfig — 保存网络配置到 NVS
static void handle_post_sysconfig() {
    bool changed = false;
    if (s_server.hasArg("ssid"))     changed |= nvs_set_sta_ssid(s_server.arg("ssid"));
    if (s_server.hasArg("password")) changed |= nvs_set_sta_password(s_server.arg("password"));
    if (s_server.hasArg("name"))     changed |= nvs_set_device_name(s_server.arg("name"));
    if (s_server.hasArg("broker"))   changed |= nvs_set_mqtt_broker(s_server.arg("broker"));
    if (s_server.hasArg("port"))     changed |= nvs_set_mqtt_port(s_server.arg("port").toInt());

    if (changed) {
        Serial.println("[WebConfig] System configurations updated in NVS.");
    }
    s_server.send(200, "text/plain", "OK");
}

// GET /api/scan — 扫描附近 WiFi
static void handle_wifi_scan() {
    int n = WiFi.scanNetworks(false, false);
    String json = "{\"networks\":[";
    if (n > 0) {
        int indices[n];
        for (int i = 0; i < n; i++) indices[i] = i;
        for (int i = 0; i < n - 1; i++) {
            for (int j = i + 1; j < n; j++) {
                if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
                    int temp = indices[i];
                    indices[i] = indices[j];
                    indices[j] = temp;
                }
            }
        }
        for (int i = 0; i < n; i++) {
            int idx = indices[i];
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(idx) + "\",";
            json += "\"rssi\":"   + String(WiFi.RSSI(idx));
            json += "}";
            if (i < n - 1) json += ",";
        }
    }
    json += "]}";
    WiFi.scanDelete();
    s_server.send(200, "application/json", json);
}

// POST /api/threshold — 配置单通道阈值偏移量
static void handle_post_threshold() {
    if (s_server.hasArg("ch") && s_server.hasArg("offset")) {
        int ch     = s_server.arg("ch").toInt();
        int offset = s_server.arg("offset").toInt();
        if (ch < 0 || ch >= 3) {
            s_server.send(400, "text/plain", "Invalid ch");
            return;
        }
        if (nvs_set_threshold_offset(ch, offset)) {
            Serial.printf("[WebConfig] Ch%d threshold offset set to %d\n", ch, offset);
            s_server.send(200, "text/plain", "OK");
            return;
        }
    }
    s_server.send(400, "text/plain", "Bad Request");
}

// POST /api/algo — 配置单通道算法类型及其参数
static void handle_post_algo() {
    if (!s_server.hasArg("ch")) {
        s_server.send(400, "text/plain", "Missing ch");
        return;
    }
    int ch = s_server.arg("ch").toInt();
    if (ch < 0 || ch >= 3) {
        s_server.send(400, "text/plain", "Invalid ch");
        return;
    }

    if (s_server.hasArg("type"))         nvs_set_algo_type(ch, s_server.arg("type").toInt());
    if (s_server.hasArg("var_thr"))      nvs_set_var_threshold(ch, s_server.arg("var_thr").toInt());
    if (s_server.hasArg("env_win"))      nvs_set_env_window(ch, s_server.arg("env_win").toInt());
    if (s_server.hasArg("env_dry_up"))   nvs_set_env_dry_up(ch, s_server.arg("env_dry_up").toInt());
    if (s_server.hasArg("env_dry_down")) nvs_set_env_dry_down(ch, s_server.arg("env_dry_down").toInt());
    if (s_server.hasArg("env_up"))       nvs_set_env_upper_offset(ch, s_server.arg("env_up").toInt());
    if (s_server.hasArg("env_lo"))       nvs_set_env_lower_offset(ch, s_server.arg("env_lo").toInt());
    Serial.printf("[WebConfig] Ch%d algo updated: type=%d\n", ch, get_algo_type(ch));
    s_server.send(200, "text/plain", "OK");
}

// POST /api/hx711 — HX711 在线校准（tare / scale / enable / disable）
// 参数：ch=0~2, action=tare|scale|enable|disable, scale=<float>
static void handle_post_hx711() {
    if (!s_server.hasArg("ch") || !s_server.hasArg("action")) {
        s_server.send(400, "text/plain", "Missing ch or action");
        return;
    }
    int ch = s_server.arg("ch").toInt();
    if (ch < 0 || ch >= 3) {
        s_server.send(400, "text/plain", "Invalid ch");
        return;
    }
    String action = s_server.arg("action");
    if (action == "tare") {
        if (!get_hx711_enabled(ch)) {
            s_server.send(409, "text/plain", "Ch disabled, turn_on first");
            return;
        }
        HX711_Tare(ch, 10);
        s_server.send(200, "text/plain", "Tare OK");
    } else if (action == "scale") {
        if (!s_server.hasArg("scale")) {
            s_server.send(400, "text/plain", "Missing scale");
            return;
        }
        float scale = s_server.arg("scale").toFloat();
        if (scale == 0.0f) {
            s_server.send(400, "text/plain", "scale=0 invalid");
            return;
        }
        HX711_SetScale(ch, scale);
        s_server.send(200, "text/plain", "Scale OK");
    } else if (action == "enable") {
        nvs_set_hx711_enabled(ch, true);
        s_server.send(200, "text/plain", "Enable OK (reboot to take effect)");
    } else if (action == "disable") {
        nvs_set_hx711_enabled(ch, false);
        s_server.send(200, "text/plain", "Disable OK (reboot to take effect)");
    } else {
        s_server.send(400, "text/plain", "Unknown action");
    }
}

// GET /api/hx711 — 返回各通道 HX711 校准参数
static void handle_get_hx711() {
    String json = "{\"channels\":[";
    for (int i = 0; i < 3; i++) {
        json += "{";
        json += "\"ch\":" + String(i) + ",";
        json += "\"scale\":" + String(get_hx711_scale(i), 4) + ",";
        json += "\"tare\":" + String((long)get_hx711_tare(i)) + ",";
        json += "\"enabled\":" + String(get_hx711_enabled(i) ? "true" : "false") + ",";
        json += "\"online\":" + String(HX711_IsOnline(i) ? "true" : "false");
        json += "}";
        if (i < 2) json += ",";
    }
    json += "]}";
    s_server.send(200, "application/json", json);
}

// ============================================================
//  公共接口实现
// ============================================================

void web_config_init() {
    // 1. 初始化 NVS 配置层
    nvs_config_init();

    // 2. 启动 WiFi AP 模式
    print_wifi_status("WebConfig BEFORE softAP");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(FACTORY_WIFI_AP_SSID, FACTORY_WIFI_AP_PASSWORD);
    print_wifi_status("WebConfig AFTER softAP");

    // 3. 挂载 Web 路由
    s_server.on("/", HTTP_GET, []() {
        s_server.send_P(200, "text/html", INDEX_HTML);
    });
    s_server.on("/api/data",      HTTP_GET,  handle_get_data);
    s_server.on("/api/sysconfig", HTTP_GET,  handle_get_sysconfig);
    s_server.on("/api/sysconfig", HTTP_POST, handle_post_sysconfig);
    s_server.on("/api/wifi",      HTTP_GET,  handle_get_sysconfig);   // 兼容旧接口
    s_server.on("/api/wifi",      HTTP_POST, handle_post_sysconfig);  // 兼容旧接口
    s_server.on("/api/scan",      HTTP_GET,  handle_wifi_scan);
    s_server.on("/api/threshold", HTTP_POST, handle_post_threshold);
    s_server.on("/api/algo",      HTTP_POST, handle_post_algo);
    s_server.on("/api/hx711",     HTTP_GET,  handle_get_hx711);
    s_server.on("/api/hx711",     HTTP_POST, handle_post_hx711);

    s_server.begin();
    Serial.println("[WebConfig] Embedded Web Server started on port 80");
}

void web_config_loop() {
    // 防回退机制：检测 WiFi 模式是否被外部库强制切回 STA
    if (WiFi.getMode() == WIFI_STA) {
        Serial.println("[WebConfig] WiFi mode reverted to STA. Restoring AP_STA...");
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(FACTORY_WIFI_AP_SSID, FACTORY_WIFI_AP_PASSWORD);
        print_wifi_status("WebConfig RESTORED AP_STA");
    }
    s_server.handleClient();
}

// ============================================================
//  web_config.h 中声明的缓存更新接口（转发至 data_cache）
// ============================================================
void web_config_update_sensor(int idx, float raw_val, uint16_t filtered,
                               uint16_t baseline, uint16_t threshold, bool detected) {
    data_cache_update_sensor(idx, raw_val, filtered, baseline, threshold, detected);
}
