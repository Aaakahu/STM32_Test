/**
 * @file    oled.c
 * @brief   SSD1306 OLED 驱动实现文件
 * @author  Embedded Driver Expert
 * @date    2026-03-22
 * 
 * @note    商业级驱动实现，特点：
 *          1. 基于本地显存缓冲 (Frame Buffer) 机制
 *          2. 所有绘图操作先写入 OLED_GRAM，通过 OLED_Refresh() 批量刷新
 *          3. 完整的边界保护，防止缓冲区溢出
 *          4. 优化的 Bresenham 直线算法和中点圆算法
 *          5. 支持 6x8 和 8x16 两种 ASCII 字体
 */

#include "oled.h"

/*==============================================================================
 *                              本地显存缓冲区 (GRAM)
 *=============================================================================*/

/**
 * @brief 本地显存缓冲区定义
 * @note  SSD1306 采用页寻址模式：
 *        - 屏幕被划分为 8 个水平页 (Page)，每页 8 像素高
 *        - 每页包含 128 列，每列 1 字节 (8 像素垂直排列)
 *        - 字节位定义：Bit0 = 最上方像素, Bit7 = 最下方像素
 *        
 *        坐标到缓冲区的映射：
 *        page = y / 8       (确定在哪一页)
 *        bit  = y % 8       (确定在字节的哪一位)
 *        OLED_GRAM[page][x] 的 bit 位对应坐标 (x, y)
 */
static uint8_t OLED_GRAM[OLED_PAGE_NUM][OLED_PAGE_SIZE];

/*==============================================================================
 *                              静态函数声明
 *=============================================================================*/
static void OLED_SetCursor(uint8_t page, uint8_t column);
static int32_t OLED_Abs(int32_t x);
static void OLED_Swap(int32_t *a, int32_t *b);

/*==============================================================================
 *                              底层 I2C 操作
 *=============================================================================*/

/**
 * @brief 向 OLED 写入单条命令
 * @param cmd 要写入的命令字节
 * @note 使用 HAL_I2C_Mem_Write 提高效率
 *       MemAddress = 0x00 表示命令寄存器
 *       MemAddSize = I2C_MEMADD_SIZE_8BIT
 */
void OLED_WriteCmd(uint8_t cmd)
{
    /* 
     * HAL_I2C_Mem_Write 参数说明：
     * hi2c: I2C 句柄指针
     * DevAddress: 设备地址 (左对齐，HAL 库会自动处理读写位)
     * MemAddress: 内部寄存器地址 (0x00 = 命令, 0x40 = 数据)
     * MemAddSize: 内存地址大小 (8位)
     * pData: 数据缓冲区
     * Size: 数据大小
     * Timeout: 超时时间
     */
    HAL_I2C_Mem_Write(&hi2c1, OLED_I2C_ADDR, 0x00, 
                      I2C_MEMADD_SIZE_8BIT, &cmd, 1, 100);
}

/**
 * @brief 向 OLED 批量写入数据
 * @param data 数据缓冲区指针
 * @param size 要写入的数据字节数
 * @note 使用 HAL_I2C_Mem_Write，MemAddress = 0x40 (数据寄存器)
 *       支持连续写入最多 1024 字节 (整屏数据)
 */
void OLED_WriteData(uint8_t *data, uint16_t size)
{
    /* 参数检查 */
    if (data == NULL || size == 0) {
        return;
    }
    
    /* 限制单次写入大小，防止 I2C 总线过载 */
    if (size > 1024) {
        size = 1024;
    }
    
    HAL_I2C_Mem_Write(&hi2c1, OLED_I2C_ADDR, 0x40,
                      I2C_MEMADD_SIZE_8BIT, data, size, 1000);
}

/*==============================================================================
 *                              初始化与电源管理
 *=============================================================================*/

/**
 * @brief OLED 初始化函数
 * @note 严格按照 SSD1306 手册配置初始化序列
 *       初始化完成后屏幕为关闭状态，需调用 OLED_Display_On() 开启
 */
