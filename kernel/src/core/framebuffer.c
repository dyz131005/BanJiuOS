#include <framebuffer.h>
#include <mm.h>

FramebufferInfo g_framebuffer = {0};

void framebuffer_init(FramebufferInfo* info) {
    g_framebuffer = *info;

    // The bootloader keeps the low 4GB identity-mapped, and QEMU's GOP
    // framebuffer lives at 0x80000000 in the current setup. Reuse that
    // mapping for now instead of building a separate higher-half alias.
    g_framebuffer.virt_base = (u8*)(u64)info->phys_base;
}

void framebuffer_draw_pixel(u32 x, u32 y, u32 color) {
    if (x >= g_framebuffer.width || y >= g_framebuffer.height) {
        return;
    }
    
    u64 offset = (u64)y * (u64)g_framebuffer.pitch + (u64)x * 4;
    u32* fb_ptr = (u32*)(g_framebuffer.virt_base + offset);
    
    if (g_framebuffer.pixel_format == PIXEL_FORMAT_BGRA) {
        *fb_ptr = color;
    } else {
        u32 r = (color >> 16) & 0xFF;
        u32 g = (color >> 8) & 0xFF;
        u32 b = color & 0xFF;
        u32 a = (color >> 24) & 0xFF;
        *fb_ptr = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

void framebuffer_clear(u32 color) {
    for (u32 y = 0; y < g_framebuffer.height; y++) {
        u32* row = (u32*)(g_framebuffer.virt_base + (u64)y * (u64)g_framebuffer.pitch);

        for (u32 x = 0; x < g_framebuffer.width; x++) {
            row[x] = color;
        }
    }
}
