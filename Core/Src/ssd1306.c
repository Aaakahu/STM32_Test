/**
 * @file    ssd1306.c
 * @brief   0.91寸 OLED I2C 驱动 (中景园电子)
 * @note    基于 STM32 HAL 库的软件 I2C 实现
 *          分辨率: 128x32
 *          控制器: SSD1306
 *          接口: I2C (SCL-PB6, SDA-PB7)
 */

#include "ssd1306.h"
#include "oled_font.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 * 本地显存缓冲区 (Frame Buffer)
 * 128x32 分辨率 = 4页 x 128字节 = 512字节
 *============================================================================*/
uint8_t OLED_GRAM[OLED_PAGE_NUM][OLED_PAGE_SIZE];

/*=============================================================================
 * 软件 I2C 底层操作
 *============================================================================*/

/**
 * @brief I2C 延时函数
 */
static void I2C_Delay(void)
{
    // 简单延时，约 5us @ 8MHz
    for(volatile int i = 0; i < 10; i++);
}

/**
 * @brief I2C 起始信号
 */
void I2C_Start(void)
{
    OLED_SDA_HIGH();
    OLED_SCL_HIGH();
    I2C_Delay();
    OLED_SDA_LOW();
    I2C_Delay();
    OLED_SCL_LOW();
}

/**
 * @brief I2C 停止信号
 */
void I2C_Stop(void)
{
    OLED_SCL_LOW();
    OLED_SDA_LOW();
    I2C_Delay();
    OLED_SCL_HIGH();
    I2C_Delay();
    OLED_SDA_HIGH();
    I2C_Delay();
}

/**
 * @brief I2C 等待应答
 * @return 1=收到ACK, 0=无应答
 */
static uint8_t I2C_WaitAck(void)
{
    uint8_t ack;
    
    OLED_SDA_HIGH();  // 释放SDA
    I2C_Delay();
    OLED_SCL_HIGH();
    I2C_Delay();
    
    // 读取ACK
    ack = (OLED_SDA_READ() == GPIO_PIN_RESET) ? 1 : 0;
    
    OLED_SCL_LOW();
    I2C_Delay();
    
    return ack;
}

/**
 * @brief I2C 发送一个字节
 * @param byte 要发送的数据
 * @return 1=收到ACK, 0=无应答
 */
uint8_t I2C_SendByte(uint8_t byte)
{
    for(uint8_t i = 0; i < 8; i++)
    {
        // 发送最高位 (MSB first)
        if(byte & 0x80)
            OLED_SDA_HIGH();
        else
            OLED_SDA_LOW();
        
        byte <<= 1;
        I2C_Delay();
        OLED_SCL_HIGH();
        I2C_Delay();
        OLED_SCL_LOW();
    }
    
    // 等待应答
    return I2C_WaitAck();
}

/**
 * @brief 向 OLED 写入命令
 * @param cmd 命令字节
 * @note I2C 格式: [设备地址] [0x00] [命令]
 */
static void OLED_WriteCmd(uint8_t cmd)
{
    I2C_Start();
    I2C_SendByte(OLED_I2C_ADDR);  // 设备地址 + 写
    I2C_SendByte(0x00);            // 控制字节: Co=0, D/C#=0 (命令)
    I2C_SendByte(cmd);
    I2C_Stop();
}

/**
 * @brief 向 OLED 写入数据 (内部静态版本)
 * @param data 数据字节
 * @note I2C 格式: [设备地址] [0x40] [数据]
 */
static void OLED_WriteDataStatic(uint8_t data)
{
    I2C_Start();
    I2C_SendByte(OLED_I2C_ADDR);  // 设备地址 + 写
    I2C_SendByte(0x40);            // 控制字节: Co=0, D/C#=1 (数据)
    I2C_SendByte(data);
    I2C_Stop();
}

/**
 * @brief 批量写入数据
 * @param data 数据缓冲区
 * @param size 数据长度
 * @note 使用连续传输提高效率
 */