void OLED_Init(void)
{
    /* 
     * 上电延迟：SSD1306 上电后需要至少 100ms 稳定时间
     * 确保电源稳定后再进行初始化
     */
    HAL_Delay(100);
    
    /* 关闭显示 (必须在配置期间关闭) */
    OLED_WriteCmd(OLED_CMD_DISPLAY_OFF);
    
    /* 设置时钟分频和振荡器频率 */
    /* A[3:0] = 分频比 (默认值 0000b = 分频比 1) */
    /* A[7:4] = 振荡器频率 (默认值 1000b) */
    OLED_WriteCmd(OLED_CMD_SET_DISPLAY_CLOCK);
    OLED_WriteCmd(0x80);  /* 推荐值：分频比 1，振荡器频率 8 */
    
    /* 设置多路复用率 (MUX Ratio) */
    /* 0x3F = 63，表示 64 复用 (0 ~ 63) */
    OLED_WriteCmd(OLED_CMD_SET_MULTIPLEX);
    OLED_WriteCmd(0x3F);
    
    /* 设置显示偏移 */
    /* 无偏移，从 COM0 开始显示 */
    OLED_WriteCmd(OLED_CMD_SET_DISPLAY_OFFSET);
    OLED_WriteCmd(0x00);
    
    /* 设置起始行 */
    /* 0x40 表示起始行为第 0 行 */
    OLED_WriteCmd(OLED_CMD_SET_START_LINE | 0x00);
    
    /* 设置电荷泵 (Charge Pump) */
    /* 必须启用电荷泵才能正常显示 */
    OLED_WriteCmd(OLED_CMD_CHARGE_PUMP);
    OLED_WriteCmd(OLED_CHARGE_PUMP_ENABLE);
    
    /* 设置内存寻址模式 */
    /* 0x00 = 水平寻址，0x01 = 垂直寻址，0x02 = 页寻址 */
    OLED_WriteCmd(OLED_CMD_SET_MEMORY_ADDR_MODE);
    OLED_WriteCmd(OLED_ADDR_MODE_PAGE);
    
    /* 设置段重映射 */
    /* 0xA1 = 列地址 127 映射到 SEG0 (屏幕旋转 180 度时使用) */
    OLED_WriteCmd(OLED_CMD_SET_SEGMENT_REMAP);
    
    /* 设置 COM 扫描方向 */
    /* 0xC8 = COM 从下到上扫描 (配合段重映射实现 180 度旋转) */
    OLED_WriteCmd(OLED_CMD_SET_COM_SCAN_DIR);
    
    /* 设置 COM 引脚硬件配置 */
    /* 0x12 = 替代 COM 引脚配置，禁止 COM 左右反置 */
    OLED_WriteCmd(OLED_CMD_SET_COM_PINS);
    OLED_WriteCmd(0x12);
    
    /* 设置对比度 */
    /* 0xCF = 207，推荐值，可根据环境光线调整 */
    OLED_WriteCmd(OLED_CMD_SET_CONTRAST);
    OLED_WriteCmd(0xCF);
    
    /* 设置预充电周期 */
    /* 0xF1 =  Phase 1: 15 DCLKs, Phase 2: 1 DCLK (推荐值) */
    OLED_WriteCmd(OLED_CMD_SET_PRECHARGE);
    OLED_WriteCmd(0xF1);
    
    /* 设置 VCOMH 取消选择电平 */
    /* 0x40 = ~0.77 x VCC (推荐值) */
    OLED_WriteCmd(OLED_CMD_SET_VCOMH);
    OLED_WriteCmd(0x40);
    
    /* 启用整个显示 (从 RAM 显示) */
    OLED_WriteCmd(OLED_CMD_DISPLAY_RAM);
    
    /* 设置正常显示 (非反色) */
    OLED_WriteCmd(OLED_CMD_DISPLAY_NORMAL);
    
    /* 停止滚动 */
    OLED_WriteCmd(OLED_CMD_DEACTIVATE_SCROLL);
    
    /* 清屏 */
    OLED_Clear();
    OLED_Refresh();
    
    /* 开启显示 */
    OLED_WriteCmd(OLED_CMD_DISPLAY_ON);
}

