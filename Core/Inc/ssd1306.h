#ifndef __SSD1306_H
#define __SSD1306_H

#include "main.h"
#include <stdint.h>

/*=============================================================================
 * 0.91寸 OLED I2C 接口定义 (中景园电子)
 * 引脚连接（4线）：
 *   VCC  -> 3.3V / 5V
 *   GND  -> 电源地
 *   SCL  -> PB6 (I2C时钟)
 *   SDA  -> PB7 (I2C数据)
 * 
 * 注意：如果 OLED 模块有 RES 引脚，可以：
 *   1. 接 3.3V（禁用硬件复位）
 *   2. 或接到 PB0（软件复位）
 *============================================================================*/

// I2C 引脚定义 (软件 I2C)
#define OLED_SCL_PORT   GPIOB
#define OLED_SCL_PIN    GPIO_PIN_6

#define OLED_SDA_PORT   GPIOB
#define OLED_SDA_PIN    GPIO_PIN_7

// 可选：RESET 引脚（如果 OLED 模块有 RES 引脚）
// #define OLED_USE_RES    // 取消注释以启用软件复位
#ifdef OLED_USE_RES
#define OLED_RES_PORT   GPIOA
#define OLED_RES_PIN    GPIO_PIN_6
#endif

// 宏函数：控制引脚
#define OLED_SCL_HIGH()   HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_SET)
#define OLED_SCL_LOW()    HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_RESET)
#define OLED_SDA_HIGH()   HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_SET)
#define OLED_SDA_LOW()    HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_RESET)
#define OLED_SDA_READ()   HAL_GPIO_ReadPin(OLED_SDA_PORT, OLED_SDA_PIN)

#ifdef OLED_USE_RES
#define OLED_RES_HIGH()   HAL_GPIO_WritePin(OLED_RES_PORT, OLED_RES_PIN, GPIO_PIN_SET)
#define OLED_RES_LOW()    HAL_GPIO_WritePin(OLED_RES_PORT, OLED_RES_PIN, GPIO_PIN_RESET)
#endif

// I2C 设备地址 (SSD1306)
// 7-bit 地址：0x3C
// 8-bit 写地址：0x78 (0x3C << 1)
// 8-bit 读地址：0x79 (0x3C << 1 | 1)
#define OLED_I2C_ADDR     0x78

// OLED 分辨率 (0.91寸 = 128x32)
#define OLED_WIDTH        128
#define OLED_HEIGHT       32
#define OLED_PAGE_NUM     4       // 32/8 = 4页
#define OLED_PAGE_SIZE    128     // 每页128字节

// 颜色定义
#define OLED_COLOR_BLACK  0
#define OLED_COLOR_WHITE  1
#define OLED_COLOR_INVERT 2

// SSD1306 命令定义
#define OLED_CMD     0x00
#define OLED_DATA    0x40
#define OLED_CMD_DISPLAY_OFF     0xAE
#define OLED_CMD_DISPLAY_ON      0xAF
#define OLED_CMD_SET_DISPLAY_CLOCK  0xD5
#define OLED_CMD_SET_MULTIPLEX    0xA8
#define OLED_CMD_SET_DISPLAY_OFFSET 0xD3
#define OLED_CMD_SET_START_LINE   0x40
#define OLED_CMD_CHARGE_PUMP      0x8D
#define OLED_CMD_SET_MEMORY_ADDR_MODE 0x20
#define OLED_CMD_SET_SEGMENT_REMAP 0xA1
#define OLED_CMD_SET_COM_SCAN_DIR  0xC8
#define OLED_CMD_SET_COM_PINS     0xDA
#define OLED_CMD_SET_CONTRAST     0x81
#define OLED_CMD_SET_PRECHARGE    0xD9
#define OLED_CMD_SET_VCOMH        0xDB
#define OLED_CMD_DISPLAY_RAM      0xA4
#define OLED_CMD_DISPLAY_NORMAL   0xA6
#define OLED_CMD_DISPLAY_INVERSE  0xA7

// 函数声明
void OLED_Init(void);
void OLED_Clear(void);
void OLED_Refresh(void);
void OLED_Display_On(void);
void OLED_Display_Off(void);
void OLED_SetContrast(uint8_t contrast);

// 底层函数 (供OLED_UI使用)
void OLED_Write_Byte(uint8_t data, uint8_t cmd);
void OLED_Set_Pos(uint8_t x, uint8_t y);
void OLED_RefreshGram(void);
void OLED_WriteCommand(uint8_t cmd);
void OLED_WriteData(uint8_t data);

// I2C 底层函数（供OLED_UI使用）
void I2C_Start(void);
void I2C_Stop(void);
uint8_t I2C_SendByte(uint8_t byte);

void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);
uint8_t OLED_GetPoint(uint8_t x, uint8_t y);
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color);
void OLED_FillRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color);
void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t r, uint8_t color);

void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size, uint8_t color);
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t size, uint8_t color);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color);
void OLED_ShowSignedNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size, uint8_t color);
void OLED_ShowHexNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color);
void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t int_len, uint8_t frac_len, uint8_t size, uint8_t color);

#endif /* __SSD1306_H */