static void OLED_WriteDataMulti(uint8_t *data, uint16_t size)
{
    if(data == NULL || size == 0)
        return;
    
    I2C_Start();
    I2C_SendByte(OLED_I2C_ADDR);  // 设备地址 + 写
    I2C_SendByte(0x40);            // 控制字节: 数据模式
    
    // 连续发送数据
    for(uint16_t i = 0; i < size; i++)
    {
        I2C_SendByte(data[i]);
    }
    
    I2C_Stop();
}

/*=============================================================================
 * 显存缓冲区操作
 *============================================================================*/

/**
 * @brief 设置 OLED 内部光标位置
 * @param page 页地址 (0~3)
 * @param column 列地址 (0~127)
 */
static void OLED_SetCursor(uint8_t page, uint8_t column)
{
    OLED_WriteCmd(0xB0 + page);
    OLED_WriteCmd(0x00 + (column & 0x0F));
    OLED_WriteCmd(0x10 + ((column >> 4) & 0x0F));
}

/**
 * @brief 将本地显存刷新到 OLED 屏幕
 * @note 逐页刷新，每页 128 字节
 */
void OLED_Refresh(void)
{
    for(uint8_t page = 0; page < OLED_PAGE_NUM; page++)
    {
        OLED_SetCursor(page, 0);
        OLED_WriteDataMulti(OLED_GRAM[page], OLED_PAGE_SIZE);
    }
}

/**
 * @brief 清除本地显存缓冲区
 */
void OLED_Clear(void)
{
    memset(OLED_GRAM, 0x00, sizeof(OLED_GRAM));
}

/**
 * @brief 填充整个屏幕
 * @param color 0=黑色, 1=白色
 */
void OLED_Fill(uint8_t color)
{
    uint8_t fill_byte = (color == OLED_COLOR_WHITE) ? 0xFF : 0x00;
    memset(OLED_GRAM, fill_byte, sizeof(OLED_GRAM));
}

/*=============================================================================
 * OLED_UI 兼容接口
 *============================================================================*/

/**
 * @brief 设置 OLED 光标位置 (OLED_UI兼容)
 * @param x 列地址 (0~127)
 * @param y 页地址 (0~3)
 */
void OLED_Set_Pos(uint8_t x, uint8_t y)
{
    OLED_WriteCmd(0xB0 + y);              // 设置页地址
    OLED_WriteCmd(0x00 + (x & 0x0F));     // 设置列低地址
    OLED_WriteCmd(0x10 + ((x >> 4) & 0x0F)); // 设置列高地址
}

/**
 * @brief 向 OLED 写入字节 (OLED_UI兼容)
 * @param data 数据/命令字节
 * @param cmd 0x00=命令, 0x40=数据
 */
void OLED_Write_Byte(uint8_t data, uint8_t cmd)
{
    I2C_Start();
    I2C_SendByte(OLED_I2C_ADDR);  // 设备地址 + 写
    I2C_SendByte(cmd);             // 控制字节
    I2C_SendByte(data);
    I2C_Stop();
}

/**
 * @brief 写入命令 (OLED_UI兼容)
 */
void OLED_WriteCommand(uint8_t cmd)
{
    OLED_Write_Byte(cmd, 0x00);
}

/**
 * @brief 写入数据 (OLED_UI兼容)
 */
void OLED_WriteData(uint8_t data)
{
    OLED_Write_Byte(data, 0x40);
}

/**
 * @brief 刷新显存到OLED (OLED_UI兼容)
 */
void OLED_RefreshGram(void)
{
    OLED_Refresh();
}

/*=============================================================================
 * 基本图形绘制
 *============================================================================*/

/**
 * @brief 在指定坐标画点
 * @param x X 坐标 (0~127)
 * @param y Y 坐标 (0~31)
 * @param color 0=灭, 1=亮, 2=反色
 */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color)
{
    if(x >= OLED_WIDTH || y >= OLED_HEIGHT)
        return;
    
    uint8_t page = y >> 3;      // y / 8
    uint8_t bit_pos = y & 0x07; // y % 8
    
    switch(color)
    {
        case OLED_COLOR_WHITE:
            OLED_GRAM[page][x] |= (1 << bit_pos);
            break;
        case OLED_COLOR_BLACK:
            OLED_GRAM[page][x] &= ~(1 << bit_pos);
            break;
        case OLED_COLOR_INVERT:
            OLED_GRAM[page][x] ^= (1 << bit_pos);
            break;
        default:
            break;
    }
}