/**
 * @brief 开启 OLED 显示
 * @note 发送 0xAF 命令唤醒屏幕
 *       同时确保电荷泵处于启用状态
 */
void OLED_Display_On(void)
{
    /* 启用电荷泵 */
    OLED_WriteCmd(OLED_CMD_CHARGE_PUMP);
    OLED_WriteCmd(OLED_CHARGE_PUMP_ENABLE);
    
    /* 开启显示 */
    OLED_WriteCmd(OLED_CMD_DISPLAY_ON);
}

/**
 * @brief 关闭 OLED 显示 (休眠模式)
 * @note 发送 0xAE 命令关闭显示
 *       同时关闭电荷泵进入低功耗模式
 */
void OLED_Display_Off(void)
{
    /* 关闭显示 */
    OLED_WriteCmd(OLED_CMD_DISPLAY_OFF);
    
    /* 关闭电荷泵 (降低功耗) */
    OLED_WriteCmd(OLED_CMD_CHARGE_PUMP);
    OLED_WriteCmd(OLED_CHARGE_PUMP_DISABLE);
}

/*==============================================================================
 *                              显存缓冲区操作
 *=============================================================================*/

/**
 * @brief 设置 OLED 内部光标位置
 * @param page 页地址 (0 ~ 7)
 * @param column 列地址 (0 ~ 127)
 * @note 私有函数，仅在 Refresh 中使用
 */
static void OLED_SetCursor(uint8_t page, uint8_t column)
{
    /* 设置页地址 (0xB0 ~ 0xB7) */
    OLED_WriteCmd(0xB0 + page);
    
    /* 设置列地址低 4 位 */
    OLED_WriteCmd(0x00 + (column & 0x0F));
    
    /* 设置列地址高 4 位 */
    OLED_WriteCmd(0x10 + ((column >> 4) & 0x0F));
}

/**
 * @brief 将本地显存缓冲区全量刷新到 OLED 屏幕
 * @note 刷新策略：逐页刷新，每页 128 字节
 *       总数据量：8 页 x 128 字节 = 1024 字节
 *       刷新时间：约 25ms @ 400kHz I2C
 */
void OLED_Refresh(void)
{
    uint8_t page;
    
    /* 逐页刷新 */
    for (page = 0; page < OLED_PAGE_NUM; page++) {
        /* 设置光标到页起始位置 */
        OLED_SetCursor(page, 0);
        
        /* 批量写入整页数据 (128 字节) */
        OLED_WriteData(OLED_GRAM[page], OLED_PAGE_SIZE);
    }
}

/**
 * @brief 清除本地显存缓冲区
 * @note 将 OLED_GRAM 全部填充为 0x00 (黑色)
 *       调用后需执行 OLED_Refresh() 才能在屏幕上看到效果
 */
void OLED_Clear(void)
{
    /* 使用 memset 快速清零，比循环更高效 */
    memset(OLED_GRAM, 0x00, sizeof(OLED_GRAM));
}

/**
 * @brief 填充整个屏幕为指定颜色
 * @param color 填充颜色 (OLED_COLOR_BLACK = 0, OLED_COLOR_WHITE = 1)
 */
void OLED_Fill(uint8_t color)
{
    uint8_t fill_byte;
    
    /* 根据颜色确定填充字节值 */
    fill_byte = (color == OLED_COLOR_WHITE) ? 0xFF : 0x00;
    
    /* 填充整个缓冲区 */
    memset(OLED_GRAM, fill_byte, sizeof(OLED_GRAM));
}

/*==============================================================================
 *                              辅助函数
 *=============================================================================*/

/**
 * @brief 求绝对值
 * @param x 输入值
 * @return 绝对值
 */
