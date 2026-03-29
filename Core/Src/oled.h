/**
 * @file    oled.h
 * @brief   SSD1306 OLED 驱动头文件
 * @author  Embedded Driver Expert
 * @date    2026-03-22
 * 
 * @note    基于 STM32 HAL 库的 I2C 驱动，支持 128x64 分辨率
 *          采用本地显存缓冲 (Frame Buffer) 机制，所有绘图操作先写入 GRAM
 *          然后通过 OLED_Refresh() 批量刷新到屏幕
 * 
 * @warning 需确保 I2C 总线速率不超过 400kHz (Fast Mode)
 *          刷新操作会阻塞，建议在主循环或低优先级任务中调用
 */

#ifndef __OLED_H
#define __OLED_H

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 *                              头文件包含
 *=============================================================================*/
#include "stm32f1xx_hal.h"  /* 根据实际芯片型号修改，如 stm32f4xx_hal.h */
#include "oledfont.h"
#include <stdint.h>
#include <string.h>

/*==============================================================================
 *                              硬件配置宏定义
 *=============================================================================*/

/**
 * @brief OLED I2C 设备地址 (8-bit 格式)
 * @note  0x78 = 0b01111000 (写操作)
 *        7-bit 地址为 0x3C，左移一位后为 0x78
 */
#define OLED_I2C_ADDR           0x78

/**
 * @brief OLED 分辨率定义
 */
#define OLED_WIDTH              128     /*!< 屏幕宽度 (像素) */
#define OLED_HEIGHT             64      /*!< 屏幕高度 (像素) */

/**
 * @brief 显存缓冲区尺寸
 * @note  SSD1306 采用页寻址模式，每页 8 像素高
 *        64 像素高 = 8 页，每页 128 字节
 *        总缓冲: 8 x 128 = 1024 字节
 */
#define OLED_PAGE_NUM           8       /*!< 页数 (64/8) */
#define OLED_PAGE_SIZE          128     /*!< 每页字节数 */

/*==============================================================================
 *                              SSD1306 命令定义
 *=============================================================================*/

/* 基本命令 */
#define OLED_CMD_SET_CONTRAST   0x81    /*!< 设置对比度 */
#define OLED_CMD_DISPLAY_RAM    0xA4    /*!< 从 RAM 显示 */
#define OLED_CMD_DISPLAY_ALLON  0xA5    /*!< 全屏点亮 (测试用) */
#define OLED_CMD_DISPLAY_NORMAL 0xA6    /*!< 正常显示 */
#define OLED_CMD_DISPLAY_INVERSE 0xA7   /*!< 反色显示 */
#define OLED_CMD_DISPLAY_OFF    0xAE    /*!< 关闭显示 */
#define OLED_CMD_DISPLAY_ON     0xAF    /*!< 开启显示 */

/* 滚动命令 */
#define OLED_CMD_DEACTIVATE_SCROLL  0x2E    /*!< 停止滚动 */
#define OLED_CMD_ACTIVATE_SCROLL    0x2F    /*!< 开始滚动 */

/* 地址设置命令 */
#define OLED_CMD_SET_MEMORY_ADDR_MODE   0x20    /*!< 设置内存寻址模式 */
#define OLED_CMD_SET_COLUMN_ADDR        0x21    /*!< 设置列地址 */
#define OLED_CMD_SET_PAGE_ADDR          0x22    /*!< 设置页地址 */

/* 寻址模式 */
#define OLED_ADDR_MODE_HORIZONTAL   0x00    /*!< 水平寻址模式 */
#define OLED_ADDR_MODE_VERTICAL     0x01    /*!< 垂直寻址模式 */
#define OLED_ADDR_MODE_PAGE         0x02    /*!< 页寻址模式 (默认) */

