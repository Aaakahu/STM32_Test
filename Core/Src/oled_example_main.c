/**
 * @file    main.c
 * @brief   SSD1306 OLED 驱动使用示例
 * @author  Embedded Driver Expert
 * @date    2026-03-22
 * 
 * @note    本示例演示 OLED 驱动的完整使用流程：
 *          1. 初始化 I2C 外设
 *          2. 初始化 OLED
 *          3. 在显存缓冲区绘制图形和文字
 *          4. 刷新到屏幕显示
 */

#include "main.h"
#include "oled.h"
#include <stdio.h>

/*==============================================================================
 *                              全局变量
 *=============================================================================*/

/* I2C 句柄 - 必须在 main.c 中定义，oled.c 中会引用 */
I2C_HandleTypeDef hi2c1;

/* UART 句柄 - 用于调试输出 */
UART_HandleTypeDef huart1;

/*==============================================================================
 *                              函数原型
 *=============================================================================*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);

/*==============================================================================
 *                              主函数
 *=============================================================================*/

/**
 * @brief 主函数
 * @note  OLED 使用流程：
 *        1. 初始化 HAL 库和系统时钟
 *        2. 初始化 I2C 外设
 *        3. 初始化 OLED
 *        4. 清屏
 *        5. 在 GRAM 上绘制内容
 *        6. 刷新到屏幕
 */
int main(void)
{
    uint8_t counter = 0;
    float temperature = 25.5f;
    
    /* 1. 初始化 HAL 库 */
    HAL_Init();
    
    /* 2. 配置系统时钟 */
    SystemClock_Config();
    
    /* 3. 初始化外设 */
    MX_GPIO_Init();
    MX_I2C1_Init();         /* I2C 初始化必须在 OLED 之前 */
    MX_USART1_UART_Init();
    
    /* 4. 初始化 OLED */
    OLED_Init();
    
    /* 5. 清屏 */
    OLED_Clear();
    OLED_Refresh();
    
    /* 延迟 1 秒，显示清空后的屏幕 */
    HAL_Delay(1000);
    
    /*=========================================================================
     *  示例 1: 显示基本信息
     *========================================================================*/
    OLED_ShowString(0, 0, "STM32 OLED Demo", 16, OLED_COLOR_WHITE);
    OLED_ShowString(0, 16, "SSD1306 Driver", 8, OLED_COLOR_WHITE);
    OLED_ShowString(0, 24, "128x64 Frame Buffer", 8, OLED_COLOR_WHITE);
    
    /* 刷新到屏幕 */
    OLED_Refresh();
    HAL_Delay(2000);
    
    /*=========================================================================
     *  示例 2: 显示数字和变量
     *========================================================================*/
    OLED_Clear();
    
    /* 显示标题 */
    OLED_ShowString(0, 0, "Counter Demo:", 16, OLED_COLOR_WHITE);
    
    /* 显示计数器值 */
    OLED_ShowString(0, 16, "Count: ", 16, OLED_COLOR_WHITE);
    OLED_ShowNum(80, 16, counter, 3, 16, OLED_COLOR_WHITE);
    
    /* 显示温度 */
    OLED_ShowString(0, 32, "Temp: ", 16, OLED_COLOR_WHITE);
    OLED_ShowFloat(56, 32, temperature, 2, 1, 16, OLED_COLOR_WHITE);
    OLED_ShowString(110, 32, "C", 16, OLED_COLOR_WHITE);
    
    /* 显示十六进制 */
    OLED_ShowString(0, 48, "Hex: 0x", 8, OLED_COLOR_WHITE);
    OLED_ShowHexNum(48, 48, 0xABCD, 4, 8, OLED_COLOR_WHITE);
    
    OLED_Refresh();
    HAL_Delay(2000);
    
    /*=========================================================================
     *  示例 3: 绘制基本图形
     *========================================================================*/
    OLED_Clear();
    
    /* 绘制矩形边框 */
    OLED_DrawRectangle(0, 0, 128, 64, OLED_COLOR_WHITE);
    
    /* 绘制标题 */
    OLED_ShowString(20, 4, "Graphics Test", 8, OLED_COLOR_WHITE);
    
    /* 绘制直线 */
    OLED_DrawLine(10, 20, 60, 20, OLED_COLOR_WHITE);  /* 水平线 */
    OLED_DrawLine(10, 20, 35, 50, OLED_COLOR_WHITE);  /* 斜线 */
    OLED_DrawLine(60, 20, 35, 50, OLED_COLOR_WHITE);  /* 斜线 */
    
    /* 绘制圆 */
    OLED_DrawCircle(90, 35, 15, OLED_COLOR_WHITE);
    OLED_DrawCircle(90, 35, 8, OLED_COLOR_WHITE);
    OLED_DrawPoint(90, 35, OLED_COLOR_WHITE);  /* 圆心 */
    
    /* 填充矩形 */
    OLED_FillRectangle(10, 54, 108, 8, OLED_COLOR_WHITE);
    OLED_ShowString(25, 56, "Filled Rectangle", 8, OLED_COLOR_BLACK);
    
    OLED_Refresh();
    HAL_Delay(2000);
    
    /*=========================================================================
     *  示例 4: 动态计数器显示
     *========================================================================*/
    OLED_Clear();
    OLED_ShowString(0, 0, "Dynamic Counter:", 16, OLED_COLOR_WHITE);
    
    while (1) {
        /* 清除数字区域 (使用黑色填充) */
        OLED_FillRectangle(40, 24, 48, 16, OLED_COLOR_BLACK);
        
        /* 显示递增的计数器 */
        OLED_ShowNum(40, 24, counter, 3, 16, OLED_COLOR_WHITE);
        
        /* 显示二进制 */
        OLED_ShowString(0, 48, "Bin: ", 8, OLED_COLOR_WHITE);
        OLED_FillRectangle(32, 48, 96, 8, OLED_COLOR_BLACK);
        OLED_ShowBinNum(32, 48, counter, 8, 8, OLED_COLOR_WHITE);
        
        /* 刷新到屏幕 */
        OLED_Refresh();
        
        /* 计数器递增 */
        counter++;
        if (counter > 255) {
            counter = 0;
        }
        
        /* LED 闪烁指示 */
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);  /* PC13 通常是板载 LED */
        
        /* 500ms 延迟 */
        HAL_Delay(500);
    }
}

