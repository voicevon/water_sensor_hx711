# water_sensor_hx711

基于 ESP32 + 3 路 HX711 的水位检测节点。采集原始值后立即转换为 16 位无符号计数，通过 BLE 广播、MQTT 上报与 Web 页面输出三路水位触发状态。 

## 功能特性

- 3 路独立 HX711 采集（1 Hz），全链路 16 位无符号原始计数，无去皮、无克数换算
- 三套可切换检测算法：DYNAMIC（动态阈值）、DISCRETE（离散方差）、ENVELOPE（包络范围），各通道独立配置参数
- 数据转换移位位数 N（NVS 可配置，0~8，默认 6），1 LSB = 2^N 原始计数；修改 N 重启生效并重置阈值
- 启动自检：空载基线超出 8192~57344 时串口告警，提示 N 与实际零偏不匹配
- BLE 广播（含数据与触发状态）、MQTT 上报（JSON）、嵌入式 Web 配置页（AP 热点）
- 参数全部存于 NVS：WiFi/MQTT、通道开关、算法类型与阈值，支持参数版本管理

## 硬件连接

| 外设 | 引脚 |
|------|------|
| HX711 #1 | DOUT=23, SCK=22 |
| HX711 #2 | DOUT=32, SCK=33 |
| HX711 #3 | DOUT=26, SCK=27 |
| LED A（MQTT 发送指示） | GPIO 2 |
| LED B（网络状态指示） | GPIO 15 |

## 数据转换

`u16 = (raw24 + 2^(N+15)) >> N`，其中 raw24 为 HX711 24 位有符号原始值，N 为唯一独立参数（K = N + 15 运行时导出）。

- N 越小精度越高（N=0 时 1 LSB = 1 原始计数），但覆盖窗口 ±2^(N+15) 越小；N=8 时窗口覆盖芯片全输出范围
- N 的合适值取决于现场机械状态，通过 Web 页面设置，重启生效
- 详见 [doc/数据转换需求.md](doc/数据转换需求.md)

## 输出报文

**MQTT**（topic `water/sensor/status`）：

```json
{"name":"home", "sensor1":32768, "sensor2":32768, "sensor3":32768, "state":5}
```

**BLE** Manufacturer Data（10 字节）：

```
CompanyID(2B, 0xFFFF 小端) + 传感器1~3(各2B, u16 大端) + StateByte(1B) + SeqNum(1B)
```

- `state` / StateByte：低 3 位对应 3 路触发状态
- 报文不携带 N；当前 N 通过 Web 页面或 `/api/hx711` 查询，订阅端需另行约定

## Web 配置

上电后开放热点 `AP_HX711`（密码 12344321），浏览器访问 `192.168.4.1`：

- 实时监控：3 路原始计数、滤波值、基线、阈值与触发状态
- 参数设置：移位位数 N（0~8，全局参数，三通道共享）
- 网络与系统：STA WiFi、设备名、MQTT Broker 地址与端口

主要 REST API：

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/data` | GET | 3 路实时数据 |
| `/api/hx711` | GET/POST | 在线状态与当前 N / 设置 N |
| `/api/threshold` | POST | 阈值偏移量 |
| `/api/algo` | POST | 算法类型与参数 |
| `/api/sysconfig` | GET/POST | 网络与系统配置 |

## 编译

```
C:\Users\feng\.platformio\penv\Scripts\platformio.exe run
```

上传（需连接设备串口）：`platformio.exe run -t upload`