static int32_t OLED_Abs(int32_t x)
{
    return (x < 0) ? -x : x;
}

/**
 * @brief 交换两个整数
 * @param a 第一个数的指针
 * @param b 第二个数的指针
 */
static void OLED_Swap(int32_t *a, int32_t *b)
{
    int32_t temp = *a;
    *a = *b;
    *b = temp;
}

/*==============================================================================
 *                              基本图形绘制
 *=============================================================================*/

/**
 * @brief 在指定坐标画点
 * @param x X 坐标 (0 ~ 127)
 * @param y Y 坐标 (0 ~ 63)
 * @param color 颜色 (0=灭, 1=亮, 2=反色)
 * @note 核心算法：通过位操作修改 GRAM
 *       page = y / 8, bit = y % 8
 *       具有严格的边界保护
 */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color)
{
    uint8_t page, bit_pos;
    
    /* 边界保护：超出屏幕范围直接返回 */
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }
    
    /* 计算页地址和位位置 */
    page = y >> 3;          /* 等价于 y / 8，但更快 */
    bit_pos = y & 0x07;     /* 等价于 y % 8，但更快 */
    
    /* 根据颜色值设置对应位 */
    switch (color) {
        case OLED_COLOR_WHITE:
            /* 置位：将对应 bit 设为 1 */
            OLED_GRAM[page][x] |= (1 << bit_pos);
            break;
            
        case OLED_COLOR_BLACK:
            /* 清零：将对应 bit 设为 0 */
            OLED_GRAM[page][x] &= ~(1 << bit_pos);
            break;
            
        case OLED_COLOR_INVERT:
            /* 反色：将对应 bit 取反 */
            OLED_GRAM[page][x] ^= (1 << bit_pos);
            break;
            
        default:
            break;
    }
}

/**
 * @brief 获取指定坐标的像素颜色
 * @param x X 坐标 (0 ~ 127)
 * @param y Y 坐标 (0 ~ 63)
 * @return 颜色值 (0 或 1)，坐标非法时返回 0
 */
uint8_t OLED_GetPoint(uint8_t x, uint8_t y)
{
    uint8_t page, bit_pos;
    
    /* 边界检查 */
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return 0;
    }
    
    page = y >> 3;
    bit_pos = y & 0x07;
    
    /* 返回对应位的值 */
    return (OLED_GRAM[page][x] >> bit_pos) & 0x01;
}

/**
 * @brief 绘制直线 (Bresenham 算法)
 * @param x1 起点 X 坐标
 * @param y1 起点 Y 坐标
 * @param x2 终点 X 坐标
 * @param y2 终点 Y 坐标
 * @param color 颜色
 * @note Bresenham 直线算法：使用整数运算绘制直线
 *       适用于所有斜率，无需浮点运算
 */
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)
{
    int32_t dx, dy, sx, sy, err, e2;
    int32_t x, y, x_end, y_end;
    
    /* 转换为有符号数进行计算 */
    x = x1;
    y = y1;
    x_end = x2;
    y_end = y2;
    
    /* 计算 delta 值 */
    dx = OLED_Abs(x_end - x);
    dy = OLED_Abs(y_end - y);
    
    /* 确定步进方向 */
    sx = (x < x_end) ? 1 : -1;
    sy = (y < y_end) ? 1 : -1;
    
    /* 初始误差 */
    err = dx - dy;
    
    while (1) {
        /* 绘制当前点 */
        OLED_DrawPoint((uint8_t)x, (uint8_t)y, color);
        
        /* 到达终点 */
        if (x == x_end && y == y_end) {
            break;
        }
        
        /* 计算下一个点 */
        e2 = err * 2;
        
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
        
        /* 安全检查：防止溢出 */
        if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
            if (x < 0) x = 0;
            if (x >= OLED_WIDTH) x = OLED_WIDTH - 1;
            if (y < 0) y = 0;
            if (y >= OLED_HEIGHT) y = OLED_HEIGHT - 1;
        }
    }
}

