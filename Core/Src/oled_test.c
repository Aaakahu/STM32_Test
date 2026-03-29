#include "oled_test.h"
#include "oled_ui.h"
#include "ssd1306.h"
#include <string.h>

// 测试基本字符显示
void OLED_Test_Characters(void) {
    OLED_UI_Clear();
    
    // 测试1: 显示简单的ASCII字符
    OLED_UI_ClearArea(AREA_STATUS);
    OLED_UI_ShowInArea(AREA_STATUS, 0, 0, "ABCD");
    OLED_UI_DrawArea(AREA_STATUS);
    
    HAL_Delay(2000);
    
    // 测试2: 显示数字
    OLED_UI_ClearArea(AREA_CONTENT);
    OLED_UI_ShowInArea(AREA_CONTENT, 0, 0, "1234");
    OLED_UI_ShowInArea(AREA_CONTENT, 0, 8, "5678");
    OLED_UI_DrawArea(AREA_CONTENT);
    
    HAL_Delay(2000);
    
    // 测试3: 显示菜单文字
    OLED_UI_ClearArea(AREA_BOTTOM);
    OLED_UI_ShowInArea(AREA_BOTTOM, 0, 0, "MENU");
    OLED_UI_DrawArea(AREA_BOTTOM);
    
    HAL_Delay(2000);
}

// 测试像素模式
void OLED_Test_Pattern(void) {
    OLED_UI_Clear();
    
    // 绘制水平线测试
    for (uint8_t y = 0; y < 32; y += 8) {
        for (uint8_t x = 0; x < 128; x++) {
            // 每8行一条线
            if (x % 16 == 0) {
                uint8_t page = y / 8;
                if (page < 4) {
                    // 直接操作显存
                    extern uint8_t OLED_Buffer[4][128];
                    OLED_Buffer[page][x] = 0xFF;
                }
            }
        }
    }
    
    // 刷新显示
    for (uint8_t page = 0; page < 4; page++) {
        OLED_Set_Pos(0, page);
        extern uint8_t OLED_Buffer[4][128];
        for (uint8_t i = 0; i < 128; i++) {
            OLED_Write_Byte(OLED_Buffer[page][i], OLED_DATA);
        }
    }
    
    HAL_Delay(3000);
}