/**
 * @brief 获取指定坐标的像素状态
 */
uint8_t OLED_GetPoint(uint8_t x, uint8_t y)
{
    if(x >= OLED_WIDTH || y >= OLED_HEIGHT)
        return 0;
    
    uint8_t page = y >> 3;
    uint8_t bit_pos = y & 0x07;
    
    return (OLED_GRAM[page][x] >> bit_pos) & 0x01;
}

/**
 * @brief 绘制直线 (Bresenham 算法)
 */
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)
{
    int16_t dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int16_t dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t x = x1, y = y1;
    
    while(1)
    {
        OLED_DrawPoint((uint8_t)x, (uint8_t)y, color);
        
        if(x == x2 && y == y2)
            break;
        
        int16_t e2 = 2 * err;
        
        if(e2 > -dy)
        {
            err -= dy;
            x += sx;
        }
        
        if(e2 < dx)
        {
            err += dx;
            y += sy;
        }
    }
}

/**
 * @brief 绘制矩形 (空心)
 */
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color)
{
    if(width == 0 || height == 0)
        return;
    
    OLED_DrawLine(x, y, x + width - 1, y, color);
    OLED_DrawLine(x, y + height - 1, x + width - 1, y + height - 1, color);
    OLED_DrawLine(x, y, x, y + height - 1, color);
    OLED_DrawLine(x + width - 1, y, x + width - 1, y + height - 1, color);
}

/**
 * @brief 绘制填充矩形 (实心)
 */
void OLED_FillRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color)
{
    if(width == 0 || height == 0)
        return;
    
    for(uint8_t i = 0; i < width; i++)
    {
        for(uint8_t j = 0; j < height; j++)
        {
            OLED_DrawPoint(x + i, y + j, color);
        }
    }
}

/**
 * @brief 绘制圆 (空心，中点圆算法)
 */
void OLED_DrawCircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color)
{
    if(r == 0)
    {
        OLED_DrawPoint(x0, y0, color);
        return;
    }
    
    int16_t x = r, y = 0;
    int16_t err = 0;
    
    while(x >= y)
    {
        OLED_DrawPoint(x0 + x, y0 + y, color);
        OLED_DrawPoint(x0 + y, y0 + x, color);
        OLED_DrawPoint(x0 - y, y0 + x, color);
        OLED_DrawPoint(x0 - x, y0 + y, color);
        OLED_DrawPoint(x0 - x, y0 - y, color);
        OLED_DrawPoint(x0 - y, y0 - x, color);
        OLED_DrawPoint(x0 + y, y0 - x, color);
        OLED_DrawPoint(x0 + x, y0 - y, color);
        
        if(err <= 0)
        {
            y++;
            err += 2 * y + 1;
        }
        
        if(err > 0)
        {
            x--;
            err -= 2 * x + 1;
        }
    }
}

/*=============================================================================
 * 文本显示
 *============================================================================*/

/**
 * @brief 显示单个 ASCII 字符
 * @param x X 坐标
 * @param y Y 坐标 (页对齐：0, 8, 16, 24)
 * @param chr 字符 (ASCII 32-126)
 * @param size 字体大小 (8=6x8, 16=8x16)
 * @param color 颜色
 */
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size, uint8_t color)
{
    if(x >= OLED_WIDTH || y >= OLED_HEIGHT)
        return;
    
    if(chr < 32 || chr > 126)
        chr = '?';
    
    uint8_t char_index = chr - 32;
    uint8_t page = y >> 3;
    
    if(size == 8)
    {
        // 6x8 字体
        for(uint8_t i = 0; i < 6; i++)
        {
            uint8_t temp = F6x8[char_index][i];
            if(color == OLED_COLOR_BLACK)
                temp = ~temp;
            
            if((x + i) < OLED_WIDTH && page < OLED_PAGE_NUM)
                OLED_GRAM[page][x + i] = temp;
        }
    }
    else if(size == 16)
    {
        // 8x16 字体
        for(uint8_t i = 0; i < 8; i++)
        {
            // 上半部分
            uint8_t temp = F8X16[char_index * 16 + i];
            if(color == OLED_COLOR_BLACK)
                temp = ~temp;
            if((x + i) < OLED_WIDTH && page < OLED_PAGE_NUM)
                OLED_GRAM[page][x + i] = temp;
            
            // 下半部分
            temp = F8X16[char_index * 16 + i + 8];
            if(color == OLED_COLOR_BLACK)
                temp = ~temp;
            if((x + i) < OLED_WIDTH && (page + 1) < OLED_PAGE_NUM)
                OLED_GRAM[page + 1][x + i] = temp;
        }
    }
}

