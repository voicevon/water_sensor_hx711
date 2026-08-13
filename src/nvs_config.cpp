#include "nvs_config.h"
#include "config.h"
#include <Preferences.h>

// ============================================================
//  NVS 存储实例（内部私有）
// ============================================================
static Preferences s_prefs;

// NVS 命名空间与键名常量
static const char NVS_NAMESPACE[]  = "hx711_cfg";
static const char NVS_KEY_SSID[]   = "sta_ssid";
static const char NVS_KEY_PASS[]   = "sta_pass";
static const char NVS_KEY_NAME[]   = "sta_name";
static const char NVS_KEY_BROKER[] = "mqtt_broker";
static const char NVS_KEY_PORT[]   = "mqtt_port";

// ============================================================
//  配置项内存缓存（内部私有）
// ============================================================
static String s_sta_ssid     = "";
static String s_sta_password = "";
static String s_device_name  = "";
static String s_mqtt_broker  = "";
static int    s_mqtt_port    = 1883;

// ---- HX711 校准参数缓存（3路） ----
// scale: ADC 原始值 / 克力，默认 1.0（未校准）
// tare : 去皮时的 ADC 偏置，默认 0
// enabled: 通道开关，默认 true（开启）
static float s_hx711_scale[3]   = { 1.0f, 1.0f, 1.0f };
static long  s_hx711_tare[3]    = { 0L,   0L,   0L   };
static bool  s_hx711_enabled[3] = { true, true, true };

// ---- 阈值偏移量（3路，默认 50） ----
static int s_threshold_offset[3] = { 50, 50, 50 };

// ---- 算法类型缓存（0=DYNAMIC, 1=DISCRETE, 2=ENVELOPE） ----
static int s_algo_type[3] = { 0, 0, 0 };

// ---- 离散方差阈值缓存（默认 5000） ----
static int s_var_threshold[3] = { 5000, 5000, 5000 };

// ---- 包络算法参数缓存 ----
static int s_env_window[3]       = { 30,   30,   30   };
static int s_env_dry_up[3]       = { 1000, 1000, 1000 };
static int s_env_dry_down[3]     = { 1000, 1000, 1000 };
static int s_env_upper_offset[3] = { 500,  500,  500  };
static int s_env_lower_offset[3] = { 300,  300,  300  };

// ============================================================
//  NVS 初始化
// ============================================================
void nvs_config_init() {
    s_prefs.begin(NVS_NAMESPACE, false);

    // 加载 3 路 HX711 校准参数
    for (int i = 0; i < 3; i++) {
        // scale 以字符串形式存储（Preferences 不直接支持 float）
        String scale_key   = "sc" + String(i);
        String tare_key    = "tr" + String(i);
        String enabled_key = "en" + String(i);
        // 若 NVS 中没有存储则使用默认值
        if (s_prefs.isKey(scale_key.c_str())) {
            s_hx711_scale[i] = s_prefs.getFloat(scale_key.c_str(), 1.0f);
        }
        s_hx711_tare[i]    = (long)s_prefs.getLong(tare_key.c_str(), 0L);
        s_hx711_enabled[i] = s_prefs.getBool(enabled_key.c_str(), true);
    }

    // 加载 3 路阈值与算法参数
    for (int i = 0; i < 3; i++) {
        s_threshold_offset[i] = s_prefs.getInt(("thr" + String(i)).c_str(), 50);
        s_algo_type[i]        = s_prefs.getInt(("al"  + String(i)).c_str(), 0);
        s_var_threshold[i]    = s_prefs.getInt(("vt"  + String(i)).c_str(), 5000);
        s_env_window[i]       = s_prefs.getInt(("ew"  + String(i)).c_str(), 30);
        s_env_dry_up[i]       = s_prefs.getInt(("edu" + String(i)).c_str(), 1000);
        s_env_dry_down[i]     = s_prefs.getInt(("edd" + String(i)).c_str(), 1000);
        s_env_upper_offset[i] = s_prefs.getInt(("eu"  + String(i)).c_str(), 500);
        s_env_lower_offset[i] = s_prefs.getInt(("el"  + String(i)).c_str(), 300);
    }

    // 加载网络配置
    s_sta_ssid    = s_prefs.getString(NVS_KEY_SSID,   FACTORY_WIFI_SSID);
    s_sta_password= s_prefs.getString(NVS_KEY_PASS,   FACTORY_WIFI_PASSWORD);
    s_device_name = s_prefs.getString(NVS_KEY_NAME,   FACTORY_DEVICE_NAME);
    s_mqtt_broker = s_prefs.getString(NVS_KEY_BROKER, FACTORY_MQTT_BROKER);
    s_mqtt_port   = s_prefs.getInt(NVS_KEY_PORT,      FACTORY_MQTT_PORT);

    Serial.printf("[NvsConfig] HX711 scale: %.4f / %.4f / %.4f\n",
                  s_hx711_scale[0], s_hx711_scale[1], s_hx711_scale[2]);
    Serial.printf("[NvsConfig] HX711 tare: %ld / %ld / %ld\n",
                  s_hx711_tare[0], s_hx711_tare[1], s_hx711_tare[2]);
    Serial.printf("[NvsConfig] HX711 enabled: %d / %d / %d\n",
                  s_hx711_enabled[0], s_hx711_enabled[1], s_hx711_enabled[2]);
    Serial.printf("[NvsConfig] WiFi STA: %s, Device: %s, MQTT: %s:%d\n",
                  s_sta_ssid.c_str(), s_device_name.c_str(),
                  s_mqtt_broker.c_str(), s_mqtt_port);
}