/* 硬件配置命令 */
#define OLED_CMD_SET_START_LINE     0x40    /*!< 设置显示起始行 (0-63) */
#define OLED_CMD_SET_SEGMENT_REMAP  0xA1    /*!< 段重映射 (列地址127映射到SEG0) */
#define OLED_CMD_SET_MULTIPLEX      0xA8    /*!< 设置复用率 */
#define OLED_CMD_SET_COM_SCAN_DIR   0xC8    /*!< COM 扫描方向 (从下到上) */
#define OLED_CMD_SET_DISPLAY_OFFSET 0xD3    /*!< 设置显示偏移 */
#define OLED_CMD_SET_COM_PINS       0xDA    /*!< 设置 COM 引脚配置 */

/* 时钟和电荷泵命令 */
#define OLED_CMD_SET_DISPLAY_CLOCK  0xD5    /*!< 设置显示时钟分频 */
#define OLED_CMD_SET_PRECHARGE      0xD9    /*!< 设置预充电周期 */
#define OLED_CMD_SET_VCOMH          0xDB    /*!< 设置 VCOMH 取消选择电平 */
#define OLED_CMD_CHARGE_PUMP        0x8D    /*!< 电荷泵设置 */

/* 电荷泵值 */
#define OLED_CHARGE_PUMP_DISABLE    0x10    /*!< 禁用电荷泵 */
#define OLED_CHARGE_PUMP_ENABLE     0x14    /*!< 启用电荷泵 */

/*==============================================================================
 *                              颜色定义
 *=============================================================================*/
#define OLED_COLOR_BLACK    0   /*!< 黑色 (灭) */
#define OLED_COLOR_WHITE    1   /*!< 白色 (亮) */
#define OLED_COLOR_INVERT   2   /*!< 反色 (异或操作) */

/*==============================================================================
 *                              外部变量声明
 *=============================================================================*/

/**
 * @brief 外部 I2C 句柄声明
 * @note  用户需在 main.c 或其他地方定义并初始化 hi2c1
 */
extern I2C_HandleTypeDef hi2c1;

/*==============================================================================
 *                              API 函数声明
 *=============================================================================*/

/*------------------------------------------------------------------------------
 * 底层 I2C 操作
 *------------------------------------------------------------------------------*/

/**
 * @brief  向 OLED 写入单条命令
 * @param  cmd: 要写入的命令字节
 * @retval None
 * @note   使用 HAL_I2C_Mem_Write，MemAddress = 0x00 (命令寄存器)
 */
void OLED_WriteCmd(uint8_t cmd);

/**
 * @brief  向 OLED 批量写入数据
 * @param  data: 数据缓冲区指针
 * @param  size: 要写入的数据字节数
 * @retval None
 * @note   使用 HAL_I2C_Mem_Write，MemAddress = 0x40 (数据寄存器)
 *         最大可连续写入 1024 字节 (整屏数据)
 */
void OLED_WriteData(uint8_t *data, uint16_t size);

/*------------------------------------------------------------------------------
 * 初始化与电源管理
 *------------------------------------------------------------------------------*/

/**
 * @brief  OLED 初始化函数
 * @param  None
 * @retval None
 * @note   严格按照 SSD1306 手册配置：
 *         - 开启电荷泵
 *         - 设置复用率 1/64
 *         - 设置硬件引脚配置
 *         - 设置对比度
 *         - 清屏并开启显示
 * @warning 调用前需确保 hi2c1 已初始化完成
 */
void OLED_Init(void);

/**
 * @brief  开启 OLED 显示
 * @param  None
 * @retval None
 * @note   发送 0xAF 命令，并确保电荷泵已启用
 */
void OLED_Display_On(void);

/**
 * @brief  关闭 OLED 显示 (休眠模式)
 * @param  None
 * @retval None
 * @note   发送 0xAE 命令，屏幕进入低功耗状态
 */
void OLED_Display_Off(void);

/*------------------------------------------------------------------------------
 * 显存缓冲区操作
 *------------------------------------------------------------------------------*/

/**
 * @brief  将本地显存缓冲区 (GRAM) 全量刷新到 OLED 屏幕
 * @param  None
 * @retval None
 * @note   使用页寻址模式，通过单次 I2C 事务发送 1024 字节数据
 *         刷新整屏约需 25ms @ 400kHz I2C
 */
void OLED_Refresh(void);