/**
 * @brief 显示字符串
 */
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t size, uint8_t color)
{
    if(str == NULL)
        return;
    
    uint8_t char_width = (size == 8) ? 6 : 8;
    
    while(*str != '\0')
    {
        if(x + char_width > OLED_WIDTH)
        {
            x = 0;
            y += (size == 8) ? 8 : 16;
            if(y >= OLED_HEIGHT)
                break;
        }
        
        OLED_ShowChar(x, y, *str, size, color);
        x += char_width;
        str++;
    }
}

/**
 * @brief 显示无符号整数 (前导零填充)
 */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color)
{
    char str[12] = {0};
    
    for(uint8_t i = 0; i < len; i++)
    {
        str[len - 1 - i] = '0' + (num % 10);
        num /= 10;
    }
    str[len] = '\0';
    
    OLED_ShowString(x, y, str, size, color);
}

/**
 * @brief 显示有符号整数
 */
void OLED_ShowSignedNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size, uint8_t color)
{
    uint8_t char_width = (size == 8) ? 6 : 8;
    
    if(num < 0)
    {
        OLED_ShowChar(x, y, '-', size, color);
        num = -num;
    }
    else
    {
        OLED_ShowChar(x, y, '+', size, color);
    }
    
    OLED_ShowNum(x + char_width, y, (uint32_t)num, len, size, color);
}

/**
 * @brief 显示十六进制数 (大写)
 */
void OLED_ShowHexNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color)
{
    char str[12] = {0};
    
    for(uint8_t i = 0; i < len; i++)
    {
        uint8_t nibble = num & 0x0F;
        if(nibble < 10)
            str[len - 1 - i] = '0' + nibble;
        else
            str[len - 1 - i] = 'A' + (nibble - 10);
        num >>= 4;
    }
    str[len] = '\0';
    
    OLED_ShowString(x, y, str, size, color);
}

/**
 * @brief 显示浮点数
 */
void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t int_len, uint8_t frac_len, uint8_t size, uint8_t color)
{
    uint8_t char_width = (size == 8) ? 6 : 8;
    
    if(num < 0)
    {
        OLED_ShowChar(x, y, '-', size, color);
        num = -num;
        x += char_width;
    }
    
    uint32_t int_part = (uint32_t)num;
    float frac = num - (float)int_part;
    
    for(uint8_t i = 0; i < frac_len; i++)
        frac *= 10;
    uint32_t frac_part = (uint32_t)(frac + 0.5f);
    
    OLED_ShowNum(x, y, int_part, int_len, size, color);
    x += int_len * char_width;
    
    OLED_ShowChar(x, y, '.', size, color);
    x += char_width;
    
    OLED_ShowNum(x, y, frac_part, frac_len, size, color);
}

/*=============================================================================
 * 电源管理与控制
 *============================================================================*/

/**
 * @brief 开启 OLED 显示
 */
void OLED_Display_On(void)
{
    OLED_WriteCmd(OLED_CMD_CHARGE_PUMP);
    OLED_WriteCmd(0x14);  // 启用电荷泵
    OLED_WriteCmd(OLED_CMD_DISPLAY_ON);
}

/**
 * @brief 关闭 OLED 显示 (休眠模式)
 */
void OLED_Display_Off(void)
{
    OLED_WriteCmd(OLED_CMD_DISPLAY_OFF);
    OLED_WriteCmd(OLED_CMD_CHARGE_PUMP);
    OLED_WriteCmd(0x10);  // 关闭电荷泵
}

/**
 * @brief 设置屏幕对比度
 * @param contrast 0~255
 */
void OLED_SetContrast(uint8_t contrast)
{
    OLED_WriteCmd(OLED_CMD_SET_CONTRAST);
    OLED_WriteCmd(contrast);
}

