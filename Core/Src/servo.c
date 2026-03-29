#include "servo.h"

// 使用寄存器直接配置 TIM2
// 系统时钟: 8MHz HSI
// 目标频率: 50Hz (20ms周期)

void Servo_Init(void)
{
    // 使能GPIOA和TIM2时钟
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    
    // 配置PA1为复用推挽输出
    // 清除MODE1和CNF1位 (PA1对应位2-3)
    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    // 设置MODE1 = 11 (50MHz输出), CNF1 = 10 (复用推挽)
    GPIOA->CRL |= GPIO_CRL_MODE1_1 | GPIO_CRL_MODE1_0 | GPIO_CRL_CNF1_1;
    
    // 配置TIM2
    TIM2->PSC = SERVO_TIM_PSC;      // 预分频 79 -> 100kHz
    TIM2->ARR = SERVO_TIM_ARR;      // 自动重载 1999 -> 20ms
    TIM2->CR1 = TIM_CR1_ARPE;       // 使能自动重载预装载
    
    // 配置通道2为PWM模式1
    // 清除OC2M和CC2S
    TIM2->CCMR1 &= ~(TIM_CCMR1_OC2M | TIM_CCMR1_CC2S);
    // PWM模式1: 110, 预装载使能
    TIM2->CCMR1 |= TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE;
    
    // 初始位置: 中间位置
    TIM2->CCR2 = SERVO_MID_PULSE;
    
    // 使能通道2输出，高电平有效
    TIM2->CCER |= TIM_CCER_CC2E;
    
    // 使能定时器
    TIM2->CR1 |= TIM_CR1_CEN;
}

// 停止PWM输出
void Servo_Stop(void)
{
    // 禁用通道输出
    TIM2->CCER &= ~TIM_CCER_CC2E;
}

// 启动PWM输出
void Servo_Start(void)
{
    // 使能通道输出
    TIM2->CCER |= TIM_CCER_CC2E;
}

// 设置舵机角度 (0-180度)
void Servo_SetAngle(uint8_t angle)
{
    // 确保角度在有效范围内
    if(angle > 180) angle = 180;
    
    // 重新使能PWM输出
    Servo_Start();
    
    // 角度映射到pulse值
    // 0度 -> 50, 180度 -> 250
    uint16_t pulse = SERVO_MIN_PULSE + (angle * (SERVO_MAX_PULSE - SERVO_MIN_PULSE) / 180);
    
    TIM2->CCR2 = pulse;
}

// 开门 - 0度
void Servo_Open(void)
{
    Servo_SetAngle(0);
}

// 关门 - 180度
void Servo_Close(void)
{
    Servo_SetAngle(180);
}
