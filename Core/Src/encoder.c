#include "encoder.h"

static uint8_t key_last_state = KEY_RELEASED;
static uint8_t key_press_flag = 0;

// 旋转检测变量
static uint8_t enc_a_last = 1;
static uint8_t enc_b_last = 1;
static int8_t enc_rotation_count = 0;
static uint32_t last_rotation_time = 0;

/**
 * @brief EC11编码器初始化
 * @note 初始化按键和旋转引脚
 */
void Encoder_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 按键引脚配置：输入模式，内部上拉
    GPIO_InitStruct.Pin = ENC_KEY_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(ENC_KEY_GPIO_PORT, &GPIO_InitStruct);
    
    // 旋转引脚配置：输入模式，内部上拉
    GPIO_InitStruct.Pin = ENC_A_GPIO_PIN | ENC_B_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // 初始化状态
    key_last_state = HAL_GPIO_ReadPin(ENC_KEY_GPIO_PORT, ENC_KEY_GPIO_PIN);
    enc_a_last = HAL_GPIO_ReadPin(ENC_A_GPIO_PORT, ENC_A_GPIO_PIN);
    enc_b_last = HAL_GPIO_ReadPin(ENC_B_GPIO_PORT, ENC_B_GPIO_PIN);
    enc_rotation_count = 0;
}

/**
 * @brief 读取按键当前状态
 * @return 0=按下, 1=释放
 */
uint8_t Encoder_KeyIsPressed(void)
{
    return (HAL_GPIO_ReadPin(ENC_KEY_GPIO_PORT, ENC_KEY_GPIO_PIN) == GPIO_PIN_RESET);
}

/**
 * @brief 检测按键按下边沿（带20ms消抖）
 * @return 1=检测到按下边沿, 0=无
 */
uint8_t Encoder_KeyIsPressedEdge(void)
{
    uint8_t current_state = HAL_GPIO_ReadPin(ENC_KEY_GPIO_PORT, ENC_KEY_GPIO_PIN);
    
    // 检测到下降沿（释放→按下）
    if (key_last_state == KEY_RELEASED && current_state == KEY_PRESSED)
    {
        HAL_Delay(20);  // 消抖延时
        
        // 再次确认
        if (HAL_GPIO_ReadPin(ENC_KEY_GPIO_PORT, ENC_KEY_GPIO_PIN) == KEY_PRESSED)
        {
            key_last_state = KEY_PRESSED;
            return 1;  // 确认按下
        }
    }
    // 检测到上升沿（按下→释放）
    else if (key_last_state == KEY_PRESSED && current_state == KEY_RELEASED)
    {
        HAL_Delay(20);  // 消抖延时
        key_last_state = KEY_RELEASED;
    }
    
    return 0;
}

/**
 * @brief 检测旋转方向和计数
 * @note 在每次主循环中调用，检测A、B相变化
 * @return 0=无旋转, +1=顺时针一步, -1=逆时针一步
 */
int8_t Encoder_GetRotation(void)
{
    uint8_t a_curr = HAL_GPIO_ReadPin(ENC_A_GPIO_PORT, ENC_A_GPIO_PIN);
    uint8_t b_curr = HAL_GPIO_ReadPin(ENC_B_GPIO_PORT, ENC_B_GPIO_PIN);
    int8_t result = 0;
    
    // 检测A相下降沿
    if (enc_a_last == 1 && a_curr == 0)
    {
        // A下降时，B为低则是顺时针，B为高则是逆时针
        if (b_curr == 0)
            result = 1;  // 顺时针
        else
            result = -1; // 逆时针
    }
    
    enc_a_last = a_curr;
    enc_b_last = b_curr;
    
    return result;
}

/**
 * @brief 带累积的旋转检测（用于替代按键）
 * @return 累计旋转步数，顺时针增加，逆时针减少
 */
int8_t Encoder_GetRotationAccumulated(void)
{
    int8_t rot = Encoder_GetRotation();
    if (rot != 0)
    {
        enc_rotation_count += rot;
        last_rotation_time = HAL_GetTick();
    }
    return enc_rotation_count;
}

/**
 * @brief 清除旋转计数
 */
void Encoder_ClearRotation(void)
{
    enc_rotation_count = 0;
}

/**
 * @brief 检测旋转动作（作为按键替代）
 * @return 1=检测到有意义的旋转动作, 0=无
 * @note 顺时针或逆时针旋转超过2步视为一次"确认"动作
 */
uint8_t Encoder_RotationAsKey(void)
{
    int8_t accum = Encoder_GetRotationAccumulated();
    
    // 顺时针或逆时针超过2步，视为一次按键动作
    if (accum >= 2 || accum <= -2)
    {
        Encoder_ClearRotation();
        return 1;
    }
    
    // 300ms无旋转，清零计数（防止累积）
    if (HAL_GetTick() - last_rotation_time > 300 && accum != 0)
    {
        Encoder_ClearRotation();
    }
    
    return 0;
}
