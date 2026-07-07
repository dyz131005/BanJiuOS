#ifndef FONT_H
#define FONT_H

#include <types.h>

#define FONT_WIDTH 8
#define FONT_HEIGHT 16
#define FONT_FIRST_CHAR 0x20
#define FONT_LAST_CHAR 0x7E
#define FONT_CHAR_COUNT 95

extern const u8 font_bitmap[FONT_CHAR_COUNT][FONT_HEIGHT];

void font_init(void);
void font_draw_char(u8 c, u16 x, u16 y, u32 fg_color, u32 bg_color);
void font_print(const char* str, u16 x, u16 y, u32 fg_color, u32 bg_color);
void font_clear_screen(u32 color);

#endif
