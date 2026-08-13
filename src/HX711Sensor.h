#ifndef _HX711SENSOR_H_
#define _HX711SENSOR_H_

#include <Arduino.h>

/* ============================================================
 *  HX711 力传感器驱动接口
 *  通信方式：3 路独立 HX711，每路 DOUT + SCK 两引脚
 * ============================================================ */

/**
 * @brief 初始化 3 路 HX711，加载 NVS 中存储的 scale/tare 校准参数
 * @return true：至少一路 HX711 就绪；false：无传感器响应
 */
bool HX711_Init_All(void);

/**
 * @brief 读取 3 路力值（克力，浮点）
 * @param out_grams  输出浮点数组（长度 >= 3），按 [0]/[1]/[2] 对应 HX711 #1/#2/#3
 * @return true：读取成功；false：所有通道离线
 */
bool HX711_Read_All(float* out_grams);

/**
 * @brief 对指定通道执行去皮（tare）操作，并将结果写入 NVS 持久化
 * @param ch  通道索引（0-2）
 * @param times  平均次数（默认 10）
 */
void HX711_Tare(int ch, int times = 10);

/**
 * @brief 设置指定通道的校准系数（scale）并写入 NVS
 * @param ch     通道索引（0-2）
 * @param scale  校准系数（原始值 / 已知重量）
 */
void HX711_SetScale(int ch, float scale);

/**
 * @brief 获取当前各通道在线状态
 */
bool HX711_IsOnline(int ch);

#endif /* _HX711SENSOR_H_ */