/*==============================================================================
 *                              I2C 初始化
 *=============================================================================*/

/**
 * @brief I2C1 初始化函数
 * @note  配置为 Fast Mode (400kHz)
 *        SCL -> PB6, SDA -> PB7 (根据实际硬件调整)
 */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;           /* 400kHz Fast Mode */
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;   /* 占空比 */
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        /* 初始化错误处理 */
        Error_Handler();
    }
}

/*==============================================================================
 *                              UART 初始化 (调试)
 *=============================================================================*/

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

/*==============================================================================
 *                              GPIO 初始化
 *=============================================================================*/

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* 使能 GPIO 时钟 */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    /* 配置 PC13 (板载 LED) */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/*==============================================================================
 *                              系统时钟配置
 *=============================================================================*/

void SystemClock_Config(void)
{
    /* 根据具体芯片配置系统时钟 */
    /* 以下为 STM32F103 示例配置 */
    
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    /* 配置 HSE 和 PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;  /* 8MHz * 9 = 72MHz */
    
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }
    
    /* 配置总线时钟 */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/*==============================================================================
 *                              错误处理
 *=============================================================================*/

void Error_Handler(void)
{
    /* 错误处理：关闭中断并进入死循环 */
    __disable_irq();
    while (1) {
        /* 错误指示：快速闪烁 LED */
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        for (volatile uint32_t i = 0; i < 50000; i++);
    }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* 断言失败处理 */
    printf("Assertion failed: %s, line %d\r\n", file, line);
}
#endif /* USE_FULL_ASSERT */
