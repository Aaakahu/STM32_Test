#ifndef __MQ2_H
#define __MQ2_H

#include "main.h"

// MQ-2 传感器引脚
#define MQ2_AO_PORT     GPIOA
#define MQ2_AO_PIN      GPIO_PIN_5      // 模拟输出
#define MQ2_DO_PORT     GPIOA
#define MQ2_DO_PIN      GPIO_PIN_6      // 数字输出

// 烟雾等级定义
#define MQ2_LEVEL_CLEAN     0   // 清洁
#define MQ2_LEVEL_LOW       1   // 低浓度
#define MQ2_LEVEL_MEDIUM    2   // 中等浓度
#define MQ2_LEVEL_HIGH      3   // 高浓度
#define MQ2_LEVEL_DANGER    4   // 危险浓度

#define MQ2_THRESHOLD_PPM   800 // 报警阈值 PPM

void MQ2_Init(void);
uint16_t MQ2_ReadAnalog(void);
uint8_t MQ2_ReadDigital(void);
uint8_t MQ2_GetSmokeLevel(uint16_t adc_value);
uint16_t MQ2_GetPPM(uint16_t adc_value);
void MQ2_Calibrate(void);

#endif
