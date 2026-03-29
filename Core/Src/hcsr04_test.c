#include "main.h"
#include "ssd1306.h"
#include "hcsr04.h"
#include <string.h>

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

// LED 引脚定义
#define LED_GREEN_PORT    GPIOA
#define LED_GREEN_PIN     GPIO_PIN_2
#define LED_RED_PORT      GPIOA
#define LED_RED_PIN       GPIO_PIN_3

#define LED_GREEN_ON()    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET)
#define LED_GREEN_OFF()   HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET)
#define LED_RED_ON()      HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET)
#define LED_RED_OFF()     HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET)

// 简单的 us 延时
static void SimpleDelay_us(uint32_t us)
{
    volatile uint32_t n = us * 8;  // 8MHz 下粗略估算
    while(n--) __asm volatile("nop");
}

// 测试 1: 检查 Trig 输出波形
static uint8_t Test_TrigOutput(void)
{
    // 手动发送 10us 脉冲，用 LED 闪烁指示
    for(int i = 0; i < 5; i++)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);   // Trig High
        SimpleDelay_us(10);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET); // Trig Low
        HAL_Delay(100);
    }
    return 0;
}

// 测试 2: 检查 Echo 引脚状态
static uint8_t Test_EchoState(uint32_t *low_time, uint32_t *high_time)
{
    uint32_t timeout;
    
    // 测量低电平持续时间
    timeout = 1000000;
    while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET && timeout--);
    *low_time = 1000000 - timeout;
    
    // 测量高电平持续时间（如果变高了）
    timeout = 1000000;
    while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET && timeout--);
    *high_time = 1000000 - timeout;
    
    // 发送触发信号
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
    SimpleDelay_us(12);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
    
    // 等待 Echo 变高
    timeout = 1000000;
    uint32_t wait_start = timeout;
    while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET && timeout--);
    uint32_t wait_time = wait_start - timeout;
    
    return (wait_time < 1000000) ? 0 : 1;  // 0=有响应, 1=无响应
}

// 测试 3: 绕过库直接读取
static uint32_t Test_DirectRead(void)
{
    uint32_t timeout;
    uint32_t pulse_count = 0;
    
    // 发送触发
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
    SimpleDelay_us(12);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
    
    // 等待 Echo 变高
    timeout = 50000;
    while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET && timeout--);
    if(timeout == 0) return 0xFFFFFFFF;  // 无响应
    
    // 测量高电平时间
    timeout = 100000;
    while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET && timeout--)
    {
        pulse_count++;
    }
    
    return pulse_count;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    
    OLED_Init();
    
    // 手动初始化引脚
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // PA0 = Trig = 输出
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
    
    // PA1 = Echo = 输入
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    LED_GREEN_ON();
    LED_RED_OFF();
    
    OLED_Clear();
    OLED_ShowString(0, 0, "SR04 Test Mode", 8, OLED_COLOR_WHITE);
    OLED_Refresh();
    HAL_Delay(1000);
    
    uint8_t test_phase = 0;
    uint32_t test_count = 0;
    uint32_t last_pulse = 0;
    
    while(1)
    {
        OLED_Clear();
        test_count++;
        
        switch(test_phase)
        {
            case 0:  // 测试 1: 基础 GPIO 检查
            {
                OLED_ShowString(0, 0, "Test1: GPIO", 8, OLED_COLOR_WHITE);
                
                // 读取当前 Echo 状态
                GPIO_PinState echo_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);
                GPIO_PinState trig_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
                
                OLED_ShowString(0, 10, "Trig:", 8, OLED_COLOR_WHITE);
                OLED_ShowNum(40, 10, trig_state, 1, 8, OLED_COLOR_WHITE);
                
                OLED_ShowString(60, 10, "Echo:", 8, OLED_COLOR_WHITE);
                OLED_ShowNum(100, 10, echo_state, 1, 8, OLED_COLOR_WHITE);
                
                // 切换 Trig 看 Echo 是否变化（正常应该不会变）
                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0);
                HAL_Delay(10);
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
                
                OLED_ShowString(0, 20, "Cnt:", 8, OLED_COLOR_WHITE);
                OLED_ShowNum(30, 20, test_count % 100, 2, 8, OLED_COLOR_WHITE);
                
                if(test_count > 5) test_phase = 1;
                break;
            }
            
            case 1:  // 测试 2: 发送触发脉冲
            {
                OLED_ShowString(0, 0, "Test2: Trigger", 8, OLED_COLOR_WHITE);
                
                // 发送触发
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
                LED_RED_ON();  // 红灯亮表示正在触发
                SimpleDelay_us(12);
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
                LED_RED_OFF();
                
                // 立即读取 Echo
                uint32_t timeout = 1000;
                uint32_t response_time = 0;
                while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET && timeout--)
                {
                    response_time++;
                }
                
                OLED_ShowString(0, 10, "Wait:", 8, OLED_COLOR_WHITE);
                OLED_ShowNum(35, 10, response_time, 4, 8, OLED_COLOR_WHITE);
                
                if(timeout == 0)
                {
                    OLED_ShowString(80, 10, "NO", 8, OLED_COLOR_WHITE);
                }
                else
                {
                    OLED_ShowString(80, 10, "OK", 8, OLED_COLOR_WHITE);
                    LED_GREEN_TOGGLE();
                }
                
                OLED_ShowString(0, 20, "Pulse:", 8, OLED_COLOR_WHITE);
                OLED_ShowNum(45, 20, last_pulse, 5, 8, OLED_COLOR_WHITE);
                
                if(test_count > 20) test_phase = 2;
                break;
            }
            
            case 2:  // 测试 3: 完整脉冲测量
            {
                OLED_ShowString(0, 0, "Test3: Measure", 8, OLED_COLOR_WHITE);
                
                uint32_t pulse = Test_DirectRead();
                last_pulse = pulse;
                
                if(pulse == 0xFFFFFFFF)
                {
                    OLED_ShowString(0, 10, "NO RESPONSE", 8, OLED_COLOR_WHITE);
                    LED_RED_ON();
                }
                else if(pulse == 0)
                {
                    OLED_ShowString(0, 10, "ZERO PULSE", 8, OLED_COLOR_WHITE);
                }
                else
                {
                    // 估算距离
                    uint32_t dist = pulse / 58;
                    OLED_ShowString(0, 10, "Pulse:", 8, OLED_COLOR_WHITE);
                    OLED_ShowNum(45, 10, pulse, 5, 8, OLED_COLOR_WHITE);
                    
                    OLED_ShowString(0, 20, "Dist:", 8, OLED_COLOR_WHITE);
                    OLED_ShowNum(35, 20, dist, 3, 8, OLED_COLOR_WHITE);
                    OLED_ShowString(65, 20, "cm", 8, OLED_COLOR_WHITE);
                    
                    LED_GREEN_TOGGLE();
                }
                break;
            }
        }
        
        OLED_Refresh();
        HAL_Delay(200);  // 快速刷新
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    
    if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
    
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    
    if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // LED 引脚
    GPIO_InitStruct.Pin = LED_GREEN_PIN | LED_RED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
}

void Error_Handler(void)
{
    __disable_irq();
    while(1) {}
}