/**
 * @brief 绘制矩形 (空心)
 * @param x 左上角 X 坐标
 * @param y 左上角 Y 坐标
 * @param width 矩形宽度
 * @param height 矩形高度
 * @param color 颜色
 */
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color)
{
    /* 参数检查 */
    if (width == 0 || height == 0) {
        return;
    }
    
    /* 绘制四条边 */
    /* 上边 */
    OLED_DrawLine(x, y, x + width - 1, y, color);
    /* 下边 */
    OLED_DrawLine(x, y + height - 1, x + width - 1, y + height - 1, color);
    /* 左边 */
    OLED_DrawLine(x, y, x, y + height - 1, color);
    /* 右边 */
    OLED_DrawLine(x + width - 1, y, x + width - 1, y + height - 1, color);
}

/**
 * @brief 绘制填充矩形 (实心)
 * @param x 左上角 X 坐标
 * @param y 左上角 Y 坐标
 * @param width 矩形宽度
 * @param height 矩形高度
 * @param color 填充颜色
 * @note 优化算法：直接操作 GRAM 字节，而非逐点绘制
 */
void OLED_FillRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color)
{
    uint8_t page_start, page_end, bit_start, bit_end;
    uint8_t page, col, mask;
    uint8_t x_end, y_end;
    
    /* 参数检查 */
    if (width == 0 || height == 0) {
        return;
    }
    
    /* 计算边界 */
    x_end = x + width - 1;
    y_end = y + height - 1;
    
    /* 边界保护 */
    if (x_end >= OLED_WIDTH) x_end = OLED_WIDTH - 1;
    if (y_end >= OLED_HEIGHT) y_end = OLED_HEIGHT - 1;
    
    /* 计算涉及的页 */
    page_start = y >> 3;
    page_end = y_end >> 3;
    bit_start = y & 0x07;
    bit_end = y_end & 0x07;
    
    /* 逐页填充 */
    for (page = page_start; page <= page_end; page++) {
        /* 计算该页需要填充的位掩码 */
        if (page == page_start && page == page_end) {
            /* 矩形在同一页内 */
            mask = ((1 << (bit_end - bit_start + 1)) - 1) << bit_start;
        } else if (page == page_start) {
            /* 起始页：从 bit_start 填充到页末尾 */
            mask = (0xFF << bit_start) & 0xFF;
        } else if (page == page_end) {
            /* 结束页：从页起始填充到 bit_end */
            mask = (1 << (bit_end + 1)) - 1;
        } else {
            /* 中间页：整页填充 */
            mask = 0xFF;
        }
        
        /* 根据颜色应用掩码 */
        for (col = x; col <= x_end; col++) {
            if (color == OLED_COLOR_WHITE) {
                OLED_GRAM[page][col] |= mask;
            } else {
                OLED_GRAM[page][col] &= ~mask;
            }
        }
    }
}

/**
 * @brief 绘制圆 (空心，中点圆算法)
 * @param x0 圆心 X 坐标
 * @param y0 圆心 Y 坐标
 * @param r 半径
 * @param color 颜色
 * @note 中点圆算法 (Midpoint Circle Algorithm)：
 *       利用圆的八对称性，只需计算 1/8 圆弧
 *       其余部分通过对称性复制
 */
