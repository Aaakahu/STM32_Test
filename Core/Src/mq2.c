#include "mq2.h"

// 使用简单的轮询方式读取 ADC
// 配置 ADC1 通道 5 (PA5)

static uint16_t mq2_clean_air_value = 300;  // 清洁空气基准值

void MQ2_Init(void)
{
    // 启用时钟
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // 配置 PA5 为模拟输入
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = MQ2_AO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(MQ2_AO_PORT, &GPIO_InitStruct);
    
    // 配置 PA6 为数字输入 (DO)
    GPIO_InitStruct.Pin = MQ2_DO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(MQ2_DO_PORT, &GPIO_InitStruct);
    
    // 简单 ADC 配置
    ADC1->CR2 |= ADC_CR2_ADON;  // 开启 ADC
    HAL_Delay(10);              // 等待稳定
    ADC1->CR2 |= ADC_CR2_CAL;   // 校准
    while(ADC1->CR2 & ADC_CR2_CAL);
}

// 读取模拟值
uint16_t MQ2_ReadAnalog(void)
{
    // 配置通道 5
    ADC1->SQR3 = 5;  // 通道 5
    
    // 开始转换
    ADC1->CR2 |= ADC_CR2_ADON;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    
    // 等待转换完成
    while(!(ADC1->SR & ADC_SR_EOC));
    
    // 读取结果
    uint16_t result = ADC1->DR;
    
    return result;
}

// 读取数字输出
uint8_t MQ2_ReadDigital(void)
{
    return (HAL_GPIO_ReadPin(MQ2_DO_PORT, MQ2_DO_PIN) == GPIO_PIN_RESET) ? 1 : 0;
}

// 获取烟雾等级
uint8_t MQ2_GetSmokeLevel(uint16_t adc_value)
{
    if(adc_value < mq2_clean_air_value + 200)
        return MQ2_LEVEL_CLEAN;
    else if(adc_value < mq2_clean_air_value + 500)
        return MQ2_LEVEL_LOW;
    else if(adc_value < mq2_clean_air_value + 900)
        return MQ2_LEVEL_MEDIUM;
    else if(adc_value < mq2_clean_air_value + 1500)
        return MQ2_LEVEL_HIGH;
    else
        return MQ2_LEVEL_DANGER;
}

// 估算 PPM
uint16_t MQ2_GetPPM(uint16_t adc_value)
{
    if(adc_value < mq2_clean_air_value)
        return 0;
    
    uint16_t ppm = (adc_value - mq2_clean_air_value) * 3;
    if(ppm > 10000) ppm = 10000;
    
    return ppm;
}

// 校准
void MQ2_Calibrate(void)
{
    uint32_t sum = 0;
    for(int i = 0; i < 20; i++)
    {
        sum += MQ2_ReadAnalog();
        HAL_Delay(50);
    }
    mq2_clean_air_value = sum / 20;
    if(mq2_clean_air_value < 200) mq2_clean_air_value = 200;  // 最小值限制
}
