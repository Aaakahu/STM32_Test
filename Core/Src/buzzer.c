#include "buzzer.h"
#include "stm32f1xx_hal.h"

// 简单的延时函数（微秒级）
static void Buzzer_Delay_us(uint32_t us)
{
    volatile uint32_t n = us * 8;
    while(n--) __asm volatile("nop");
}

void Buzzer_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = BUZZER_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_PORT, &GPIO_InitStruct);
    
    // 初始关闭（PNP三极管，高电平截止）
    BUZZER_OFF();
}

// 简单的 beep，持续 ms 毫秒
void Buzzer_Beep(uint32_t ms)
{
    BUZZER_ON();
    HAL_Delay(ms);
    BUZZER_OFF();
}

// 报警声：响几次
void Buzzer_Alarm(uint8_t times, uint32_t on_ms, uint32_t off_ms)
{
    for(uint8_t i = 0; i < times; i++)
    {
        BUZZER_ON();
        HAL_Delay(on_ms);
        BUZZER_OFF();
        if(i < times - 1)  // 最后一次不响间隔
        {
            HAL_Delay(off_ms);
        }
    }
}

// 开机音效：do-mi-sol-do（上升音阶）
void Buzzer_StartupSound(void)
{
    // 有源蜂鸣器频率固定，只能通过断续产生不同节奏
    // 短促表示高音，长音表示低音
    
    // "哆" - 短促
    BUZZER_ON();
    HAL_Delay(100);
    BUZZER_OFF();
    HAL_Delay(50);
    
    // "来" - 稍长
    BUZZER_ON();
    HAL_Delay(150);
    BUZZER_OFF();
    HAL_Delay(50);
    
    // "咪" - 更长
    BUZZER_ON();
    HAL_Delay(200);
    BUZZER_OFF();
    HAL_Delay(50);
    
    // "发" - 长音结束
    BUZZER_ON();
    HAL_Delay(300);
    BUZZER_OFF();
}

// 持续报警声（急促）
void Buzzer_AlarmSound(void)
{
    // 急促的 "滴滴滴" 声
    for(uint8_t i = 0; i < 3; i++)
    {
        BUZZER_ON();
        HAL_Delay(100);
        BUZZER_OFF();
        HAL_Delay(100);
    }
}
