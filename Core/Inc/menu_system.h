#ifndef __MENU_SYSTEM_H
#define __MENU_SYSTEM_H

#include "main.h"
#include "button.h"

// 菜单ID定义
typedef enum {
    MENU_MAIN = 0,          // 主菜单
    MENU_HOME,              // 主页（传感器概览）
    MENU_SENSOR_DETAIL,     // 传感器详情
    MENU_DOOR_CONTROL,      // 门控制
    MENU_SETTINGS,          // 设置
    MENU_ABOUT,             // 关于
    MENU_MAX
} MenuID_t;

// 菜单项结构
typedef struct MenuItem {
    const char *name;           // 显示名称
    MenuID_t    next_menu;      // 进入的子菜单
    void      (*action)(void);  // 执行的动作
    uint8_t     is_submenu;     // 是否有子菜单
} MenuItem_t;

// 菜单页面结构
typedef struct MenuPage {
    MenuID_t      id;
    const char   *title;
    MenuItem_t   *items;
    uint8_t       item_count;
    uint8_t       selected;     // 当前选中项
    uint8_t       scroll_offset;// 滚动偏移
    MenuID_t      parent;       // 父菜单
} MenuPage_t;

// 菜单系统初始化
void Menu_Init(void);

// 处理按键事件
void Menu_HandleKey(KeyEvent_t event);

// 刷新显示
void Menu_RefreshDisplay(void);

// 获取当前菜单ID
MenuID_t Menu_GetCurrent(void);

// 设置指定菜单
void Menu_SetMenu(MenuID_t menu);

// 返回上级菜单
void Menu_GoBack(void);

// 强制刷新显示
void Menu_ForceRefresh(void);

#endif
