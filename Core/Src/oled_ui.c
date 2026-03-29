#include "oled_ui.h"
#include "ssd1306.h"
#include "oled_font.h"
#include <stdlib.h>
#include <string.h>

// 外部函数声明（来自 ssd1306.c）
extern void OLED_WriteCommand(uint8_t cmd);

// I2C 底层函数（需要在 ssd1306.c 中暴露）
extern void I2C_Start(void);
extern void I2C_Stop(void);
extern uint8_t I2C_SendByte(uint8_t byte);

// 128x32 屏幕分区定义 - 避免重叠显示
const OLED_Area_t OLED_Areas[3] = {
    {0,  0,  128, 8,  0, 0},   // AREA_STATUS:    顶部 0-7px (1 page)
    {0,  8,  128, 16, 1, 2},   // AREA_CONTENT:   中部 8-23px (2 pages) - 主显示区
    {0, 24, 128, 8,  3, 3}     // AREA_BOTTOM:    底部 24-31px (1 page)
};

static uint8_t OLED_Buffer[OLED_PAGES][OLED_WIDTH];  // 显示缓冲区

// 清屏
void OLED_UI_Clear(void) {
    memset(OLED_Buffer, 0, sizeof(OLED_Buffer));
    OLED_Clear();
}

// 清指定区域
void OLED_UI_ClearArea(uint8_t area_id) {
    if (area_id > 2) return;
    const OLED_Area_t *a = &OLED_Areas[area_id];
    for (uint8_t page = a->page_start; page <= a->page_end; page++) {
        memset(OLED_Buffer[page], 0, OLED_WIDTH);
    }
    // 清除可能的下一页（防止字符越界污染）
    if (a->page_end + 1 < OLED_PAGES) {
        memset(OLED_Buffer[a->page_end + 1], 0, OLED_WIDTH);
    }
}

// 刷新显示
void OLED_UI_Refresh(void) {
    OLED_RefreshGram();
}

// 批量写入数据到OLED（连续传输）
static void OLED_WriteDataMulti(uint8_t *data, uint16_t len) {
    if (data == NULL || len == 0) return;
    
    I2C_Start();
    I2C_SendByte(OLED_I2C_ADDR);  // 设备地址 + 写
    I2C_SendByte(0x40);            // 控制字节: 数据模式
    
    for (uint16_t i = 0; i < len; i++) {
        I2C_SendByte(data[i]);
    }
    
    I2C_Stop();
}

// 将缓冲区内容写入OLED
void OLED_UI_DrawArea(uint8_t area_id) {
    if (area_id > 2) return;
    const OLED_Area_t *a = &OLED_Areas[area_id];
    
    for (uint8_t page = a->page_start; page <= a->page_end; page++) {
        OLED_Set_Pos(0, page);
        OLED_WriteDataMulti(OLED_Buffer[page], OLED_WIDTH);
    }
}

// 在缓冲区显示字符 (6x8字体) - 简化版，只支持页对齐的Y坐标
static void OLED_UI_DrawChar(uint8_t x, uint8_t y, char chr) {
    uint8_t c = chr - ' ';
    if (c >= 96 || x > OLED_WIDTH - 6 || y > OLED_HEIGHT - 8) return;
    
    // 强制页对齐，避免位操作问题
    uint8_t page = (y / 8) % OLED_PAGES;
    
    for (uint8_t i = 0; i < 6; i++) {
        if (x + i < OLED_WIDTH) {
            OLED_Buffer[page][x + i] = asc2_0608[c][i];
        }
    }
}

// 显示字符串到指定区域
void OLED_UI_ShowInArea(uint8_t area_id, uint8_t x, uint8_t y, const char *str) {
    if (area_id > 2 || str == NULL) return;
    const OLED_Area_t *a = &OLED_Areas[area_id];
    
    // 坐标转换到绝对坐标
    uint8_t abs_x = a->x + x;
    uint8_t abs_y = a->y + y;
    
    // 检查边界（确保字符不会超出区域底部，字符高度8）
    if (abs_x >= a->x + a->width) return;
    if (abs_y + 8 > a->y + a->height) return;
    
    while (*str && abs_x + 6 <= a->x + a->width) {
        OLED_UI_DrawChar(abs_x, abs_y, *str++);
        abs_x += 6;
    }
}

// printf格式化显示
void OLED_UI_PrintfInArea(uint8_t area_id, uint8_t x, uint8_t y, const char *fmt, ...) {
    char buf[32];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    OLED_UI_ShowInArea(area_id, x, y, buf);
}

