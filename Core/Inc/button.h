#ifndef __BUTTON_H
#define __BUTTON_H

#include "main.h"

// 按键引脚定义
#define BUTTON_OK_PORT      GPIOB
#define BUTTON_OK_PIN       GPIO_PIN_1

#define BUTTON_UP_PORT      GPIOA
#define BUTTON_UP_PIN       GPIO_PIN_0

// 按键状态
#define BTN_NONE            0
#define BTN_OK_PRESS        1   // PB1 - 确认/进入/选择
#define BTN_UP_PRESS        2   // PA0 - 上/返回
#define BTN_OK_LONG         3   // 长按 - 特殊功能
#define BTN_UP_LONG         4   // 长按 - 返回主菜单

// 按键事件类型
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_OK_SHORT,     // OK短按
    KEY_EVENT_OK_LONG,      // OK长按
    KEY_EVENT_UP_SHORT,     // UP短按
    KEY_EVENT_UP_LONG,      // UP长按
    KEY_EVENT_BOTH          // 双键同时按
} KeyEvent_t;

// 初始化
void Button_Init(void);

// 扫描按键（在main循环中调用）
void Button_Scan(void);

// 获取当前按键事件（读取后自动清除）
KeyEvent_t Button_GetEvent(void);

// 检查是否有按键按下（非阻塞）
uint8_t Button_IsPressed(void);

// 获取按键状态（供旧代码兼容）
uint8_t Button_IsPressedEdge(void);

#endif
