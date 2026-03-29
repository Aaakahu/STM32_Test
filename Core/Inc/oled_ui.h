#ifndef __OLED_UI_H
#define __OLED_UI_H

#include "main.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// 128x32 屏幕分区定义
#define OLED_WIDTH      128
#define OLED_HEIGHT     32
#define OLED_PAGES      4

// 区域ID定义 (将屏幕分成3个区域，避免重叠)
#define AREA_STATUS     0   // 顶部状态区 (Y: 0-7, 1行)
#define AREA_CONTENT    1   // 中间内容区 (Y: 8-23, 2行)
#define AREA_BOTTOM     2   // 底部信息区 (Y: 24-31, 1行)

// 字体大小
#define FONT_6X8        6
#define FONT_8X8        8

// 滚动方向
#define SCROLL_LEFT     0
#define SCROLL_RIGHT    1

// 区域结构体
typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
    uint8_t page_start;
    uint8_t page_end;
} OLED_Area_t;

// 滚动文本结构体
typedef struct {
    char text[64];          // 原文本
    char display[32];       // 显示缓冲区
    uint8_t text_len;       // 文本长度
    uint8_t offset;         // 当前滚动偏移
    uint8_t direction;      // 滚动方向
    uint8_t speed;          // 滚动速度(帧数)
    uint8_t counter;        // 滚动计数器
    uint8_t paused;         // 暂停标志
} ScrollText_t;

// 初始化
void OLED_UI_Init(void);
void OLED_UI_Clear(void);
void OLED_UI_ClearArea(uint8_t area_id);
void OLED_UI_Refresh(void);

// 分区显示
void OLED_UI_DrawArea(uint8_t area_id);
void OLED_UI_ShowInArea(uint8_t area_id, uint8_t x, uint8_t y, const char *str);
void OLED_UI_PrintfInArea(uint8_t area_id, uint8_t x, uint8_t y, const char *fmt, ...);

// 长文本滚动显示 (解决显示不清问题)
void OLED_UI_ScrollText_Init(ScrollText_t *scroll, const char *text, uint8_t speed);
void OLED_UI_ScrollText_Update(ScrollText_t *scroll, uint8_t area_id, uint8_t y);

// 图标显示
void OLED_UI_DrawIcon(uint8_t x, uint8_t y, const uint8_t *icon, uint8_t width, uint8_t height);
void OLED_UI_DrawBattery(uint8_t x, uint8_t y, uint8_t percent);
void OLED_UI_DrawSignal(uint8_t x, uint8_t y, uint8_t strength);

// 工具函数
uint8_t OLED_UI_GetStrWidth(const char *str);
void OLED_UI_DrawHLine(uint8_t x, uint8_t y, uint8_t len);
void OLED_UI_DrawVLine(uint8_t x, uint8_t y, uint8_t len);

// 区域定义 (外部可用)
extern const OLED_Area_t OLED_Areas[3];

#endif
