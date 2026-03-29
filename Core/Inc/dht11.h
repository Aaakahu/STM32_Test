/**
  ******************************************************************************
  * @file           : dht11.h
  * @brief          : DHT11温湿度传感器驱动头文件 - 诊断版本
  ******************************************************************************
  */

#ifndef __DHT11_H
#define __DHT11_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Exported defines ----------------------------------------------------------*/
// 引脚定义
#define DHT11_PIN       GPIO_PIN_4
#define DHT11_PORT      GPIOA

// 错误码定义
#define DHT11_OK                0   // 成功
#define DHT11_ERROR_CHECKSUM    1   // 校验和错误
#define DHT11_ERROR_TIMEOUT     2   // 超时错误
#define DHT11_ERROR_NO_RESPONSE 3   // DHT11 无响应
#define DHT11_ERROR_NULL        4   // 空指针错误

/* Exported types ------------------------------------------------------------*/
/**
  * @brief DHT11 数据结构
  */
typedef struct {
    uint8_t humidity_int;      // 湿度整数部分
    uint8_t humidity_dec;      // 湿度小数部分（DHT11通常固定为0）
    uint8_t temperature_int;   // 温度整数部分
    uint8_t temperature_dec;   // 温度小数部分（DHT11通常固定为0）
    uint8_t checksum;          // 校验和
    float humidity;            // 湿度值 (%)
    float temperature;         // 温度值 (℃)
} DHT11_Data_t;

/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  DHT11初始化
  * @note   配置GPIO并等待传感器上电稳定
  */
void DHT11_Init(void);

/**
  * @brief  读取DHT11数据
  * @param  data: 指向DHT11_Data_t结构体的指针
  * @retval 0: 成功
  *         1: 校验失败
  *         2: 超时
  *         3: 无响应
  *         4: 空指针
  * 
  * @note   读取间隔应 >= 1 秒
  */
uint8_t DHT11_Read_Data(DHT11_Data_t *data);

/**
  * @brief  快速读取DHT11（不校验数据）
  * @param  data: 指向DHT11_Data_t结构体的指针
  * @retval 同 DHT11_Read_Data
  * @note   用于调试，忽略校验和错误
  */
uint8_t DHT11_Read_Data_NoChecksum(DHT11_Data_t *data);
uint8_t DHT11_Read_Data_Alt(DHT11_Data_t *data);

/**
  * @brief  获取错误码描述字符串
  * @param  error_code: 错误码
  * @retval 错误描述字符串
  */
static inline const char* DHT11_GetErrorString(uint8_t error_code)
{
    switch(error_code)
    {
        case DHT11_OK:                return "OK";
        case DHT11_ERROR_CHECKSUM:    return "CHECKSUM";
        case DHT11_ERROR_TIMEOUT:     return "TIMEOUT";
        case DHT11_ERROR_NO_RESPONSE: return "NO RESP";
        case DHT11_ERROR_NULL:        return "NULL PTR";
        default:                      return "UNKNOWN";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* __DHT11_H */
