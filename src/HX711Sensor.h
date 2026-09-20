#ifndef _HX711SENSOR_H_
#define _HX711SENSOR_H_

#include <Arduino.h>

/* ============================================================
 *  HX711 传感器驱动接口
 *  通信方式：3 路独立 HX711，每路 DOUT + SCK 两引脚
 *  数据语义：24 位原始值经两步转换（+2^K 右移 N）后输出
 *            16 位无符号计数，无去皮、无克数换算
 * ============================================================ */

/**
 * @brief 初始化 3 路 HX711（不加载任何校准/去皮参数）
 * @return true：至少一路 HX711 就绪；false：无传感器响应
 */
bool HX711_Init_All(void);

/**
 * @brief 读取 3 路 16 位无符号原始计数值
 *        直接取 HX711 24 位原始值，按当前 N（NVS 配置）立即转换
 * @param out_u16  输出数组（长度 >= 3），按 [0]/[1]/[2] 对应 HX711 #1/#2/#3
 * @return true：读取成功；false：所有通道离线
 */
bool HX711_Read_All(uint16_t* out_u16);

/**
 * @brief 启动自检：各在线通道读取若干样本求平均基线，
 *        基线超出 8192~57344 时串口告警（当前 N 与实际零偏不匹配）
 */
void HX711_SelfCheckBaseline(void);

/**
 * @brief 获取当前各通道在线状态
 */
bool HX711_IsOnline(int ch);

#endif /* _HX711SENSOR_H_ */