/**
 * @brief  清除本地显存缓冲区 (GRAM)
 * @param  None
 * @retval None
 * @note   将 OLED_GRAM 全部清零 (填充黑色)
 *         调用后需执行 OLED_Refresh() 才能在屏幕上看到效果
 */
void OLED_Clear(void);

/**
 * @brief  填充整个屏幕为指定颜色
 * @param  color: 填充颜色 (OLED_COLOR_BLACK 或 OLED_COLOR_WHITE)
 * @retval None
 */
void OLED_Fill(uint8_t color);

/*------------------------------------------------------------------------------
 * 基本图形绘制 (基于 GRAM)
 *------------------------------------------------------------------------------*/

/**
 * @brief  在指定坐标画点
 * @param  x: X 坐标 (0 ~ 127)
 * @param  y: Y 坐标 (0 ~ 63)
 * @param  color: 颜色 (OLED_COLOR_BLACK, OLED_COLOR_WHITE, OLED_COLOR_INVERT)
 * @retval None
 * @note   核心算法：通过位操作修改 GRAM 中对应位
 *         计算公式：page = y / 8, bit = y % 8
 *         具有边界保护，超出屏幕范围自动返回
 */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);

/**
 * @brief  获取指定坐标的像素颜色
 * @param  x: X 坐标 (0 ~ 127)
 * @param  y: Y 坐标 (0 ~ 63)
 * @retval 颜色值 (0 或 1)，坐标非法时返回 0
 */
uint8_t OLED_GetPoint(uint8_t x, uint8_t y);

/**
 * @brief  绘制直线 (Bresenham 算法)
 * @param  x1: 起点 X 坐标
 * @param  y1: 起点 Y 坐标
 * @param  x2: 终点 X 坐标
 * @param  y2: 终点 Y 坐标
 * @param  color: 颜色
 * @retval None
 * @note   支持任意斜率，包含端点
 */
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);

/**
 * @brief  绘制矩形 (空心)
 * @param  x: 左上角 X 坐标
 * @param  y: 左上角 Y 坐标
 * @param  width: 矩形宽度
 * @param  height: 矩形高度
 * @param  color: 颜色
 * @retval None
 */
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color);

/**
 * @brief  绘制填充矩形 (实心)
 * @param  x: 左上角 X 坐标
 * @param  y: 左上角 Y 坐标
 * @param  width: 矩形宽度
 * @param  height: 矩形高度
 * @param  color: 填充颜色
 * @retval None
 */
void OLED_FillRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color);

/**
 * @brief  绘制圆 (空心，中点圆算法)
 * @param  x: 圆心 X 坐标
 * @param  y: 圆心 Y 坐标
 * @param  r: 半径
 * @param  color: 颜色
 * @retval None
 * @note   使用整数运算，无浮点开销
 */
void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t r, uint8_t color);

/**
 * @brief  绘制填充圆 (实心)
 * @param  x: 圆心 X 坐标
 * @param  y: 圆心 Y 坐标
 * @param  r: 半径
 * @param  color: 填充颜色
 * @retval None
 */
void OLED_FillCircle(uint8_t x, uint8_t y, uint8_t r, uint8_t color);

/*------------------------------------------------------------------------------
 * 文本显示 (基于 GRAM)
 *------------------------------------------------------------------------------*/

/**
 * @brief  显示单个 ASCII 字符
 * @param  x: 起始 X 坐标
 * @param  y: 起始 Y 坐标 (页对齐，即 0, 8, 16, 24...)
 * @param  chr: 要显示的字符 (ASCII 32-126)
 * @param  size: 字体大小 (8 表示 6x8，16 表示 8x16)
 * @param  color: 颜色
 * @retval None
 * @note   size=8 时高度为 8 像素，size=16 时高度为 16 像素
 *         Y 坐标建议按 8 像素对齐以获得最佳显示效果
 */
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size, uint8_t color);

