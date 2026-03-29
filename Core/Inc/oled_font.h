#ifndef __OLED_FONT_H
#define __OLED_FONT_H

#include <stdint.h>

// 6x8 ASCII字体 (96个字符: 空格0x20 ~ 下划线0x5F)
// 每个字符6字节 - 用于OLED_UI库
extern const uint8_t asc2_0608[96][6];

// 8x16 ASCII字体 (96个字符)
// 每个字符16字节 - 用于ssd1306.c
extern const uint8_t F8X16[96][16];

// 6x8 ASCII字体 (96个字符) - 用于ssd1306.c
extern const uint8_t F6x8[96][6];

#endif /* __OLED_FONT_H */
