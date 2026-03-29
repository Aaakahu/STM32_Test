/**
  ******************************************************************************
  * @file           : dht11.c
  * @brief          : DHT11温湿度传感器驱动 - 修复校验和错误版本
  * @note           : 针对 8MHz HSI 时钟优化时序
  ******************************************************************************
  */

#include "dht11.h"
#include "main.h"

/*=============================================================================
 * 宏定义 - 引脚操作
 *============================================================================*/
#define DHT11_SET_OUTPUT()  { \
    GPIO_InitTypeDef GPIO_InitStruct = {0}; \
    GPIO_InitStruct.Pin = DHT11_PIN; \
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; \
    GPIO_InitStruct.Pull = GPIO_NOPULL; \
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; \
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct); \
}

#define DHT11_SET_INPUT()   { \
    GPIO_InitTypeDef GPIO_InitStruct = {0}; \
    GPIO_InitStruct.Pin = DHT11_PIN; \
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; \
    GPIO_InitStruct.Pull = GPIO_NOPULL; \
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; \
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct); \
}

#define DHT11_WRITE_HIGH()  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET)
#define DHT11_WRITE_LOW()   HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET)
#define DHT11_READ()        HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)

/*=============================================================================
 * 延时函数 - 针对 8MHz 校准
 *============================================================================*/

/**
  * @brief  毫秒级延时
  */
static void DHT11_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

/**
  * @brief  微秒级延时 - 使用 DWT 或简单循环
  * @note   在 8MHz 下，1 个时钟周期 = 125ns
  *         延时循环大约需要 8-10 个周期
  */
static void DHT11_DelayUs(uint32_t us)
{
    // 对于 8MHz，更精确的延时
    // 内层循环大约 8-10 个时钟周期
    volatile uint32_t count = us * 1;  // 调整因子，实际需要测试
    while(count--)
    {
        __asm volatile ("nop");
    }
}

/**
  * @brief  更长的微秒延时（用于 50-80us 的等待）
  */
static void DHT11_DelayUsLong(uint32_t us)
{
    volatile uint32_t count = us * 1;
    while(count--)
    {
        __asm volatile ("nop\n\tnop\n\tnop\n\tnop");
    }
}

/*=============================================================================
 * DHT11 初始化
 *============================================================================*/

void DHT11_Init(void)
{
    // 使能 GPIO 时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // 配置为开漏输出
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
    
    // 初始状态置高
    DHT11_WRITE_HIGH();
    
    // DHT11 需要至少 1 秒上电稳定
    DHT11_DelayMs(1200);
}

/*=============================================================================
 * DHT11 数据读取 - 修复版本
 *============================================================================*/

/**
  * @brief  读取一位数据
  * @retval 0 或 1
  * @note   DHT11 位编码：
  *         - 每个位以 50us 低电平开始
  *         - '0': 26-28us 高电平（总共约 76-78us）
  *         - '1': 70us 高电平（总共约 120us）
  *         
  *         判断方法：延时 40us 后检测电平
  *         - 如果为低，则是 '0'
  *         - 如果为高，则是 '1'
  */
static uint8_t DHT11_ReadBit(void)
{
    uint16_t timeout;
    
    // 等待低电平结束（起始信号，约 50us）
    timeout = 1000;
    while(DHT11_READ() == GPIO_PIN_RESET && timeout--);
    if(timeout == 0) return 0xFF;  // 错误
    
    // 延时 40us
    DHT11_DelayUs(40);
    
    // 读取电平状态
    uint8_t bit = (DHT11_READ() == GPIO_PIN_SET) ? 1 : 0;
    
    // 等待高电平结束（准备接收下一位）
    timeout = 1000;
    while(DHT11_READ() == GPIO_PIN_SET && timeout--);
    
    return bit;
}

/**
  * @brief  读取一个字节（8位，高位先发）
  */
static uint8_t DHT11_ReadByte(void)
{
    uint8_t byte = 0;
    for(uint8_t i = 0; i < 8; i++)
    {
        byte <<= 1;
        uint8_t bit = DHT11_ReadBit();
        if(bit == 0xFF) return 0xFF;  // 错误
        byte |= bit;
    }
    return byte;
}

/**
  * @brief  读取DHT11数据 - 使用位读取方式
  */