// ============================================================
//  Getter 实现
// ============================================================
String get_sta_ssid()                 { return s_sta_ssid; }
String get_sta_password()             { return s_sta_password; }
String get_device_name()              { return s_device_name; }
String get_mqtt_broker()              { return s_mqtt_broker; }
int    get_mqtt_port()                { return s_mqtt_port; }

float  get_hx711_scale(int ch)        { return (ch >= 0 && ch < 3) ? s_hx711_scale[ch] : 1.0f; }
long   get_hx711_tare(int ch)         { return (ch >= 0 && ch < 3) ? s_hx711_tare[ch] : 0L; }
bool   get_hx711_enabled(int ch)      { return (ch >= 0 && ch < 3) ? s_hx711_enabled[ch] : true; }

int    get_channel_threshold(int ch)  { return (ch >= 0 && ch < 3) ? s_threshold_offset[ch] : 50; }
int    get_algo_type(int ch)          { return (ch >= 0 && ch < 3) ? s_algo_type[ch] : 0; }
int    get_var_threshold(int ch)      { return (ch >= 0 && ch < 3) ? s_var_threshold[ch] : 5000; }
int    get_env_window(int ch)         { return (ch >= 0 && ch < 3) ? s_env_window[ch] : 30; }
int    get_env_dry_up(int ch)         { return (ch >= 0 && ch < 3) ? s_env_dry_up[ch] : 1000; }
int    get_env_dry_down(int ch)       { return (ch >= 0 && ch < 3) ? s_env_dry_down[ch] : 1000; }
int    get_env_upper_offset(int ch)   { return (ch >= 0 && ch < 3) ? s_env_upper_offset[ch] : 500; }
int    get_env_lower_offset(int ch)   { return (ch >= 0 && ch < 3) ? s_env_lower_offset[ch] : 300; }

// ============================================================
//  Setter 实现（含变化检测 + NVS 写入）
// ============================================================
bool nvs_set_sta_ssid(const String& val) {
    if (val.length() == 0 || val == s_sta_ssid) return false;
    s_sta_ssid = val;
    s_prefs.putString(NVS_KEY_SSID, val);
    return true;
}

bool nvs_set_sta_password(const String& val) {
    if (val == s_sta_password) return false;
    s_sta_password = val;
    s_prefs.putString(NVS_KEY_PASS, val);
    return true;
}

bool nvs_set_device_name(const String& val) {
    if (val.length() == 0 || val == s_device_name) return false;
    s_device_name = val;
    s_prefs.putString(NVS_KEY_NAME, val);
    return true;
}

bool nvs_set_mqtt_broker(const String& val) {
    if (val.length() == 0 || val == s_mqtt_broker) return false;
    s_mqtt_broker = val;
    s_prefs.putString(NVS_KEY_BROKER, val);
    return true;
}

