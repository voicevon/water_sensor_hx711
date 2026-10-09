#ifndef _HX711SENSOR_H_
#define _HX711SENSOR_H_

#include <Arduino.h>

/* ============================================================
 *  HX711 传感器驱动接口
 *  通信方式：3 路独立 HX711，每路 DOUT + SCK 两引脚
 *  数据语义：直接输出 24 位有符号原生 ADC 计数（-8388608 ~ +8388607），
 *            无移位截断、无去皮、无克数换算
 * ============================================================ */

/**
 * @brief 初始化 3 路 HX711（不加载任何校准/去皮参数）
 * @return true：至少一路 HX711 就绪；false：无传感器响应
 */
bool HX711_Init_All(void);

/**
 * @brief 读取 3 路 24 位有符号原始计数值（int32_t 承载）
 * @param out_raw  输出数组（长度 >= 3），按 [0]/[1]/[2] 对应 HX711 #1/#2/#3
 * @return true：读取成功；false：所有通道离线
 */
bool HX711_Read_All(int32_t* out_raw);

/**
 * @brief 启动自检：各在线通道读取若干样本求平均基线，
 *        若读数逼近极值饱和线（-8388608 或 8388607）则串口告警
 */
void HX711_SelfCheckBaseline(void);

/**
 * @brief 获取当前各通道在线状态
 */
bool HX711_IsOnline(int ch);

#endif /* _HX711SENSOR_H_ */
