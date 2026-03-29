#ifndef __ENCODER_H
#define __ENCODER_H

#include "main.h"

// EC11编码器引脚定义
#define ENC_KEY_GPIO_PORT    GPIOA
#define ENC_KEY_GPIO_PIN     GPIO_PIN_0   // 按键引脚（SW）

// 旋转引脚定义
#define ENC_A_GPIO_PORT      GPIOA
#define ENC_A_GPIO_PIN       GPIO_PIN_7   // A相/CLK
#define ENC_B_GPIO_PORT      GPIOA
#define ENC_B_GPIO_PIN       GPIO_PIN_8   // B相/DT

// 按键状态
#define KEY_RELEASED    1
#define KEY_PRESSED     0

void Encoder_Init(void);
uint8_t Encoder_KeyIsPressed(void);
uint8_t Encoder_KeyIsPressedEdge(void);

// 旋转功能
int8_t Encoder_GetRotation(void);
int8_t Encoder_GetRotationAccumulated(void);
void Encoder_ClearRotation(void);
uint8_t Encoder_RotationAsKey(void);  // 旋转替代按键

#endif