void OLED_DrawCircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color)
{
    int32_t x, y, err;
    
    /* 参数检查 */
    if (r == 0) {
        OLED_DrawPoint(x0, y0, color);
        return;
    }
    
    /* 初始化 */
    x = r;
    y = 0;
    err = 0;
    
    while (x >= y) {
        /* 利用八对称性，绘制 8 个点 */
        OLED_DrawPoint(x0 + x, y0 + y, color);
        OLED_DrawPoint(x0 + y, y0 + x, color);
        OLED_DrawPoint(x0 - y, y0 + x, color);
        OLED_DrawPoint(x0 - x, y0 + y, color);
        OLED_DrawPoint(x0 - x, y0 - y, color);
        OLED_DrawPoint(x0 - y, y0 - x, color);
        OLED_DrawPoint(x0 + y, y0 - x, color);
        OLED_DrawPoint(x0 + x, y0 - y, color);
        
        /* 更新决策参数 */
        if (err <= 0) {
            y++;
            err += 2 * y + 1;
        }
        
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

/**
 * @brief 绘制填充圆 (实心)
 * @param x0 圆心 X 坐标
 * @param y0 圆心 Y 坐标
 * @param r 半径
 * @param color 填充颜色
 * @note 使用水平线填充圆的每一行
 */
void OLED_FillCircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color)
{
    int32_t x, y, err;
    
    /* 参数检查 */
    if (r == 0) {
        OLED_DrawPoint(x0, y0, color);
        return;
    }
    
    /* 初始化 */
    x = r;
    y = 0;
    err = 0;
    
    while (x >= y) {
        /* 绘制水平线填充 */
        OLED_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
        OLED_DrawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
        OLED_DrawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
        OLED_DrawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);
        
        /* 更新决策参数 */
        if (err <= 0) {
            y++;
            err += 2 * y + 1;
        }
        
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

/*==============================================================================
 *                              文本显示
 *=============================================================================*/

/**
 * @brief 显示单个 ASCII 字符
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标 (页对齐，建议 0, 8, 16, 24...)
 * @param chr 要显示的字符 (ASCII 32-126)
 * @param size 字体大小 (8=6x8, 16=8x16)
 * @param color 颜色
 * @note 字符绘制原理：
 *       6x8 字体：每个字符 6 字节，每字节表示一列的 8 个像素
 *       8x16 字体：每个字符 16 字节，分上下两部分各 8 字节
 */
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size, uint8_t color)
{
    uint8_t i, j;
    uint8_t page, temp;
    uint8_t char_index;
    
    /* 边界检查 */
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }
    
    /* 字符范围检查 */
    if (chr < 32 || chr > 126) {
        chr = '?';  /* 不可打印字符显示为问号 */
    }
    
    /* 计算字符在字库中的索引 */
    char_index = chr - 32;
    
    /* 计算起始页 */
    page = y >> 3;
    
    if (size == 8) {
        /* 6x8 字体处理 */
        for (i = 0; i < 6; i++) {
            /* 获取字模数据 */
            temp = OLED_F6x8[char_index][i];
            
            /* 颜色处理 (反色时取反) */
            if (color == OLED_COLOR_BLACK) {
                temp = ~temp;
            }
            
            /* 写入显存 */
            if ((x + i) < OLED_WIDTH && page < OLED_PAGE_NUM) {
                OLED_GRAM[page][x + i] = temp;
            }
        }
    } else if (size == 16) {
        /* 8x16 字体处理 (分两页) */
        for (i = 0; i < 8; i++) {
            /* 上半部分 (前 8 字节) */
            temp = OLED_F8x16[char_index][i];
            if (color == OLED_COLOR_BLACK) {
                temp = ~temp;
            }
            if ((x + i) < OLED_WIDTH && page < OLED_PAGE_NUM) {
                OLED_GRAM[page][x + i] = temp;
            }
            
            /* 下半部分 (后 8 字节) */
            temp = OLED_F8x16[char_index][i + 8];
            if (color == OLED_COLOR_BLACK) {
                temp = ~temp;
            }
            if ((x + i) < OLED_WIDTH && (page + 1) < OLED_PAGE_NUM) {
                OLED_GRAM[page + 1][x + i] = temp;
            }
        }
    }
}

