#include "nvs_config.h"
#include "config.h"
#include <Preferences.h>

// ============================================================
//  NVS 存储实例（内部私有）
// ============================================================
static Preferences s_prefs;

// NVS 命名空间与键名常量
static const char NVS_NAMESPACE[]    = "hx711_cfg";
static const char NVS_KEY_SSID[]     = "sta_ssid";
static const char NVS_KEY_PASS[]     = "sta_pass";
static const char NVS_KEY_NAME[]     = "sta_name";
static const char NVS_KEY_BROKER[]   = "mqtt_broker";
static const char NVS_KEY_PORT[]     = "mqtt_port";
static const char NVS_KEY_SHIFT_N[]  = "shift_n";
static const char NVS_KEY_PARAM_VER[]= "pver";

// 参数版本号：参数存储结构变更（如 N 引入/语义变化）时递增，
// 参数版本号：升级为 3（24 位原生有符号模式，默认阈值与结构更新）
static const uint8_t PARAM_VERSION = 3;

// ============================================================
//  配置项内存缓存（内部私有）
// ============================================================
static String s_sta_ssid     = "";
static String s_sta_password = "";
static String s_device_name  = "";
static String s_mqtt_broker  = "";
static int    s_mqtt_port    = 1883;

// ---- 移位位数已废弃（保持 0 直通） ----
static int s_shift_n = 0;

// ---- 阈值偏移量（3路，24位原生模式默认 3200） ----
static int s_threshold_offset[3] = { 3200, 3200, 3200 };

// ---- 算法类型缓存（0=DYNAMIC, 1=DISCRETE, 2=ENVELOPE） ----
static int s_algo_type[3] = { 0, 0, 0 };

// ---- 离散方差阈值缓存（24位模式默认 500000） ----
static int s_var_threshold[3] = { 500000, 500000, 500000 };

// ---- 包络算法参数缓存 ----
static int s_env_window[3]       = { 30,    30,    30    };
static int s_env_dry_up[3]       = { 1000,  1000,  1000  };
static int s_env_dry_down[3]     = { 1000,  1000,  1000  };
static int s_env_upper_offset[3] = { 32000, 32000, 32000 };
static int s_env_lower_offset[3] = { 19200, 19200, 19200 };

// ============================================================
//  内部辅助：阈值类参数恢复默认值（内存 + NVS）
// ============================================================
static void reset_threshold_params() {
    for (int i = 0; i < 3; i++) {
        s_threshold_offset[i] = 3200;   s_prefs.putInt(("thr" + String(i)).c_str(), 3200);
        s_var_threshold[i]    = 500000; s_prefs.putInt(("vt"  + String(i)).c_str(), 500000);
        s_env_window[i]       = 30;     s_prefs.putInt(("ew"  + String(i)).c_str(), 30);
        s_env_dry_up[i]       = 1000;   s_prefs.putInt(("edu" + String(i)).c_str(), 1000);
        s_env_dry_down[i]     = 1000;   s_prefs.putInt(("edd" + String(i)).c_str(), 1000);
        s_env_upper_offset[i] = 32000;  s_prefs.putInt(("eu"  + String(i)).c_str(), 32000);
        s_env_lower_offset[i] = 19200;  s_prefs.putInt(("el"  + String(i)).c_str(), 19200);
    }
    Serial.println("[NvsConfig] Threshold-class params reset to 24-bit defaults.");
}

// ============================================================
//  NVS 初始化
// ============================================================
void nvs_config_init() {
    s_prefs.begin(NVS_NAMESPACE, false);

    // 参数版本检查：检测到旧版本参数（或首次使用）时重置为默认值，
    // 并清理已废弃的旧版键（sc*/tr*/en*）
    uint8_t stored_ver = s_prefs.getUChar(NVS_KEY_PARAM_VER, 0);
    if (stored_ver != PARAM_VERSION) {
        Serial.printf("[NvsConfig] Param version %u != %u, resetting params to defaults.\n",
                      stored_ver, PARAM_VERSION);
        reset_threshold_params();
        for (int i = 0; i < 3; i++) {
            s_prefs.remove(("sc" + String(i)).c_str());
            s_prefs.remove(("tr" + String(i)).c_str());
            s_prefs.remove(("en" + String(i)).c_str());
        }
        s_prefs.putUChar(NVS_KEY_PARAM_VER, PARAM_VERSION);
    }

    // 移位位数 N 已废弃，保持 0 直通
    s_shift_n = 0;

    // 加载 3 路阈值与算法参数
    for (int i = 0; i < 3; i++) {
        s_threshold_offset[i] = s_prefs.getInt(("thr" + String(i)).c_str(), 3200);
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

    Serial.printf("[NvsConfig] Shift N: %d (1 LSB = %d raw counts)\n",
                  s_shift_n, 1 << s_shift_n);
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

int    get_shift_n()                  { return s_shift_n; }

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

bool nvs_set_shift_n(int n) {
    (void)n;
    return false; // 24位直通模式下移位参数已废弃
}

bool nvs_set_threshold_offset(int ch, int offset) {
    if (ch < 0 || ch >= 3) return false;
    if (offset < -500000 || offset > 500000) return false;
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
    if (threshold < 0 || threshold > 100000000) return false;
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
    if (offset < 0 || offset > 500000) return false;
    if (s_env_upper_offset[ch] == offset) return false;
    s_env_upper_offset[ch] = offset;
    s_prefs.putInt(("eu" + String(ch)).c_str(), offset);
    return true;
}

bool nvs_set_env_lower_offset(int ch, int offset) {
    if (ch < 0 || ch >= 3) return false;
    if (offset < 0 || offset > 500000) return false;
    if (s_env_lower_offset[ch] == offset) return false;
    s_env_lower_offset[ch] = offset;
    s_prefs.putInt(("el" + String(ch)).c_str(), offset);
    return true;
}
