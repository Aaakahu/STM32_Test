#include "main.h"
#include <string.h>
#include "ssd1306.h"
#include "oled_font.h"
#include "dht11.h"
#include "buzzer_music.h"
#include "mq2.h"
#include "servo.h"
#include "hcsr04.h"
#include "esp8266_bridge.h"

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

// ESP8266 需要的全局变量
float DHT11_Temp = 0.0f, DHT11_Humi = 0.0f;
uint8_t smoke_level = 0;
uint16_t smoke_ppm = 0;
uint16_t distance_cm = 999;
uint8_t door_open_flag = 0;

#define LED_GREEN_PORT    GPIOA
#define LED_GREEN_PIN     GPIO_PIN_2
#define LED_RED_PORT      GPIOA
#define LED_RED_PIN       GPIO_PIN_3
#define LED_GREEN_ON()    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET)
#define LED_GREEN_OFF()   HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET)
#define LED_RED_ON()      HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET)
#define LED_RED_OFF()     HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET)

// 按键引脚直接定义
#define BT_OK_PORT    GPIOB
#define BT_OK_PIN     GPIO_PIN_1    // PB1
#define BT_UP_PORT    GPIOA
#define BT_UP_PIN     GPIO_PIN_0    // PA0

#define BT_OK_READ()  (HAL_GPIO_ReadPin(BT_OK_PORT, BT_OK_PIN) == GPIO_PIN_RESET)
#define BT_UP_READ()  (HAL_GPIO_ReadPin(BT_UP_PORT, BT_UP_PIN) == GPIO_PIN_RESET)

// 显示缓冲区
extern uint8_t OLED_GRAM[OLED_PAGE_NUM][OLED_PAGE_SIZE];

// 在指定位置显示字符 (6x8字体)
static void OLED_DrawChar(uint8_t x, uint8_t page, char c) {
    // 只支持可打印 ASCII (0x20-0x7F)
    if (c < ' ' || c > '~') c = '?';  // 不可显示字符显示为 '?'
    
    // 小写转大写（因为字体只有大写 A-Z）
    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    
    uint8_t idx = c - ' ';
    if (idx >= 96 || x > 122 || page > 3) return;
    for (uint8_t i = 0; i < 6; i++) {
        OLED_GRAM[page][x + i] = F6x8[idx][i];
    }
}

// 显示字符串
static void OLED_DrawString(uint8_t x, uint8_t page, const char *str) {
    while (*str && x <= 122) {
        OLED_DrawChar(x, page, *str++);
        x += 6;
    }
}

