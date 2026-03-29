#ifndef __BUZZER_H
#define __BUZZER_H

#include "main.h"

// 蜂鸣器引脚定义 - 使用 PB0
#define BUZZER_PORT     GPIOB
#define BUZZER_PIN      GPIO_PIN_0

// 有源蜂鸣器控制（PNP三极管，低电平导通）
#define BUZZER_ON()     HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET)
#define BUZZER_OFF()    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET)
#define BUZZER_TOGGLE() HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN)

// 初始化
void Buzzer_Init(void);

// 基本控制
void Buzzer_Beep(uint32_t ms);
void Buzzer_Alarm(uint8_t times, uint32_t on_ms, uint32_t off_ms);

// 开机音效
void Buzzer_StartupSound(void);

// 报警音效
void Buzzer_AlarmSound(void);

#endif