/**
 * @brief  显示字符串
 * @param  x: 起始 X 坐标
 * @param  y: 起始 Y 坐标
 * @param  str: 要显示的字符串 (以 '\0' 结尾)
 * @param  size: 字体大小 (8 或 16)
 * @param  color: 颜色
 * @retval None
 * @note   字符串会自动换行，但不会自动滚屏
 *         超出屏幕部分将被截断
 */
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t size, uint8_t color);

/**
 * @brief  显示无符号整数
 * @param  x: 起始 X 坐标
 * @param  y: 起始 Y 坐标
 * @param  num: 要显示的数字 (0 ~ 4294967295)
 * @param  len: 显示位数 (前导零填充)
 * @param  size: 字体大小 (8 或 16)
 * @param  color: 颜色
 * @retval None
 * @note   例如：len=5, num=123 将显示 "00123"
 */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color);

/**
 * @brief  显示有符号整数
 * @param  x: 起始 X 坐标
 * @param  y: 起始 Y 坐标
 * @param  num: 要显示的数字 (-2147483648 ~ 2147483647)
 * @param  len: 显示位数 (不含符号位)
 * @param  size: 字体大小 (8 或 16)
 * @param  color: 颜色
 * @retval None
 */
void OLED_ShowSignedNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size, uint8_t color);

/**
 * @brief  显示十六进制数
 * @param  x: 起始 X 坐标
 * @param  y: 起始 Y 坐标
 * @param  num: 要显示的数字
 * @param  len: 显示位数 (前导零填充)
 * @param  size: 字体大小 (8 或 16)
 * @param  color: 颜色
 * @retval None
 * @note   显示格式为大写十六进制，如 "0A3F"
 */
void OLED_ShowHexNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color);

/**
 * @brief  显示二进制数
 * @param  x: 起始 X 坐标
 * @param  y: 起始 Y 坐标
 * @param  num: 要显示的数字
 * @param  len: 显示位数 (前导零填充)
 * @param  size: 字体大小 (8 或 16)
 * @param  color: 颜色
 * @retval None
 */
void OLED_ShowBinNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color);

/**
 * @brief  显示浮点数
 * @param  x: 起始 X 坐标
 * @param  y: 起始 Y 坐标
 * @param  num: 要显示的浮点数
 * @param  int_len: 整数部分位数
 * @param  frac_len: 小数部分位数
 * @param  size: 字体大小 (8 或 16)
 * @param  color: 颜色
 * @retval None
 * @note   注意浮点数精度限制，建议 frac_len <= 6
 */
void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t int_len, uint8_t frac_len, uint8_t size, uint8_t color);

/*------------------------------------------------------------------------------
 * 高级功能
 *------------------------------------------------------------------------------*/

/**
 * @brief  设置屏幕对比度
 * @param  contrast: 对比度值 (0 ~ 255)，默认值 0xCF (207)
 * @retval None
 * @note   值越大对比度越高，在强光环境下可适当调高
 */
void OLED_SetContrast(uint8_t contrast);

/**
 * @brief  屏幕内容旋转 180 度
 * @param  rotate: 0 = 正常方向, 1 = 旋转 180 度
 * @retval None
 * @note   通过设置段重映射和 COM 扫描方向实现
 */
void OLED_SetRotation(uint8_t rotate);

/**
 * @brief  屏幕内容水平/垂直翻转
 * @param  horizontal: 水平翻转 (0 = 正常, 1 = 翻转)
 * @param  vertical: 垂直翻转 (0 = 正常, 1 = 翻转)
 * @retval None
 */
void OLED_SetFlip(uint8_t horizontal, uint8_t vertical);

/**
 * @brief  显示 BMP 位图
 * @param  x: 起始 X 坐标
 * @param  y: 起始 Y 坐标 (页对齐)
 * @param  width: 位图宽度 (像素)
 * @param  height: 位图高度 (像素，必须是 8 的倍数)
 * @param  bmp: 位图数据指针 (每行 8 像素打包为 1 字节)
 * @param  color: 颜色
 * @retval None
 * @note   位图数据格式：从上到下，每行从左到右，MSB 在前
 */
void OLED_DrawBMP(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *bmp, uint8_t color);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H */
