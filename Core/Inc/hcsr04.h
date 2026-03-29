/**
  ******************************************************************************
  * @file           : hcsr04.h
  * @brief          : HC-SR04 超声波测距模块驱动头文件
  * @note           : 
  *   Trig 引脚: 触发测距 (输出)
  *   Echo 引脚: 接收回波 (输入) 
  *   距离计算: distance(cm) = pulse_width(us) / 58
  ******************************************************************************
  */

#ifndef __HCSR04_H
#define __HCSR04_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* 引脚定义 - 根据实际硬件连接修改 */
#define HCSR04_TRIG_PORT    GPIOB
#define HCSR04_TRIG_PIN     GPIO_PIN_8      // PB8 - 触发

#define HCSR04_ECHO_PORT    GPIOB
#define HCSR04_ECHO_PIN     GPIO_PIN_9      // PB9 - 回波

/* 宏函数 */
#define HCSR04_TRIG_HIGH()  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_SET)
#define HCSR04_TRIG_LOW()   HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET)
#define HCSR04_ECHO_READ()  HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN)

/* 常量定义 */
#define HCSR04_TIMEOUT_US       38000       // 最大超时时间 (38ms = 约6米)
#define HCSR04_MAX_DISTANCE_CM  400         // 最大测量距离 400cm
#define HCSR04_MIN_DISTANCE_CM  2           // 最小测量距离 2cm

#define HCSR04_ALARM_DISTANCE_CM    3       // 报警距离阈值 3cm

/* 函数声明 */
void HCSR04_Init(void);
uint16_t HCSR04_GetDistance(void);
uint8_t HCSR04_IsTooClose(void);

#ifdef __cplusplus
}
#endif

#endif /* __HCSR04_H */
