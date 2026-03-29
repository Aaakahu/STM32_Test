#include "menu_system.h"
#include "oled_ui.h"
#include "dht11.h"
#include "mq2.h"
#include "servo.h"
#include <string.h>
#include <stdio.h>

// MENU_NONE 定义
#define MENU_NONE   0xFF

// 外部数据引用
extern uint8_t g_door_open;
extern uint8_t g_alarm_active;
extern DHT11_Data_t dht11_data;
extern uint16_t smoke_adc;
extern uint8_t smoke_level;
extern uint16_t smoke_ppm;

// 函数声明
void action_open_door(void);
void action_close_door(void);
void action_toggle_door(void);
void action_calibrate_mq2(void);

//=================== 菜单项定义 ===================

// 主菜单项
MenuItem_t main_menu_items[] = {
    {"Home",        MENU_HOME,          NULL, 1},
    {"Sensors",     MENU_SENSOR_DETAIL, NULL, 1},
    {"Door",        MENU_DOOR_CONTROL,  NULL, 1},
    {"Settings",    MENU_SETTINGS,      NULL, 1},
    {"About",       MENU_ABOUT,         NULL, 1},
};

// 门控制菜单项
MenuItem_t door_menu_items[] = {
    {"Toggle Door", MENU_NONE, action_toggle_door, 0},
    {"Open Door",   MENU_NONE, action_open_door,   0},
    {"Close Door",  MENU_NONE, action_close_door,  0},
};

// 设置菜单项
MenuItem_t settings_menu_items[] = {
    {"Calibrate MQ2", MENU_NONE, action_calibrate_mq2, 0},
    {"Backlight",     MENU_NONE, NULL, 0},
    {"Contrast",      MENU_NONE, NULL, 0},
};

// 关于菜单项
MenuItem_t about_menu_items[] = {
    {"Smart Home",    MENU_NONE, NULL, 0},
    {"Ver: 1.0",      MENU_NONE, NULL, 0},
    {"STM32F103C8",   MENU_NONE, NULL, 0},
    {"By: Claude",    MENU_NONE, NULL, 0},
};

//=================== 菜单页面定义 ===================

MenuPage_t menu_pages[MENU_MAX] = {
    // MENU_MAIN
    {
        .id = MENU_MAIN,
        .title = "Main Menu",
        .items = main_menu_items,
        .item_count = sizeof(main_menu_items) / sizeof(MenuItem_t),
        .selected = 0,
        .scroll_offset = 0,
        .parent = MENU_HOME
    },
    // MENU_HOME
    {
        .id = MENU_HOME,
        .title = "Home",
        .items = NULL,      // 特殊处理，显示传感器概览
        .item_count = 0,
        .selected = 0,
        .scroll_offset = 0,
        .parent = MENU_MAIN
    },
    // MENU_SENSOR_DETAIL
    {
        .id = MENU_SENSOR_DETAIL,
        .title = "Sensors",
        .items = NULL,      // 特殊处理，显示传感器详情
        .item_count = 0,
        .selected = 0,
        .scroll_offset = 0,
        .parent = MENU_MAIN
    },
    // MENU_DOOR_CONTROL
    {
        .id = MENU_DOOR_CONTROL,
        .title = "Door Control",
        .items = door_menu_items,
        .item_count = sizeof(door_menu_items) / sizeof(MenuItem_t),
        .selected = 0,
        .scroll_offset = 0,
        .parent = MENU_MAIN
    },
    // MENU_SETTINGS
    {
        .id = MENU_SETTINGS,
        .title = "Settings",
        .items = settings_menu_items,
        .item_count = sizeof(settings_menu_items) / sizeof(MenuItem_t),
        .selected = 0,
        .scroll_offset = 0,
        .parent = MENU_MAIN
    },
    // MENU_ABOUT
    {
        .id = MENU_ABOUT,
        .title = "About",
        .items = about_menu_items,
        .item_count = sizeof(about_menu_items) / sizeof(MenuItem_t),
        .selected = 0,
        .scroll_offset = 0,
        .parent = MENU_MAIN
    }
};

static MenuPage_t *current_menu = &menu_pages[MENU_HOME];
static uint8_t menu_refresh_needed = 1;

//=================== 动作函数 ===================

void action_open_door(void) {
    extern void Servo_Open(void);
    g_door_open = 1;
    Servo_Open();
}

