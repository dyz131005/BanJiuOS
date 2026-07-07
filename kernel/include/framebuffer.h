#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <types.h>

#define PIXEL_FORMAT_BGRA 0
#define PIXEL_FORMAT_RGBA 1

typedef struct {
    u64 phys_base;
    u32 width;
    u32 height;
    u32 pitch;
    u32 pixel_format;
    u64 size;
    u8* virt_base;
} FramebufferInfo;

extern FramebufferInfo g_framebuffer;

void framebuffer_init(FramebufferInfo* info);
void framebuffer_draw_pixel(u32 x, u32 y, u32 color);
void framebuffer_clear(u32 color);

#endif