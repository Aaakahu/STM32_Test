/**
  ******************************************************************************
  * @file           : hcsr04.c
  * @brief          : HC-SR04 超声波测距模块驱动实现
  * @note           : 使用 TIM4 进行精确微秒计时 (8MHz 系统时钟)
  ******************************************************************************
  */

#include "hcsr04.h"

/*=============================================================================
 * 私有宏定义
 *============================================================================*/
#define HCSR04_TIM          TIM4
#define HCSR04_TIM_CLK_EN   __HAL_RCC_TIM4_CLK_ENABLE()

/*=============================================================================
 * 私有函数
 *============================================================================*/

/**
  * @brief  微秒级延时
  * @param  us: 延时时间 (微秒)
  * @note   在 8MHz 下，1us = 8 个时钟周期
  */
static void HCSR04_DelayUs(uint32_t us)
{
    // TIM4 已配置为 1MHz (1us 计数一次)
    HCSR04_TIM->CNT = 0;
    while (HCSR04_TIM->CNT < us);
}

/**
  * @brief  获取 Echo 高电平持续时间 (微秒)
  * @retval 高电平时间 (us)，超时返回 0xFFFF
  */
static uint16_t HCSR04_GetEchoTime(void)
{
    uint16_t timeout = 0;
    
    // 等待 Echo 变高
    while (HCSR04_ECHO_READ() == GPIO_PIN_RESET) {
        if (++timeout >= 1000) return 0;  // 超时检测
    }
    
    // 开始计时
    HCSR04_TIM->CNT = 0;
    
    // 等待 Echo 变低
    while (HCSR04_ECHO_READ() == GPIO_PIN_SET) {
        if (HCSR04_TIM->CNT >= HCSR04_TIMEOUT_US) {
            return HCSR04_TIMEOUT_US;  // 超时
        }
    }
    
    return HCSR04_TIM->CNT;
}

/*=============================================================================
 * 公有函数
 *============================================================================*/

/**
  * @brief  HC-SR04 初始化
  * @note   配置 GPIO 和 TIM4 (1MHz 计数频率)
  */
void HCSR04_Init(void)
{
    // 使能时钟
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HCSR04_TIM_CLK_EN;
    
    // 配置 Trig 为推挽输出
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = HCSR04_TRIG_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(HCSR04_TRIG_PORT, &GPIO_InitStruct);
    
    // 配置 Echo 为浮空输入
    GPIO_InitStruct.Pin = HCSR04_ECHO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(HCSR04_ECHO_PORT, &GPIO_InitStruct);
    
    // 初始化 Trig 为低电平
    HCSR04_TRIG_LOW();
    
    // 配置 TIM4: 8MHz / 8 = 1MHz (1us 计数)
    HCSR04_TIM->PSC = 7;        // 预分频 8-1
    HCSR04_TIM->ARR = 0xFFFF;   // 最大计数值
    HCSR04_TIM->CR1 = TIM_CR1_CEN;  // 使能计数器
    
    // 上电稳定延时
    HAL_Delay(50);
}

/**
  * @brief  获取距离 (cm)
  * @retval 距离值 (cm)，无效返回 999
  * @note   距离 = 时间(us) / 58
  *         声速 340m/s = 0.034cm/us
  *         往返距离 = 时间 * 0.034 / 2 = 时间 / 58.8
  */
uint16_t HCSR04_GetDistance(void)
{
    uint16_t echo_time;
    uint16_t distance;
    
    // 发送触发信号: 至少 10us 高电平
    HCSR04_TRIG_HIGH();
    HCSR04_DelayUs(15);  // 15us 确保可靠触发
    HCSR04_TRIG_LOW();
    
    // 测量回波时间
    echo_time = HCSR04_GetEchoTime();
    
    // 计算距离: time(us) / 58 = distance(cm)
    // 使用整数除法: time * 100 / 588 更精确
    if (echo_time < 150 || echo_time >= HCSR04_TIMEOUT_US) {
        // 小于 2cm 或超时，视为无效
        return 999;
    }
    
    distance = echo_time / 58;
    
    // 限制范围
    if (distance > HCSR04_MAX_DISTANCE_CM) {
        return 999;  // 超出范围
    }
    if (distance < HCSR04_MIN_DISTANCE_CM) {
        return 999;  // 太近无效
    }
    
    return distance;
}

/**
  * @brief  检测是否太靠近 (小于 3cm)
  * @retval 1=距离小于3cm, 0=正常
  */
uint8_t HCSR04_IsTooClose(void)
{
    uint16_t distance = HCSR04_GetDistance();
    return (distance <= HCSR04_ALARM_DISTANCE_CM && distance != 999);
}