void action_close_door(void) {
    extern void Servo_Close(void);
    g_door_open = 0;
    Servo_Close();
}

void action_toggle_door(void) {
    g_door_open = !g_door_open;
    if (g_door_open) action_open_door();
    else action_close_door();
}

void action_calibrate_mq2(void) {
    extern void MQ2_Calibrate(void);
    MQ2_Calibrate();
}

//=================== 菜单核心函数 ===================

void Menu_Init(void) {
    current_menu = &menu_pages[MENU_HOME];
    menu_refresh_needed = 1;
}

MenuID_t Menu_GetCurrent(void) {
    return current_menu->id;
}

void Menu_SetMenu(MenuID_t menu_id) {
    if (menu_id < MENU_MAX) {
        current_menu = &menu_pages[menu_id];
        menu_refresh_needed = 1;
    }
}

void Menu_GoBack(void) {
    if (current_menu->parent < MENU_MAX) {
        current_menu = &menu_pages[current_menu->parent];
        menu_refresh_needed = 1;
    }
}

//=================== 按键处理 ===================

void Menu_HandleKey(KeyEvent_t event) {
    switch (event) {
        case KEY_EVENT_OK_SHORT:
            if (current_menu->items != NULL && current_menu->item_count > 0) {
                MenuItem_t *item = &current_menu->items[current_menu->selected];
                
                if (item->is_submenu && item->next_menu < MENU_MAX) {
                    // 进入子菜单
                    Menu_SetMenu(item->next_menu);
                } else if (item->action != NULL) {
                    // 执行动作
                    item->action();
                    menu_refresh_needed = 1;
                }
            } else {
                // 特殊页面（Home/Sensor）按OK进入主菜单
                Menu_SetMenu(MENU_MAIN);
            }
            break;
            
        case KEY_EVENT_OK_LONG:
            // 长按OK返回上级
            Menu_GoBack();
            break;
            
        case KEY_EVENT_UP_SHORT:
            if (current_menu->items != NULL && current_menu->item_count > 0) {
                // 向上移动选择
                if (current_menu->selected > 0) {
                    current_menu->selected--;
                    menu_refresh_needed = 1;
                }
            }
            break;
            
        case KEY_EVENT_UP_LONG:
            // 长按UP直接返回主页
            Menu_SetMenu(MENU_HOME);
            break;
            
        case KEY_EVENT_BOTH:
            // 双键重置
            Menu_SetMenu(MENU_HOME);
            break;
            
        default:
            break;
    }
}

//=================== 显示函数 ===================

// 显示列表菜单
static void draw_list_menu(void) {
    // 顶部标题栏
    OLED_UI_ClearArea(AREA_STATUS);
    OLED_UI_PrintfInArea(AREA_STATUS, 0, 0, "<%s>", current_menu->title);
    OLED_UI_DrawArea(AREA_STATUS);
    
    // 中间内容区 - 显示菜单项
    OLED_UI_ClearArea(AREA_CONTENT);
    
    uint8_t y = 0;
    uint8_t max_display = 2;  // 128x32可以显示2个菜单项
    
    // 计算显示范围
    uint8_t start = 0;
    if (current_menu->selected >= max_display) {
        start = current_menu->selected - max_display + 1;
    }
    
    for (uint8_t i = 0; i < max_display && (start + i) < current_menu->item_count; i++) {
        uint8_t idx = start + i;
        MenuItem_t *item = &current_menu->items[idx];
        
        char buf[32];
        buf[0] = '\0';
        
        // 门控制页面第一项显示当前状态
        if (current_menu->id == MENU_DOOR_CONTROL && idx == 0 && idx == current_menu->selected) {
            snprintf(buf, sizeof(buf), ">%s[%c]", item->name, 
                     g_door_open ? 'O' : 'C');
        } else if (idx == current_menu->selected) {
            // 选中项用 > 标记
            snprintf(buf, sizeof(buf), ">%s", item->name);
        } else {
            snprintf(buf, sizeof(buf), " %s", item->name);
        }
        
        OLED_UI_ShowInArea(AREA_CONTENT, 0, 0, buf);  // 强制Y=0，页对齐
    }
    
    // 显示页码
    if (current_menu->item_count > max_display) {
        OLED_UI_PrintfInArea(AREA_CONTENT, 100, 0, "%d/%d", 
                            current_menu->selected + 1, 
                            current_menu->item_count);
    }
    
    OLED_UI_DrawArea(AREA_CONTENT);
    
    // 底部提示
    OLED_UI_ClearArea(AREA_BOTTOM);
    OLED_UI_ShowInArea(AREA_BOTTOM, 0, 0, "UP:Up OK:Sel/Enter");
    OLED_UI_DrawArea(AREA_BOTTOM);
}

