/*
 * 智能车库 - STM32 串口通信模块
 * 功能：通过串口与 ESP8266 通信，接收控制命令，上报传感器数据
 * 添加到现有 main.c 中
 */

#ifndef __ESP8266_BRIDGE_H
#define __ESP8266_BRIDGE_H

#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ==================== 配置 ====================

// 数据上报间隔 (ms) - 改为 1 秒以提高实时性
#define ESP_REPORT_INTERVAL 1000

// 接收缓冲区大小
#define ESP_RX_BUFFER_SIZE 128

// ==================== 数据结构 ====================

// 上报数据结构
typedef struct {
    float temperature;      // DHT11 温度
    float humidity;         // DHT11 湿度
    uint8_t smokeLevel;     // MQ2 等级 0-4
    uint16_t smokePpm;      // MQ2 PPM
    uint16_t distance;      // HC-SR04 距离 (cm)
    uint8_t doorStatus;     // 0=CLOSED, 1=OPEN
    uint8_t alarmFlag;      // 报警标志
    char alarmType[16];     // 报警类型
} ESP_SensorData_t;

// ==================== 全局变量（extern，定义在你的 main.c）====================

extern float DHT11_Temp, DHT11_Humi;           // DHT11 数据
extern uint8_t smoke_level;                     // MQ2 等级
extern uint16_t smoke_ppm;                      // MQ2 PPM
// extern uint16_t distance_cm;  // 注释掉，已在 main.c 定义
extern uint8_t door_open_flag;                  // 门状态标志

// 本模块全局变量
extern volatile uint8_t esp_command_received;
extern volatile char esp_command_buffer[ESP_RX_BUFFER_SIZE];

// ==================== 函数声明 ====================

void ESP_Init(void);
void ESP_ProcessCommand(void);
void ESP_ReportData(void);
void ESP_SendAlarm(const char* alarmType);
void ESP_HandleRxInterrupt(void);

// 串口发送辅助函数
void ESP_SendString(const char* str);
void ESP_SendDataJSON(void);
void ESP_SendDataSimple(void);

#endif /* __ESP8266_BRIDGE_H */