// 显示数字
static void OLED_DrawNum(uint8_t x, uint8_t page, int num, uint8_t digits) {
    char buf[8];
    for (int i = digits - 1; i >= 0; i--) {
        buf[i] = '0' + (num % 10);
        num /= 10;
    }
    buf[digits] = '\0';
    OLED_DrawString(x, page, buf);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    
    // 初始化各模块
    BuzzerMusic_Init();
    BuzzerMusic_Stop();
    OLED_Init();
    DHT11_Init();
    MQ2_Init();
    Servo_Init();
    HCSR04_Init();  // 初始化超声波测距模块
    
    // 初始化 ESP8266 通信 (USART2)
    MX_USART2_UART_Init();
    ESP_Init();
    
    // 初始化按键GPIO
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    GPIO_InitStruct.Pin = BT_OK_PIN;
    HAL_GPIO_Init(BT_OK_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = BT_UP_PIN;
    HAL_GPIO_Init(BT_UP_PORT, &GPIO_InitStruct);
    
    LED_GREEN_ON();
    LED_RED_OFF();
    
    //==== 开机画面 ====
    OLED_Clear();
    // 尝试在page 0, 1, 2都显示测试
    OLED_DrawString(28, 0, "Smart Home");
    OLED_DrawString(24, 1, "System Ready");
    OLED_DrawString(10, 2, "Loading...");
    OLED_Refresh();
    HAL_Delay(2000);  // 显示2秒确保看到
    
    // 舵机自检
    Servo_SetAngle(90); HAL_Delay(300);
    Servo_SetAngle(0);  HAL_Delay(300);
    Servo_SetAngle(180);HAL_Delay(300);
    Servo_SetAngle(90);
    
    // MQ-2预热
    HAL_Delay(1000);
    MQ2_Calibrate();
    StartupSound();
    
    // 状态变量
    DHT11_Data_t dht11_data;
    uint8_t dht11_ok = 0;
    uint16_t smoke_adc = 0;
    uint8_t smoke_level = 0;
    uint16_t smoke_ppm = 0;
    uint8_t alarm_active = 0;
    
    // 超声波测距变量
    uint16_t distance_cm = 999;
    uint8_t distance_alarm = 0;
    uint32_t last_distance_update = 0;
    
    // 红灯闪烁变量
    uint32_t last_led_toggle = 0;
    uint8_t led_red_state = 0;
    
    uint32_t last_sensor_update = 0;
    uint32_t last_display_update = 0;
    uint32_t last_alarm_time = 0;
    
    // 按键消抖变量
    uint8_t ok_last = 0, up_last = 0;
    uint32_t ok_press_time = 0, up_press_time = 0;
    
    while(1)
    {
        uint32_t now = HAL_GetTick();
        
        //==== 简单按键处理 ====
        uint8_t ok_now = BT_OK_READ();
        uint8_t up_now = BT_UP_READ();
        
        // PB1 检测 - 短按直接转到180度
        if (ok_now && !ok_last) {
            ok_press_time = now;
        }
        if (!ok_now && ok_last) {
            if (now - ok_press_time < 1000) {  // 短按
                Servo_SetAngle(180);  // PB1 = 180度
            }
        }
        ok_last = ok_now;
        
        // PA0 检测 - 短按转到0度
        if (up_now && !up_last) {
            up_press_time = now;
        }
        if (!up_now && up_last) {
            if (now - up_press_time < 1000) {  // 短按
                Servo_SetAngle(0);  // PA0 = 0度
            }
        }
        up_last = up_now;
        
        //==== 传感器读取 (每秒) ====
        if (now - last_sensor_update >= 1000) {
            last_sensor_update = now;
            dht11_ok = (DHT11_Read_Data(&dht11_data) == DHT11_OK);
            if (dht11_ok) {
                DHT11_Temp = dht11_data.temperature;
                DHT11_Humi = dht11_data.humidity;
            }
            smoke_adc = MQ2_ReadAnalog();
            smoke_level = MQ2_GetSmokeLevel(smoke_adc);
            smoke_ppm = MQ2_GetPPM(smoke_adc);
        }
        
        //==== 超声波测距 (每200ms) ====
        if (now - last_distance_update >= 200) {
            last_distance_update = now;
            distance_cm = HCSR04_GetDistance();
            // 更新全局变量供 ESP8266 上报
            extern uint16_t distance_cm;
            // 距离小于10cm触发报警，小于3cm强烈报警
            distance_alarm = (distance_cm <= 10 && distance_cm != 999);
        }
        
        //==== ESP8266 通信处理 ====
        ESP_ProcessCommand();  // 处理手机发来的控制命令
        ESP_ReportData();      // 定时上报传感器数据
        
        //==== 显示刷新 (每200ms) ====
        if (now - last_display_update >= 200) {
            last_display_update = now;
            
            // 报警检测
            uint8_t temp_alarm = dht11_ok && (dht11_data.temperature_int > 30);
            uint8_t humi_alarm = dht11_ok && (dht11_data.humidity_int > 80);
            uint8_t smoke_alarm = (smoke_level >= MQ2_LEVEL_HIGH);
            alarm_active = temp_alarm || humi_alarm || smoke_alarm;
            
            // 距离报警处理 - 距离越近声音越大越尖锐，红灯闪烁越快
            if (distance_alarm) {
                // 红灯闪烁频率与距离成反比
                // 距离越小，闪烁越快 (间隔越短)
                uint16_t led_interval;
                if (distance_cm <= 2) {
                    led_interval = 100;  // 2cm: 100ms闪烁一次 (最快)
                } else if (distance_cm <= 3) {
                    led_interval = 150;  // 3cm: 150ms闪烁
                } else if (distance_cm <= 4) {
                    led_interval = 200;  // 4cm: 200ms闪烁
                } else if (distance_cm <= 5) {
                    led_interval = 250;  // 5cm: 250ms闪烁
                } else {
                    led_interval = 300;  // >5cm: 300ms闪烁 (较慢)
                }
                
                // 红灯闪烁控制
                if (now - last_led_toggle >= led_interval) {
                    last_led_toggle = now;
                    led_red_state = !led_red_state;  // 翻转状态
                    if (led_red_state) {
                        LED_RED_ON();
                    } else {
                        LED_RED_OFF();
                    }
                }
                
                // 同时绿灯关闭
                LED_GREEN_OFF();
                
                // 使用动态报警：距离越近，频率越高、音量越大
                ProximityAlarm(distance_cm);
            } else if (alarm_active) {
                LED_GREEN_OFF();
                LED_RED_ON();
                // 停止距离报警音效，使用普通报警
                ProximityAlarm_Stop();
                if (now - last_alarm_time >= 2000) {
                    AlarmSound();
                    last_alarm_time = now;
                }
            } else {
                LED_GREEN_ON();
                LED_RED_OFF();
                led_red_state = 0;  // 重置红灯闪烁状态
                BuzzerMusic_Stop();
            }
            
            // 清屏并绘制
            OLED_Clear();
            
            if (dht11_ok) {
                // 第0行: 温湿度
                OLED_DrawString(0, 0, "T:");
                OLED_DrawNum(12, 0, dht11_data.temperature_int, 2);
                OLED_DrawString(24, 0, "C H:");
                OLED_DrawNum(48, 0, dht11_data.humidity_int, 2);
                OLED_DrawString(60, 0, "%");
                if (alarm_active) OLED_DrawString(120, 0, "!");
                
                // 第1行: 烟雾等级
                OLED_DrawString(0, 1, "SMOKE:");
                const char* lvl;
                switch(smoke_level) {
                    case 0: lvl = "CLEAN"; break;
                    case 1: lvl = "LOW  "; break;
                    case 2: lvl = "MED  "; break;
                    case 3: lvl = "HIGH "; break;
                    case 4: lvl = "!!!!!"; break;
                    default: lvl = "???  ";
                }
                OLED_DrawString(42, 1, lvl);
                OLED_DrawNum(90, 1, smoke_adc, 4);
                
                // 第2行: 距离和PPM
                OLED_DrawString(0, 2, "D:");
                if (distance_cm == 999) {
                    OLED_DrawString(12, 2, "---");
                } else {
                    OLED_DrawNum(12, 2, distance_cm, 3);
                }
                OLED_DrawString(30, 2, "CM PPM:");
                OLED_DrawNum(78, 2, smoke_ppm, 4);
                
                // 第3行: 状态
                if (distance_alarm) {
                    OLED_DrawString(0, 3, "TOO CLOSE!");
                    // 通过 ESP8266 发送报警
                    static uint32_t last_esp_alarm = 0;
                    if (now - last_esp_alarm > 5000) {  // 5秒发送一次，避免刷屏
                        ESP_SendAlarm("TOO_CLOSE");
                        last_esp_alarm = now;
                    }
                } else if (alarm_active) {
                    if (smoke_alarm) OLED_DrawString(0, 3, "SMOKE ALARM!");
                    else if (temp_alarm) OLED_DrawString(0, 3, "TEMP ALARM!");
                    else OLED_DrawString(0, 3, "HUMI ALARM!");
                } else {
                    OLED_DrawString(12, 3, "-- NORMAL --");
                }
            } else {
                OLED_DrawString(12, 1, "SENSOR ERROR");
                OLED_DrawString(0, 2, "Check DHT11");
            }
            
            OLED_Refresh();
        }
        
        HAL_Delay(10);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    
    if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }
    
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    
    if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Pin = LED_GREEN_PIN | LED_RED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
}

void Error_Handler(void)
{
    __disable_irq();
    while(1) {}
}

// ==================== ESP8266 通信接口 (USART2 寄存器方式) ====================

// USART2 寄存器初始化 (115200 baud @ 8MHz)
static void MX_USART2_UART_Init(void)
{
    // 使能 GPIOA 和 USART2 时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    
    // 配置 PA2 (TX) 和 PA3 (RX) 为复用推挽输出
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // 配置 USART2: 115200 baud, 8N1
    // 波特率计算: 8000000 / (16 * 115200) = 4.34
    // BRR = 4 (整数部分) + 5 (小数部分, 0.34*16≈5) = 0x45
    USART2->BRR = 0x45;  // 115200 @ 8MHz
    
    // 使能发送、接收和接收中断
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    
    // 使能 USART2
    USART2->CR1 |= USART_CR1_UE;
    
    // 配置 NVIC 中断
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

// UART 发送一个字节 (轮询方式)
void UART2_SendByte(uint8_t byte)
{
    while (!(USART2->SR & USART_SR_TXE));  // 等待发送缓冲区空
    USART2->DR = byte;
}

// UART 接收中断处理
void USART2_IRQHandler(void)
{
    if (USART2->SR & USART_SR_RXNE)  // 接收缓冲区非空
    {
        uint8_t rx_byte = (uint8_t)(USART2->DR & 0xFF);
        // 调用 ESP8266 处理函数
        extern void ESP_ReceiveByte(uint8_t byte);
        ESP_ReceiveByte(rx_byte);
    }
}
