#include "buzzer_music.h"

// 使用 TIM3 硬件 PWM 产生方波，声音更纯净
// TIM3_CH3 -> PB0

// 小星星音乐数组 (频率, 持续时间ms) - 高音阶版本
// 旋律：1155665 4433221 (全部使用中高音，更清脆)
static const uint16_t startup_music[] = {
    // 第一乐句: 一闪一闪亮晶晶
    M1, 200,  // 一
    M1, 200,  // 闪
    M5, 200,  // 一
    M5, 200,  // 闪
    M6, 200,  // 亮
    M6, 200,  // 晶
    M5, 400,  // 晶 (长音)
    REST, 100,
    
    // 第二乐句: 满天都是小星星 (提升音阶，更响亮)
    H1, 200,  // 满 (提升至高音1)
    H1, 200,  // 天
    H7, 200,  // 都 (高音7)
    H7, 200,  // 是
    H6, 200,  // 小 (高音6)
    H6, 200,  // 星
    H5, 400,  // 星 (长音，高音5)
    REST, 200,
};

#define STARTUP_MUSIC_LEN (sizeof(startup_music) / sizeof(startup_music[0]) / 2)

/**
 * @brief 蜂鸣器PWM初始化
 * @note 使用TIM3_CH3 (PB0)
 */
void BuzzerMusic_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    
    // 配置PB0为复用推挽输出
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = BUZZER_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BUZZER_GPIO_PORT, &GPIO_InitStruct);
    
    // TIM3时基配置: 8MHz / (79+1) = 100kHz计数频率
    TIM3->PSC = 79;
    TIM3->ARR = 100 - 1;  // 默认1kHz
    TIM3->CCR3 = 50;      // 50%占空比
    
    // PWM Mode 1, 通道3
    TIM3->CCMR2 = (TIM3->CCMR2 & ~TIM_CCMR2_OC3M) | (0x06 << 4);  // PWM Mode 1
    TIM3->CCMR2 |= TIM_CCMR2_OC3PE;  // 预装载使能
    
    // 初始状态: 关闭输出
    TIM3->CCER &= ~TIM_CCER_CC3E;
    TIM3->CR1 |= TIM_CR1_CEN;  // 使能定时器
}

/**
 * @brief 设置蜂鸣器频率
 * @param freq 频率(Hz), 0表示静音
 */
void Buzzer_SetFrequency(uint16_t freq)
{
    if(freq == 0) {
        // 静音: 关闭PWM输出，设置GPIO为高电平
        TIM3->CCER &= ~TIM_CCER_CC3E;
        
        // 切回GPIO模式并置高
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = BUZZER_GPIO_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(BUZZER_GPIO_PORT, &GPIO_InitStruct);
        HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_SET);
        return;
    }
    
    // 重新配置为复用模式
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = BUZZER_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BUZZER_GPIO_PORT, &GPIO_InitStruct);
    
    // 计算ARR: Freq = 8MHz / 80 / (ARR+1) = 100kHz / (ARR+1)
    // ARR = 100000 / freq - 1
    uint16_t arr = (uint16_t)(100000UL / freq) - 1;
    if(arr > 65535) arr = 65535;
    if(arr < 10) arr = 10;  // 限制最高频率约10kHz
    
    TIM3->ARR = arr;
    TIM3->CCR3 = arr / 2;  // 50%占空比
    TIM3->CCER |= TIM_CCER_CC3E;  // 使能输出
}

/**
 * @brief 停止蜂鸣器
 */
void BuzzerMusic_Stop(void)
{
    Buzzer_SetFrequency(0);
}

/**
 * @brief 播放开机音乐 - 《起风了》前奏
 */
void StartupSound(void)
{
    uint16_t len = STARTUP_MUSIC_LEN;
    
    for(uint16_t i = 0; i < len; i++) {
        uint16_t freq = startup_music[i * 2];
        uint16_t duration = startup_music[i * 2 + 1];
        
        if(freq == REST) {
            Buzzer_SetFrequency(0);
        } else {
            Buzzer_SetFrequency(freq);
        }
        
        HAL_Delay(duration);
        
        // 音符间短暂停顿，使音乐更清晰
        Buzzer_SetFrequency(0);
        HAL_Delay(20);
    }
    
    Buzzer_SetFrequency(0);
}

/**
 * @brief 报警音效 - 高频交替
 */