/**
 * @brief 显示字符串
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param str 要显示的字符串
 * @param size 字体大小 (8 或 16)
 * @param color 颜色
 * @note 字符串会自动换行，当 X 坐标超出屏幕宽度时换到下一行
 */
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t size, uint8_t color)
{
    uint8_t char_width;
    
    /* 参数检查 */
    if (str == NULL) {
        return;
    }
    
    /* 根据字体大小确定字符宽度 */
    char_width = (size == 8) ? 6 : 8;
    
    /* 逐字符显示 */
    while (*str != '\0') {
        /* 检查是否需要换行 */
        if (x + char_width > OLED_WIDTH) {
            x = 0;
            y += (size == 8) ? 8 : 16;
            
            /* 检查是否超出屏幕高度 */
            if (y >= OLED_HEIGHT) {
                break;
            }
        }
        
        /* 显示当前字符 */
        OLED_ShowChar(x, y, *str, size, color);
        
        /* 移动坐标 */
        x += char_width;
        str++;
    }
}

/**
 * @brief 显示无符号整数
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param num 要显示的数字
 * @param len 显示位数 (前导零填充)
 * @param size 字体大小
 * @param color 颜色
 * @note 数字转换为字符串后显示
 */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color)
{
    char str[12] = {0};  /* 最大 10 位数字 + 结束符 + 预留 */
    uint8_t i;
    
    /* 数字转字符串，前导零填充 */
    for (i = 0; i < len; i++) {
        str[len - 1 - i] = '0' + (num % 10);
        num /= 10;
    }
    str[len] = '\0';
    
    OLED_ShowString(x, y, str, size, color);
}

/**
 * @brief 显示有符号整数
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param num 要显示的数字
 * @param len 数字位数 (不含符号)
 * @param size 字体大小
 * @param color 颜色
 */
void OLED_ShowSignedNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size, uint8_t color)
{
    uint8_t char_width;
    
    char_width = (size == 8) ? 6 : 8;
    
    /* 显示符号 */
    if (num < 0) {
        OLED_ShowChar(x, y, '-', size, color);
        num = -num;
    } else {
        OLED_ShowChar(x, y, '+', size, color);
    }
    
    /* 显示数字 */
    OLED_ShowNum(x + char_width, y, (uint32_t)num, len, size, color);
}

/**
 * @brief 显示十六进制数
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param num 要显示的数字
 * @param len 显示位数
 * @param size 字体大小
 * @param color 颜色
 * @note 显示格式为大写十六进制
 */
void OLED_ShowHexNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color)
{
    char str[12] = {0};
    uint8_t i;
    uint8_t nibble;
    
    for (i = 0; i < len; i++) {
        nibble = num & 0x0F;
        if (nibble < 10) {
            str[len - 1 - i] = '0' + nibble;
        } else {
            str[len - 1 - i] = 'A' + (nibble - 10);
        }
        num >>= 4;
    }
    str[len] = '\0';
    
    OLED_ShowString(x, y, str, size, color);
}

/**
 * @brief 显示二进制数
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param num 要显示的数字
 * @param len 显示位数
 * @param size 字体大小
 * @param color 颜色
 */
void OLED_ShowBinNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color)
{
    char str[33] = {0};  /* 最大 32 位 + 结束符 */
    uint8_t i;
    
    for (i = 0; i < len; i++) {
        str[len - 1 - i] = '0' + (num & 0x01);
        num >>= 1;
    }
    str[len] = '\0';
    
    OLED_ShowString(x, y, str, size, color);
}

/**
 * @brief 显示浮点数
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param num 要显示的浮点数
 * @param int_len 整数部分位数
 * @param frac_len 小数部分位数
 * @param size 字体大小
 * @param color 颜色
 * @note 使用整数运算避免浮点精度问题
 */