// 显示主页（传感器概览）
static void draw_home_page(void) {
    // 顶部 - 状态栏
    OLED_UI_ClearArea(AREA_STATUS);
    OLED_UI_PrintfInArea(AREA_STATUS, 0, 0, "T:%d H:%d", 
                         dht11_data.temperature_int, 
                         dht11_data.humidity_int);
    
    // 显示门状态图标
    OLED_UI_ShowInArea(AREA_STATUS, 90, 0, g_door_open ? "[O]" : "[C]");
    
    // 报警指示
    if (g_alarm_active) {
        OLED_UI_ShowInArea(AREA_STATUS, 115, 0, "!");
    }
    OLED_UI_DrawArea(AREA_STATUS);
    
    // 中间 - 烟雾信息
    OLED_UI_ClearArea(AREA_CONTENT);
    
    const char* level_str;
    switch(smoke_level) {
        case 0: level_str = "CLEAN"; break;
        case 1: level_str = "LOW"; break;
        case 2: level_str = "MED"; break;
        case 3: level_str = "HIGH"; break;
        case 4: level_str = "!!!"; break;
        default: level_str = "?";
    }
    
    OLED_UI_PrintfInArea(AREA_CONTENT, 0, 0, "SMK:%s", level_str);
    OLED_UI_PrintfInArea(AREA_CONTENT, 80, 0, "ADC:%d", smoke_adc);
    OLED_UI_PrintfInArea(AREA_CONTENT, 0, 8, "PPM:%d", smoke_ppm);
    OLED_UI_ShowInArea(AREA_CONTENT, 70, 8, g_door_open ? "DOOR:OPEN" : "DOOR:CL");
    
    OLED_UI_DrawArea(AREA_CONTENT);
    
    // 底部 - 滚动提示
    static ScrollText_t scroll_info;
    static uint8_t scroll_init = 0;
    if (!scroll_init) {
        char info_str[64];
        snprintf(info_str, sizeof(info_str), 
                 "Smart Home Monitor - OK:Menu ");
        OLED_UI_ScrollText_Init(&scroll_info, info_str, 10);
        scroll_init = 1;
    }
    OLED_UI_ScrollText_Update(&scroll_info, AREA_BOTTOM, 0);
}

// 显示传感器详情
static void draw_sensor_detail(void) {
    // 顶部标题
    OLED_UI_ClearArea(AREA_STATUS);
    OLED_UI_ShowInArea(AREA_STATUS, 0, 0, "<Sensors>");
    OLED_UI_DrawArea(AREA_STATUS);
    
    // 中间 - 详细数据
    OLED_UI_ClearArea(AREA_CONTENT);
    OLED_UI_PrintfInArea(AREA_CONTENT, 0, 0, "Temp: %d C", dht11_data.temperature_int);
    OLED_UI_PrintfInArea(AREA_CONTENT, 0, 8, "Humi: %d %%", dht11_data.humidity_int);
    OLED_UI_DrawArea(AREA_CONTENT);
    
    // 底部
    OLED_UI_ClearArea(AREA_BOTTOM);
    OLED_UI_PrintfInArea(AREA_BOTTOM, 0, 0, "ADC:%d PPM:%d", smoke_adc, smoke_ppm);
    OLED_UI_DrawArea(AREA_BOTTOM);
}

//=================== 主刷新函数 ===================

void Menu_RefreshDisplay(void) {
    if (!menu_refresh_needed) return;
    
    switch (current_menu->id) {
        case MENU_HOME:
            draw_home_page();
            break;
            
        case MENU_SENSOR_DETAIL:
            draw_sensor_detail();
            break;
            
        case MENU_MAIN:
        case MENU_DOOR_CONTROL:
        case MENU_SETTINGS:
        case MENU_ABOUT:
        default:
            draw_list_menu();
            break;
    }
    
    menu_refresh_needed = 0;
}

// 强制刷新（供外部调用）
void Menu_ForceRefresh(void) {
    menu_refresh_needed = 1;
}