void AlarmSound(void)
{
    for(int i = 0; i < 3; i++) {
        Buzzer_SetFrequency(H3);  // 高音
        HAL_Delay(150);
        Buzzer_SetFrequency(0);
        HAL_Delay(50);
        Buzzer_SetFrequency(H6);  // 更高
        HAL_Delay(150);
        Buzzer_SetFrequency(0);
        HAL_Delay(50);
    }
    Buzzer_SetFrequency(0);
}

/**
 * @brief 成功音效 - 上升音阶
 */
void SuccessSound(void)
{
    uint16_t notes[] = {M3, M5, H1, H3};
    for(int i = 0; i < 4; i++) {
        Buzzer_SetFrequency(notes[i]);
        HAL_Delay(150);
    }
    Buzzer_SetFrequency(0);
}

/**
 * @brief 设置蜂鸣器音量 (通过PWM占空比)
 * @param volume 音量 0-100 (0=静音, 100=最大)
 * @note 通过调整占空比改变音量，50%为正常音量
 */
void Buzzer_SetVolume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    
    // 占空比: 30%-80% 范围变化 (50%为正常)
    // 音量越大，占空比偏离50%越多 (音量实际是PWM的有效能量)
    uint16_t arr = TIM3->ARR;
    uint16_t ccr;
    
    if (volume == 0) {
        ccr = 0;  // 静音
    } else {
        // 音量映射到占空比: 音量100 -> 70%占空比, 音量0 -> 0%占空比
        ccr = (arr * volume) / 100;
        if (ccr == 0) ccr = 1;  // 最小1
    }
    
    TIM3->CCR3 = ccr;
}

/**
 * @brief 近距离报警 - "嘟嘟嘟"间歇式，距离越近节奏越快、音调越高
 * @param distance_cm 距离 (cm)
 * @note 距离 < 10cm 时触发"嘟嘟嘟"报警
 *       距离越近：嘟嘟嘟频率越快(间隔越短)、音调越高、音量越大
 *       5cm→2cm 区间变化明显，使用线性插值平滑过渡
 */
void ProximityAlarm(uint16_t distance_cm)
{
    static uint32_t last_beep_time = 0;
    static uint8_t beep_state = 0;  // 0=静音, 1=发声
    
    if (distance_cm > 10) {
        Buzzer_SetFrequency(0);
        return;
    }
    
    // 限制最小距离为1cm，防止除零
    if (distance_cm < 1) distance_cm = 1;
    
    // 参数定义 (距离->参数映射)
    // 距离越小，频率越高、间隔越短
    uint16_t beep_freq;      // 蜂鸣器频率 (音调)
    uint16_t beep_duration;  // 单声"嘟"持续时间 (ms)
    uint16_t beep_interval;  // 两声"嘟"之间间隔 (ms)
    
    if (distance_cm <= 5) {
        // 危险区域 (1-5cm): 线性插值实现平滑过渡
        // 5cm -> 2cm 区间变化明显
        // 公式: 值 = 最小值 + (距离-1) * (最大值-最小值) / (5-1)
        
        // 频率: 2cm时3500Hz -> 5cm时2500Hz (线性变化)
        // 距离越小，频率越高
        beep_freq = 3500 - (uint16_t)((distance_cm - 1) * 250);  // 3500->2500
        
        // 单声时长: 2cm时180ms -> 5cm时100ms
        beep_duration = 180 - (uint16_t)((distance_cm - 1) * 20);  // 180->100
        
        // 间隔: 2cm时80ms -> 5cm时200ms (越近间隔越短，节奏越急)
        beep_interval = 80 + (uint16_t)((distance_cm - 1) * 30);   // 80->200
        
    } else if (distance_cm <= 8) {
        // 警告区域 (6-8cm)
        beep_freq = 2000;
        beep_duration = 100;
        beep_interval = 350;
    } else {
        // 提醒区域 (9-10cm)
        beep_freq = 1500;
        beep_duration = 80;
        beep_interval = 500;
    }
    
    // 使用 HAL_GetTick() 实现非阻塞式"嘟嘟嘟"
    uint32_t now = HAL_GetTick();
    uint16_t cycle_time = beep_state ? beep_duration : beep_interval;
    
    if (now - last_beep_time >= cycle_time) {
        last_beep_time = now;
        beep_state = !beep_state;  // 切换状态
        
        if (beep_state) {
            // "嘟" - 发声
            Buzzer_SetFrequency(beep_freq);
        } else {
            // 静音间隔
            Buzzer_SetFrequency(0);
        }
    }
}

/**
 * @brief 停止近距离报警
 */
void ProximityAlarm_Stop(void)
{
    Buzzer_SetFrequency(0);
}