void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t int_len, uint8_t frac_len, uint8_t size, uint8_t color)
{
    uint8_t char_width;
    uint32_t int_part, frac_part;
    float frac;
    
    char_width = (size == 8) ? 6 : 8;
    
    /* 处理负数 */
    if (num < 0) {
        OLED_ShowChar(x, y, '-', size, color);
        num = -num;
        x += char_width;
    }
    
    /* 分离整数和小数部分 */
    int_part = (uint32_t)num;
    frac = num - (float)int_part;
    
    /* 计算小数部分 */
    /* 乘以 10^frac_len 转换为整数 */
    for (uint8_t i = 0; i < frac_len; i++) {
        frac *= 10;
    }
    frac_part = (uint32_t)(frac + 0.5f);  /* 四舍五入 */
    
    /* 显示整数部分 */
    OLED_ShowNum(x, y, int_part, int_len, size, color);
    x += int_len * char_width;
    
    /* 显示小数点 */
    OLED_ShowChar(x, y, '.', size, color);
    x += char_width;
    
    /* 显示小数部分 */
    OLED_ShowNum(x, y, frac_part, frac_len, size, color);
}

/*==============================================================================
 *                              高级功能
 *=============================================================================*/

/**
 * @brief 设置屏幕对比度
 * @param contrast 对比度值 (0 ~ 255)
 */
void OLED_SetContrast(uint8_t contrast)
{
    OLED_WriteCmd(OLED_CMD_SET_CONTRAST);
    OLED_WriteCmd(contrast);
}

/**
 * @brief 屏幕内容旋转 180 度
 * @param rotate 0 = 正常, 1 = 旋转 180 度
 * @note 通过设置段重映射和 COM 扫描方向实现
 */
void OLED_SetRotation(uint8_t rotate)
{
    if (rotate) {
        /* 旋转 180 度：正常段映射 + 正常 COM 扫描 */
        OLED_WriteCmd(0xA0);  /* 段映射正常 */
        OLED_WriteCmd(0xC0);  /* COM 扫描从上到下 */
    } else {
        /* 正常方向：反向段映射 + 反向 COM 扫描 */
        OLED_WriteCmd(OLED_CMD_SET_SEGMENT_REMAP);  /* 0xA1 */
        OLED_WriteCmd(OLED_CMD_SET_COM_SCAN_DIR);   /* 0xC8 */
    }
    
    /* 刷新显示 */
    OLED_Refresh();
}

/**
 * @brief 屏幕内容水平/垂直翻转
 * @param horizontal 水平翻转
 * @param vertical 垂直翻转
 */
void OLED_SetFlip(uint8_t horizontal, uint8_t vertical)
{
    /* 段重映射控制水平翻转 */
    if (horizontal) {
        OLED_WriteCmd(0xA0);  /* 正常映射 */
    } else {
        OLED_WriteCmd(0xA1);  /* 反向映射 */
    }
    
    /* COM 扫描方向控制垂直翻转 */
    if (vertical) {
        OLED_WriteCmd(0xC0);  /* 从上到下 */
    } else {
        OLED_WriteCmd(0xC8);  /* 从下到上 */
    }
    
    OLED_Refresh();
}

/**
 * @brief 显示 BMP 位图
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标 (页对齐)
 * @param width 位图宽度
 * @param height 位图高度 (必须是 8 的倍数)
 * @param bmp 位图数据指针
 * @param color 颜色
 * @note 位图数据格式：从上到下，每行从左到右，MSB 在前
 */
void OLED_DrawBMP(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *bmp, uint8_t color)
{
    uint8_t page, page_num, col;
    uint8_t i, j;
    uint8_t temp;
    
    /* 参数检查 */
    if (bmp == NULL || width == 0 || height == 0) {
        return;
    }
    
    /* 计算页数 */
    page_num = height >> 3;  /* height / 8 */
    if (height & 0x07) {     /* 有余数则加一页 */
        page_num++;
    }
    
    /* 起始页 */
    page = y >> 3;
    
    /* 逐页复制位图数据 */
    for (i = 0; i < page_num && (page + i) < OLED_PAGE_NUM; i++) {
        for (j = 0; j < width && (x + j) < OLED_PAGE_SIZE; j++) {
            /* 获取位图数据 */
            temp = bmp[i * width + j];
            
            /* 颜色处理 */
            if (color == OLED_COLOR_BLACK) {
                temp = ~temp;
            }
            
            /* 写入显存 */
            OLED_GRAM[page + i][x + j] = temp;
        }
    }
}