// 初始化滚动文本
void OLED_UI_ScrollText_Init(ScrollText_t *scroll, const char *text, uint8_t speed) {
    strncpy(scroll->text, text, sizeof(scroll->text) - 1);
    scroll->text[sizeof(scroll->text) - 1] = '\0';
    scroll->text_len = strlen(scroll->text);
    scroll->offset = 0;
    scroll->direction = SCROLL_LEFT;
    scroll->speed = speed ? speed : 10;  // 默认10帧滚动一次
    scroll->counter = 0;
    scroll->paused = 0;
    
    // 初始化显示缓冲区
    memset(scroll->display, 0, sizeof(scroll->display));
}

// 更新滚动文本显示
void OLED_UI_ScrollText_Update(ScrollText_t *scroll, uint8_t area_id, uint8_t y) {
    if (scroll->paused || scroll->text_len == 0) return;
    
    // 计数器递增
    scroll->counter++;
    if (scroll->counter < scroll->speed) return;
    scroll->counter = 0;
    
    const OLED_Area_t *a = &OLED_Areas[area_id];
    uint8_t max_chars = a->width / 6;  // 6x8字体能显示的最大字符数
    
    // 计算显示内容
    memset(scroll->display, ' ', max_chars);
    scroll->display[max_chars] = '\0';
    
    // 填充可见字符
    for (uint8_t i = 0; i < max_chars; i++) {
        uint8_t text_idx = (scroll->offset + i) % scroll->text_len;
        scroll->display[i] = scroll->text[text_idx];
    }
    
    // 更新偏移量
    scroll->offset++;
    if (scroll->offset >= scroll->text_len) {
        scroll->offset = 0;
    }
    
    // 清除并显示
    OLED_UI_ClearArea(area_id);
    OLED_UI_ShowInArea(area_id, 0, y, scroll->display);
    OLED_UI_DrawArea(area_id);
}

// 获取字符串宽度
uint8_t OLED_UI_GetStrWidth(const char *str) {
    return strlen(str) * 6;
}

// 绘制水平线
void OLED_UI_DrawHLine(uint8_t x, uint8_t y, uint8_t len) {
    uint8_t page = y / 8;
    uint8_t bit = y % 8;
    if (page >= OLED_PAGES) return;
    for (uint8_t i = 0; i < len && x + i < OLED_WIDTH; i++) {
        OLED_Buffer[page][x + i] |= (1 << bit);
    }
}

// 绘制垂直线
void OLED_UI_DrawVLine(uint8_t x, uint8_t y, uint8_t len) {
    for (uint8_t i = 0; i < len && y + i < OLED_HEIGHT; i++) {
        uint8_t page = (y + i) / 8;
        uint8_t bit = (y + i) % 8;
        if (page < OLED_PAGES) {
            OLED_Buffer[page][x] |= (1 << bit);
        }
    }
}

// 绘制图标
void OLED_UI_DrawIcon(uint8_t x, uint8_t y, const uint8_t *icon, uint8_t width, uint8_t height) {
    uint8_t pages = (height + 7) / 8;
    for (uint8_t p = 0; p < pages && (y / 8 + p) < OLED_PAGES; p++) {
        for (uint8_t w = 0; w < width && x + w < OLED_WIDTH; w++) {
            OLED_Buffer[y / 8 + p][x + w] = icon[p * width + w];
        }
    }
}

// 电池图标 16x8
static const uint8_t icon_battery_empty[] = {
    0x3E, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x3E, 0x00
};

static const uint8_t icon_battery_full[] = {
    0x3E, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x3E, 0x00
};

// 绘制电池
void OLED_UI_DrawBattery(uint8_t x, uint8_t y, uint8_t percent) {
    // 简化为百分比数字显示
    char buf[5];
    snprintf(buf, sizeof(buf), "%3d%%", percent > 100 ? 100 : percent);
    OLED_UI_ShowInArea(AREA_STATUS, x, y, buf);
}

// 信号强度 8x8
static const uint8_t icon_signal[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18,  // 1格
    0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x18, 0x18,  // 2格
    0x00, 0x00, 0x7E, 0x7E, 0x3C, 0x3C, 0x18, 0x18,  // 3格
    0xFF, 0xFF, 0x7E, 0x7E, 0x3C, 0x3C, 0x18, 0x18   // 满格
};

void OLED_UI_DrawSignal(uint8_t x, uint8_t y, uint8_t strength) {
    // 简化为数字显示
    char buf[3];
    snprintf(buf, sizeof(buf), "S%d", strength > 4 ? 4 : strength);
    OLED_UI_ShowInArea(AREA_STATUS, x, y, buf);
}

// 初始化
void OLED_UI_Init(void) {
    OLED_Init();
    OLED_UI_Clear();
}
