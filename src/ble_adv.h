#pragma once

// ============================================================
//  BLE 广播管理层
//  职责：BLE 设备初始化、每轮采样后更新 Manufacturer Data 广播包
// ============================================================

#include "config.h"

/**
 * @brief 初始化 BLE 设备并启动广播
 *        设备名称由 config.h 中的 BLE_DEVICE_NAME 宏决定
 */
void ble_init();

/**
 * @brief 用最新的传感器数据更新 BLE 广播包并重启广播
 *
 * 广播数据格式（Manufacturer Specific Data，标准 LTV 结构）：
 *   Company ID : 0xFFFF（LSB 在前，小端序，共 2 字节）
 *   Ch0~Ch3    : 各通道伪电容值，每通道 uint16_t 大端序，共 8 字节
 *   Seq Num    : 递增序列号，1 字节，用于接收端检测丢包
 *   总 Payload : 11 字节
 *
 * @param sensors 本轮采集的传感器数据数组指针（大小为 SENSOR_COUNT）
 */
void ble_update(const uint16_t *sensors, const bool *states);
