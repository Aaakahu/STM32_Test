#ifndef __SERVO_H
#define __SERVO_H

#include "main.h"

// 舵机连接引脚 - PA1 (TIM2_CH2)
#define SERVO_GPIO_PORT     GPIOA
#define SERVO_GPIO_PIN      GPIO_PIN_1

// 定时器配置 - 系统时钟8MHz
// 目标: 50Hz PWM (20ms周期)
// PSC = 79 -> 8MHz/80 = 100kHz
// ARR = 1999 -> 100kHz/2000 = 50Hz
#define SERVO_TIM           TIM2
#define SERVO_TIM_PSC       79
#define SERVO_TIM_ARR       1999

// 180度舵机角度控制 (单位: 10us @ 100kHz)
// 20ms周期 = 2000 counts
// 0.5ms = 50 counts = 0度
// 1.5ms = 150 counts = 90度
// 2.5ms = 250 counts = 180度
#define SERVO_MIN_PULSE     50      // 0.5ms - 0度
#define SERVO_MAX_PULSE     250     // 2.5ms - 180度
#define SERVO_MID_PULSE     150     // 1.5ms - 90度

// 门状态
#define DOOR_STATE_UNKNOWN  0
#define DOOR_STATE_OPEN     1       // 门开 (0度)
#define DOOR_STATE_CLOSED   2       // 门关 (180度)
#define DOOR_STATE_OPENING  3       // 正在开门
#define DOOR_STATE_CLOSING  4       // 正在关门

void Servo_Init(void);
void Servo_Stop(void);              // 停止PWM输出
void Servo_SetAngle(uint8_t angle); // 设置角度 0-180
void Servo_Open(void);              // 开门 (0度)
void Servo_Close(void);             // 关门 (180度)

#endif