uint8_t DHT11_Read_Data(DHT11_Data_t *data)
{
    uint8_t buf[5] = {0};
    uint16_t timeout;
    
    if(data == NULL) 
        return DHT11_ERROR_NULL;

    //=========================================================================
    // 步骤1: 发送起始信号（拉低18ms以上）
    //=========================================================================
    DHT11_SET_OUTPUT();
    DHT11_WRITE_LOW();
    DHT11_DelayMs(20);  // 拉低 20ms
    
    //=========================================================================
    // 步骤2: 拉高 20-40us，然后释放总线
    //=========================================================================
    DHT11_WRITE_HIGH();
    DHT11_DelayUs(35);   // 延时 35us
    
    //=========================================================================
    // 步骤3: 切换到输入模式
    //=========================================================================
    DHT11_SET_INPUT();
    
    //=========================================================================
    // 步骤4: 等待DHT11拉低响应（80us）
    //=========================================================================
    timeout = 1000;
    while(DHT11_READ() == GPIO_PIN_SET && timeout--);
    if(timeout == 0) return DHT11_ERROR_NO_RESPONSE;
    
    //=========================================================================
    // 步骤5: 等待DHT11拉高（80us）
    //=========================================================================
    timeout = 1000;
    while(DHT11_READ() == GPIO_PIN_RESET && timeout--);
    if(timeout == 0) return DHT11_ERROR_TIMEOUT;
    
    //=========================================================================
    // 步骤6: 等待拉高结束，准备接收数据
    //=========================================================================
    timeout = 1000;
    while(DHT11_READ() == GPIO_PIN_SET && timeout--);
    if(timeout == 0) return DHT11_ERROR_TIMEOUT;
    
    //=========================================================================
    // 步骤7: 读取40位数据（5字节）
    //=========================================================================
    // 关闭中断，防止时序被干扰
    __disable_irq();
    
    for(uint8_t i = 0; i < 5; i++) 
    {
        buf[i] = DHT11_ReadByte();
        if(buf[i] == 0xFF) {
            __enable_irq();
            DHT11_SET_OUTPUT();
            DHT11_WRITE_HIGH();
            return DHT11_ERROR_TIMEOUT;
        }
    }
    
    // 恢复中断
    __enable_irq();
    
    //=========================================================================
    // 步骤8: 校验数据
    //=========================================================================
    uint8_t checksum = buf[0] + buf[1] + buf[2] + buf[3];
    if(checksum != buf[4]) 
    {
        // 保存原始数据供调试
        data->humidity_int = buf[0];
        data->humidity_dec = buf[1];
        data->temperature_int = buf[2];
        data->temperature_dec = buf[3];
        data->checksum = buf[4];
        
        DHT11_SET_OUTPUT();
        DHT11_WRITE_HIGH();
        return DHT11_ERROR_CHECKSUM;
    }
    
    //=========================================================================
    // 步骤9: 填充数据结构
    //=========================================================================
    data->humidity_int = buf[0];
    data->humidity_dec = buf[1];
    data->temperature_int = buf[2];
    data->temperature_dec = buf[3];
    data->checksum = buf[4];
    
    // DHT11 小数部分通常为 0，如果有小数需要特殊处理
    data->humidity = (float)buf[0];
    data->temperature = (float)buf[2];
    
    // 恢复输出模式
    DHT11_SET_OUTPUT();
    DHT11_WRITE_HIGH();
    
    return DHT11_OK;
}

/**
  * @brief  备用读取方法 - 使用 HAL_Delay 微秒版本
  * @note   如果主方法失败，可以尝试这个
  */
uint8_t DHT11_Read_Data_Alt(DHT11_Data_t *data)
{
    uint8_t buf[5] = {0};
    uint16_t timeout;
    
    if(data == NULL) 
        return DHT11_ERROR_NULL;

    // 起始信号
    DHT11_SET_OUTPUT();
    DHT11_WRITE_LOW();
    HAL_Delay(20);
    DHT11_WRITE_HIGH();
    
    // 短暂延时
    for(volatile int i = 0; i < 200; i++);
    
    // 切换输入
    DHT11_SET_INPUT();
    
    // 等待响应
    timeout = 10000;
    while(DHT11_READ() && timeout--);
    if(timeout == 0) return DHT11_ERROR_NO_RESPONSE;
    
    timeout = 10000;
    while(!DHT11_READ() && timeout--);
    if(timeout == 0) return DHT11_ERROR_TIMEOUT;
    
    timeout = 10000;
    while(DHT11_READ() && timeout--);
    if(timeout == 0) return DHT11_ERROR_TIMEOUT;
    
    // 读取数据 - 使用 HAL_Delay 的简化方法
    __disable_irq();
    
    for(uint8_t i = 0; i < 5; i++)
    {
        buf[i] = 0;
        for(uint8_t j = 0; j < 8; j++)
        {
            buf[i] <<= 1;
            
            // 等待低电平结束
            timeout = 10000;
            while(!DHT11_READ() && timeout--);
            
            // 延时约 50us
            for(volatile int k = 0; k < 100; k++);
            
            // 读取位
            if(DHT11_READ())
            {
                buf[i] |= 1;
                // 等待高电平结束
                timeout = 10000;
                while(DHT11_READ() && timeout--);
            }
        }
    }
    
    __enable_irq();
    
    // 校验
    if((buf[0] + buf[1] + buf[2] + buf[3]) != buf[4])
    {
        data->humidity_int = buf[0];
        data->humidity_dec = buf[1];
        data->temperature_int = buf[2];
        data->temperature_dec = buf[3];
        data->checksum = buf[4];
        
        DHT11_SET_OUTPUT();
        DHT11_WRITE_HIGH();
        return DHT11_ERROR_CHECKSUM;
    }
    
    data->humidity_int = buf[0];
    data->humidity_dec = buf[1];
    data->temperature_int = buf[2];
    data->temperature_dec = buf[3];
    data->humidity = buf[0];
    data->temperature = buf[2];
    
    DHT11_SET_OUTPUT();
    DHT11_WRITE_HIGH();
    
    return DHT11_OK;
}