bool nvs_set_mqtt_port(int val) {
    if (val <= 0 || val == s_mqtt_port) return false;
    s_mqtt_port = val;
    s_prefs.putInt(NVS_KEY_PORT, val);
    return true;
}

bool nvs_set_hx711_scale(int ch, float scale) {
    if (ch < 0 || ch >= 3) return false;
    if (scale == 0.0f) return false; // 防止除零
    if (s_hx711_scale[ch] == scale) return false;
    s_hx711_scale[ch] = scale;
    s_prefs.putFloat(("sc" + String(ch)).c_str(), scale);
    return true;
}

bool nvs_set_hx711_tare(int ch, long offset) {
    if (ch < 0 || ch >= 3) return false;
    if (s_hx711_tare[ch] == offset) return false;
    s_hx711_tare[ch] = offset;
    s_prefs.putLong(("tr" + String(ch)).c_str(), offset);
    return true;
}

bool nvs_set_hx711_enabled(int ch, bool enabled) {
    if (ch < 0 || ch >= 3) return false;
    if (s_hx711_enabled[ch] == enabled) return false;
    s_hx711_enabled[ch] = enabled;
    s_prefs.putBool(("en" + String(ch)).c_str(), enabled);
    Serial.printf("[NvsConfig] HX711 Ch%d %s\n", ch + 1, enabled ? "ENABLED" : "DISABLED");
    return true;
}

bool nvs_set_threshold_offset(int ch, int offset) {
    if (ch < 0 || ch >= 3) return false;
    if (offset < -500 || offset > 500) return false;
    if (s_threshold_offset[ch] == offset) return false;
    s_threshold_offset[ch] = offset;
    s_prefs.putInt(("thr" + String(ch)).c_str(), offset);
    return true;
}

bool nvs_set_algo_type(int ch, int type) {
    if (ch < 0 || ch >= 3) return false;
    if (type < 0 || type > 2) return false;
    if (s_algo_type[ch] == type) return false;
    s_algo_type[ch] = type;
    s_prefs.putInt(("al" + String(ch)).c_str(), type);
    return true;
}

bool nvs_set_var_threshold(int ch, int threshold) {
    if (ch < 0 || ch >= 3) return false;
    if (threshold < 0 || threshold > 100000) return false;
    if (s_var_threshold[ch] == threshold) return false;
    s_var_threshold[ch] = threshold;
    s_prefs.putInt(("vt" + String(ch)).c_str(), threshold);
    return true;
}

bool nvs_set_env_window(int ch, int window) {
    if (ch < 0 || ch >= 3) return false;
    if (window < 1 || window > 120) return false;
    if (s_env_window[ch] == window) return false;
    s_env_window[ch] = window;
    s_prefs.putInt(("ew" + String(ch)).c_str(), window);
    return true;
}

bool nvs_set_env_dry_up(int ch, int window) {
    if (ch < 0 || ch >= 3) return false;
    if (window < 1 || window > 10000) return false;
    if (s_env_dry_up[ch] == window) return false;
    s_env_dry_up[ch] = window;
    s_prefs.putInt(("edu" + String(ch)).c_str(), window);
    return true;
}

bool nvs_set_env_dry_down(int ch, int window) {
    if (ch < 0 || ch >= 3) return false;
    if (window < 1 || window > 10000) return false;
    if (s_env_dry_down[ch] == window) return false;
    s_env_dry_down[ch] = window;
    s_prefs.putInt(("edd" + String(ch)).c_str(), window);
    return true;
}

bool nvs_set_env_upper_offset(int ch, int offset) {
    if (ch < 0 || ch >= 3) return false;
    if (offset < 0 || offset > 5000) return false;
    if (s_env_upper_offset[ch] == offset) return false;
    s_env_upper_offset[ch] = offset;
    s_prefs.putInt(("eu" + String(ch)).c_str(), offset);
    return true;
}

bool nvs_set_env_lower_offset(int ch, int offset) {
    if (ch < 0 || ch >= 3) return false;
    if (offset < 0 || offset > 5000) return false;
    if (s_env_lower_offset[ch] == offset) return false;
    s_env_lower_offset[ch] = offset;
    s_prefs.putInt(("el" + String(ch)).c_str(), offset);
    return true;
}
