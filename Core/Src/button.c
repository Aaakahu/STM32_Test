#include "button.h"

// 按键消抖和状态
#define DEBOUNCE_TIME       20      // 消抖时间 ms
#define LONG_PRESS_TIME     800     // 长按时间 ms
#define REPEAT_DELAY        300     // 重复触发延迟 ms
#define REPEAT_INTERVAL     150     // 重复触发间隔 ms

typedef struct {
    uint8_t cur_state;      // 当前物理状态
    uint8_t last_state;     // 上次物理状态
    uint8_t pressed;        // 已确认按下
    uint8_t long_pressed;   // 长按标志
    uint32_t press_time;    // 按下时间
    uint32_t last_repeat;   // 上次重复触发时间
} ButtonState_t;

static ButtonState_t btn_ok = {0};      // PB1
static ButtonState_t btn_up = {0};      // PA0
static volatile KeyEvent_t key_event = KEY_EVENT_NONE;
static uint32_t last_scan_time = 0;

// 读取GPIO状态 (0=按下，1=释放，因为内部上拉)
static uint8_t read_ok_pin(void) {
    return HAL_GPIO_ReadPin(BUTTON_OK_PORT, BUTTON_OK_PIN) == GPIO_PIN_RESET ? 1 : 0;
}

static uint8_t read_up_pin(void) {
    return HAL_GPIO_ReadPin(BUTTON_UP_PORT, BUTTON_UP_PIN) == GPIO_PIN_RESET ? 1 : 0;
}

void Button_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 使能时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    // PB1 - OK键 (输入，内部上拉)
    GPIO_InitStruct.Pin = BUTTON_OK_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUTTON_OK_PORT, &GPIO_InitStruct);
    
    // PA0 - UP键 (输入，内部上拉)
    GPIO_InitStruct.Pin = BUTTON_UP_PIN;
    HAL_GPIO_Init(BUTTON_UP_PORT, &GPIO_InitStruct);
    
    // 初始化状态
    btn_ok.last_state = read_ok_pin();
    btn_up.last_state = read_up_pin();
}

// 扫描单个按键
static void scan_button(ButtonState_t *btn, uint8_t (*read_pin)(void), 
                        KeyEvent_t short_event, KeyEvent_t long_event,
                        uint32_t current_time) {
    btn->cur_state = read_pin();
    
    // 检测下降沿（按下）
    if (btn->cur_state && !btn->last_state) {
        btn->press_time = current_time;
        btn->pressed = 0;
        btn->long_pressed = 0;
    }
    // 检测持续按下
    else if (btn->cur_state && btn->last_state) {
        uint32_t press_duration = current_time - btn->press_time;
        
        // 短按确认
        if (!btn->pressed && press_duration >= DEBOUNCE_TIME) {
            btn->pressed = 1;
        }
        
        // 长按检测
        if (!btn->long_pressed && press_duration >= LONG_PRESS_TIME) {
            btn->long_pressed = 1;
            key_event = long_event;
        }
    }
    // 检测上升沿（释放）
    else if (!btn->cur_state && btn->last_state) {
        uint32_t press_duration = current_time - btn->press_time;
        
        // 短按释放，且未达到长按
        if (btn->pressed && !btn->long_pressed && press_duration < LONG_PRESS_TIME) {
            key_event = short_event;
        }
        
        btn->pressed = 0;
        btn->long_pressed = 0;
    }
    
    btn->last_state = btn->cur_state;
}

void Button_Scan(void) {
    uint32_t current_time = HAL_GetTick();
    
    // 每10ms扫描一次
    if (current_time - last_scan_time < 10) return;
    last_scan_time = current_time;
    
    // 检查双键同时按下
    uint8_t ok_now = read_ok_pin();
    uint8_t up_now = read_up_pin();
    
    if (ok_now && up_now) {
        // 双键同时按下超过500ms，触发重置/特殊事件
        static uint32_t both_press_time = 0;
        if (btn_ok.last_state || btn_up.last_state) {
            both_press_time = current_time;
        }
        if (current_time - both_press_time >= 500) {
            key_event = KEY_EVENT_BOTH;
            both_press_time = current_time + 1000; // 防止连续触发
        }
    }
    
    // 扫描各个按键
    scan_button(&btn_ok, read_ok_pin, KEY_EVENT_OK_SHORT, KEY_EVENT_OK_LONG, current_time);
    scan_button(&btn_up, read_up_pin, KEY_EVENT_UP_SHORT, KEY_EVENT_UP_LONG, current_time);
}

KeyEvent_t Button_GetEvent(void) {
    KeyEvent_t event = key_event;
    key_event = KEY_EVENT_NONE;
    return event;
}

// 旧API兼容
uint8_t Button_IsPressed(void) {
    return read_ok_pin() || read_up_pin();
}

// 旧API兼容 - 检测OK键边沿
uint8_t Button_IsPressedEdge(void) {
    static uint8_t last = 0;
    uint8_t now = read_ok_pin();
    uint8_t edge = (now && !last);
    last = now;
    return edge;
}