/*=============================================================================
 * GPIO 初始化和 OLED 初始化
 *============================================================================*/

/**
 * @brief I2C GPIO 初始化
 */
static void OLED_GPIO_Init(void)
{
    // 使能 GPIO 时钟
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // SCL 和 SDA 配置为开漏输出（I2C 标准模式）
    GPIO_InitStruct.Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;  // 开漏输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;           // 上拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // 初始状态：都置高
    OLED_SCL_HIGH();
    OLED_SDA_HIGH();
    
#ifdef OLED_USE_RES
    // 如果使用了 RES 引脚
    GPIO_InitStruct.Pin = OLED_RES_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(OLED_RES_PORT, &GPIO_InitStruct);
    OLED_RES_HIGH();
#endif
}

/**
 * @brief OLED 初始化
 * @note 针对 128x32 分辨率 (0.91寸 OLED I2C 版本)
 */
void OLED_Init(void)
{
    // 初始化 GPIO
    OLED_GPIO_Init();
    
    // 上电延时
    HAL_Delay(100);
    
#ifdef OLED_USE_RES
    // 硬件复位
    OLED_RES_HIGH();
    HAL_Delay(1);
    OLED_RES_LOW();
    HAL_Delay(10);
    OLED_RES_HIGH();
    HAL_Delay(10);
#endif
    
    // 关闭显示
    OLED_WriteCmd(OLED_CMD_DISPLAY_OFF);
    
    // 设置时钟分频
    OLED_WriteCmd(OLED_CMD_SET_DISPLAY_CLOCK);
    OLED_WriteCmd(0x80);
    
    // 设置多路复用率 (MUX Ratio) - 128x32 屏幕使用 0x1F
    OLED_WriteCmd(OLED_CMD_SET_MULTIPLEX);
    OLED_WriteCmd(0x1F);  // 32 复用
    
    // 设置显示偏移
    OLED_WriteCmd(OLED_CMD_SET_DISPLAY_OFFSET);
    OLED_WriteCmd(0x00);
    
    // 设置起始行
    OLED_WriteCmd(OLED_CMD_SET_START_LINE | 0x00);
    
    // 设置电荷泵 (必须启用)
    OLED_WriteCmd(OLED_CMD_CHARGE_PUMP);
    OLED_WriteCmd(0x14);
    
    // 设置内存寻址模式
    OLED_WriteCmd(OLED_CMD_SET_MEMORY_ADDR_MODE);
    OLED_WriteCmd(0x02);  // 页寻址模式
    
    // 设置段重映射 (A0 = 正常, A1 = 反向)
    // 对于中景园 128x32 OLED，使用正常映射
    OLED_WriteCmd(0xA1);  // 反向段重映射 (修复左右镜像)
    
    // 设置 COM 扫描方向 (C0 = 从上到下, C8 = 从下到上)
    OLED_WriteCmd(0xC8);  // 从下到上扫描 (修复倒转)
    
    // 设置 COM 引脚硬件配置 - 128x32 屏幕使用 0x02
    OLED_WriteCmd(OLED_CMD_SET_COM_PINS);
    OLED_WriteCmd(0x02);
    
    // 设置对比度
    OLED_WriteCmd(OLED_CMD_SET_CONTRAST);
    OLED_WriteCmd(0xCF);
    
    // 设置预充电周期
    OLED_WriteCmd(OLED_CMD_SET_PRECHARGE);
    OLED_WriteCmd(0xF1);
    
    // 设置 VCOMH
    OLED_WriteCmd(OLED_CMD_SET_VCOMH);
    OLED_WriteCmd(0x40);
    
    // 从 RAM 显示
    OLED_WriteCmd(OLED_CMD_DISPLAY_RAM);
    
    // 正常显示 (非反色)
    OLED_WriteCmd(OLED_CMD_DISPLAY_NORMAL);
    
    // 停止滚动
    OLED_WriteCmd(0x2E);
    
    // 清屏
    OLED_Clear();
    OLED_Refresh();
    
    // 开启显示
    OLED_WriteCmd(OLED_CMD_DISPLAY_ON);
    
    HAL_Delay(100);
}
