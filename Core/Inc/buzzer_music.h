#ifndef __BUZZER_MUSIC_H
#define __BUZZER_MUSIC_H

#include "main.h"

// 蜂鸣器引脚定义 (PB0)
#define BUZZER_GPIO_PORT    GPIOB
#define BUZZER_GPIO_PIN     GPIO_PIN_0

// 音符频率定义 (参考《起风了》项目)
// 低音 (Low)
#define L1      262
#define L2      294
#define L3      330
#define L4      349
#define L5      392
#define L6      440
#define L7      494

// 中音 (Medium) - 常用音阶
#define M1      523
#define M2      587
#define M3      659
#define M4      698
#define M5      784
#define M6      880
#define M7      988

// 高音 (High)
#define H1      1046
#define H2      1175
#define H3      1318
#define H4      1397
#define H5      1568
#define H6      1760
#define H7      1976

// 超高音
#define H8      2093

// 休止符
#define REST    0

// 时基 (一拍ms数)
#define TEMPO   400

void BuzzerMusic_Init(void);
void BuzzerMusic_Stop(void);
void Buzzer_SetFrequency(uint16_t freq);
void Buzzer_SetVolume(uint8_t volume);  // 设置音量 0-100

// 音效函数
void StartupSound(void);    // 开机音乐 - 《起风了》前奏
void AlarmSound(void);      // 报警音效
void SuccessSound(void);    // 成功音效

// 距离报警函数
void ProximityAlarm(uint16_t distance_cm);   // 近距离报警 (距离越近越尖锐)
void ProximityAlarm_Stop(void);              // 停止近距离报警

#endif
