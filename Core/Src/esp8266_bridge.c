/*
 * 智能车库 - STM32 串口通信实现
 * 功能：处理 ESP8266 通信，上报传感器数据
 * 文件：esp8266_bridge.c
 */

#include "esp8266_bridge.h"
#include "main.h"
#include "dht11.h"
#include "mq2.h"
#include "hcsr04.h"
#include "servo.h"

// ==================== 全局变量 ====================

// 接收缓冲区
static volatile uint8_t esp_rx_buffer[ESP_RX_BUFFER_SIZE];
static volatile uint16_t esp_rx_index = 0;
static volatile uint8_t esp_rx_complete = 0;

// 命令标志
volatile uint8_t esp_command_received = 0;
volatile char esp_command_buffer[ESP_RX_BUFFER_SIZE];

// 定时上报
static uint32_t esp_last_report_time = 0;

// 上一次的门状态（用于检测变化）
static uint8_t last_door_status = 0;

// ==================== 初始化 ====================

void ESP_Init(void) {
    esp_rx_index = 0;
    esp_rx_complete = 0;
    esp_command_received = 0;
    
    // 发送启动消息
    ESP_SendString("STM32_READY\n");
}

// ==================== 串口接收处理 ====================

// 在 USART2_IRQHandler 中调用此函数
void ESP_ReceiveByte(uint8_t received_byte) {
    // 检测结束符（换行）
    if (received_byte == '\n' || received_byte == '\r') {
        if (esp_rx_index > 0) {
            esp_rx_buffer[esp_rx_index] = '\0';  // 字符串结束符
            
            // 复制到命令缓冲区
            memcpy((char*)esp_command_buffer, (char*)esp_rx_buffer, esp_rx_index + 1);
            esp_command_received = 1;
            
            esp_rx_index = 0;
        }
    } else {
        // 存入缓冲区
        esp_rx_buffer[esp_rx_index++] = received_byte;
        
        // 防止溢出
        if (esp_rx_index >= ESP_RX_BUFFER_SIZE - 1) {
            esp_rx_index = 0;
        }
    }
}

// ==================== 处理接收到的命令 ====================

void ESP_ProcessCommand(void) {
    if (!esp_command_received) return;
    
    esp_command_received = 0;
    
    char* cmd = (char*)esp_command_buffer;
    
    // 打印调试信息（如果有 OLED 或串口调试）
    // OLED_ShowString(0, 6, cmd);
    
    // 解析命令格式: CMD:DOOR:OPEN 或 CMD:DOOR:CLOSE
    if (strncmp(cmd, "CMD:DOOR:", 9) == 0) {
        char* action = cmd + 9;
        
        if (strcmp(action, "OPEN") == 0) {
            // 调用你的开门函数
            Servo_SetAngle(90);  // 舵机转到 90 度（开门）
            door_open_flag = 1;
            ESP_SendString("ACK:DOOR:OPENED\n");
            
        } else if (strcmp(action, "CLOSE") == 0) {
            // 调用你的关门函数
            Servo_SetAngle(0);   // 舵机转到 0 度（关门）
            door_open_flag = 0;
            ESP_SendString("ACK:DOOR:CLOSED\n");
        }
    }
    // 可以扩展其他命令...
}

// ==================== 上报传感器数据 ====================

void ESP_ReportData(void) {
    uint32_t current_time = HAL_GetTick();
    
    // 定时上报
    if (current_time - esp_last_report_time < ESP_REPORT_INTERVAL) {
        return;
    }
    esp_last_report_time = current_time;
    
    // 门状态变化时立即上报
    uint8_t current_door = door_open_flag;
    uint8_t door_changed = (current_door != last_door_status);
    last_door_status = current_door;
    
    // 使用简易格式发送: DATA:温度,湿度,烟雾等级,PPM,距离,门状态
    ESP_SendDataSimple();
}

// ==================== 发送数据（简易格式）====================

void ESP_SendDataSimple(void) {
    char buffer[128];
    
    // 格式化: DATA:25.5,60,1,200,45,OPEN
    snprintf(buffer, sizeof(buffer), 
             "DATA:%.1f,%.1f,%d,%d,%d,%s\n",
             DHT11_Temp,
             DHT11_Humi,
             smoke_level,
             smoke_ppm,
             (distance_cm < 400) ? distance_cm : 999,  // 超出范围显示 999
             door_open_flag ? "OPEN" : "CLOSED");
    
    ESP_SendString(buffer);
}

// ==================== 发送数据（JSON 格式）====================

void ESP_SendDataJSON(void) {
    char buffer[256];
    
    // 格式化 JSON
    snprintf(buffer, sizeof(buffer),
             "{\"temp\":%.1f,\"hum\":%.1f,\"smoke\":%d,\"ppm\":%d,\"dist\":%d,\"door\":\"%s\"}\n",
             DHT11_Temp,
             DHT11_Humi,
             smoke_level,
             smoke_ppm,
             (distance_cm < 400) ? distance_cm : 999,
             door_open_flag ? "OPEN" : "CLOSED");
    
    ESP_SendString(buffer);
}

// ==================== 发送报警信息 ====================

void ESP_SendAlarm(const char* alarmType) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "ALARM:%s\n", alarmType);
    ESP_SendString(buffer);
}

// ==================== 串口发送辅助函数 ====================

// 外部声明的 UART2 发送函数（在 main.c 中定义）
extern void UART2_SendByte(uint8_t byte);

void ESP_SendString(const char* str) {
    while (*str) {
        UART2_SendByte(*str++);
    }
}

// ==================== 集成到你的主循环 ====================

/*
 * 在你的 main.c 的 while(1) 循环中添加：
 * 
 * // ESP8266 通信处理
 * ESP_ProcessCommand();  // 处理接收到的命令
 * ESP_ReportData();      // 定时上报数据
 * 
 * // 报警检测（在你的报警逻辑中添加）
 * if (temperature > 30.0f) {
 *     ESP_SendAlarm("TEMP_HIGH");
 * }
 * if (smoke_level >= 3) {
 *     ESP_SendAlarm("SMOKE_HIGH");
 * }
 * if (distance_cm <= 5) {
 *     ESP_SendAlarm("TOO_CLOSE");
 * }
 */