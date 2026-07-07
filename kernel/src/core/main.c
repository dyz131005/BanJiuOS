#include <types.h>
#include <mm.h>
#include <proc.h>
#include <fs.h>
#include <string.h>
#include <font.h>
#include <framebuffer.h>
#include <shell.h>

extern void pmm_init(MemoryMapEntry* mmap, size_t mmap_size);
extern void kmalloc_init(void);

typedef struct {
    u32 signature;
    u32 revision;
    u32 header_size;
    u32 crc32;
    u32 reserved;
} EFI_TABLE_HEADER;

typedef struct {
    EFI_TABLE_HEADER hdr;
    u64 firmware_vendor;
    u32 firmware_revision;
    u64 console_in_handle;
    u64 con_in;
    u64 console_out_handle;
    u64 con_out;
    u64 standard_error_handle;
    u64 std_err;
    u64 runtime_services;
    u64 boot_services;
    u64 number_of_table_entries;
    u64 configuration_table;
} EFI_SYSTEM_TABLE;

typedef struct {
    u32 type;
    u32 padding;
    u64 physical_start;
    u64 virtual_start;
    u64 number_of_pages;
    u64 attribute;
} EFI_MEMORY_DESCRIPTOR;

void* efi_system_table = NULL;

#define COLOR_BLACK 0x00000000
#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_RED 0xFFFF0000
#define COLOR_GREEN 0xFF00FF00
#define COLOR_BLUE 0xFF0000FF
#define COLOR_YELLOW 0xFFFFFF00
#define COLOR_CYAN 0xFF00FFFF
#define COLOR_MAGENTA 0xFFFF00FF
#define COLOR_GRAY 0xFF808080

static void __attribute__((ms_abi)) kmain(u64 mmap_addr, u64 mmap_size, void* system_table, FramebufferInfo* fb_info) {
    efi_system_table = system_table;
    
    MemoryMapEntry* mmap = (MemoryMapEntry*)mmap_addr;
    pmm_init(mmap, mmap_size);
    kmalloc_init();
    
    if (fb_info->phys_base != 0) {
        framebuffer_init(fb_info);
        font_clear_screen(COLOR_BLACK);
        
        font_print("=== BanJiuOS Kernel v1.0 ===\n", 0, 0, COLOR_WHITE, COLOR_BLACK);
        font_print("----------------------------\n\n", 0, 16, COLOR_WHITE, COLOR_BLACK);
        
        font_print("[*] Framebuffer initialized\n", 0, 48, COLOR_YELLOW, COLOR_BLACK);
        font_print("[*] Resolution: ", 0, 64, COLOR_YELLOW, COLOR_BLACK);
        
        char res_str[32];
        itoa(fb_info->width, res_str, 10);
        font_print(res_str, FONT_WIDTH * 18, 64, COLOR_GREEN, COLOR_BLACK);
        font_print("x", FONT_WIDTH * 22, 64, COLOR_WHITE, COLOR_BLACK);
        itoa(fb_info->height, res_str, 10);
        font_print(res_str, FONT_WIDTH * 24, 64, COLOR_GREEN, COLOR_BLACK);
        font_print("\n", 0, 80, COLOR_WHITE, COLOR_BLACK);
        
        font_print("[OK] Physical Memory Manager initialized\n", 0, 96, COLOR_GREEN, COLOR_BLACK);
        font_print("[OK] Kernel Heap initialized\n", 0, 112, COLOR_GREEN, COLOR_BLACK);
        
        font_print("[*] Testing kmalloc... ", 0, 128, COLOR_YELLOW, COLOR_BLACK);
        void* test_ptr = kmalloc(1024);
        if (test_ptr) {
            font_print("SUCCESS\n", FONT_WIDTH * 20, 128, COLOR_GREEN, COLOR_BLACK);
            kfree(test_ptr);
            font_print("[OK] kfree test passed\n", 0, 144, COLOR_GREEN, COLOR_BLACK);
        } else {
            font_print("FAILED\n", FONT_WIDTH * 20, 128, COLOR_RED, COLOR_BLACK);
            font_print("[ERR] Memory allocation failed!\n", 0, 144, COLOR_RED, COLOR_BLACK);
        }
        
        font_print("\n[*] Initializing file systems...\n", 0, 160, COLOR_YELLOW, COLOR_BLACK);
        
        fs_init();
        font_print("[OK] File System Manager initialized\n", 0, 176, COLOR_GREEN, COLOR_BLACK);
        
        extern FileSystem ext4_fs;
        fs_register(&ext4_fs);
        font_print("[OK] EXT4 File System registered\n", 0, 192, COLOR_GREEN, COLOR_BLACK);
        
        extern FileSystem fat32_fs;
        fs_register(&fat32_fs);
        font_print("[OK] FAT32 File System registered\n", 0, 208, COLOR_GREEN, COLOR_BLACK);
        
        font_print("\n[*] Initializing process manager...\n", 0, 224, COLOR_YELLOW, COLOR_BLACK);
        proc_init();
        font_print("[OK] Process Manager initialized\n", 0, 240, COLOR_GREEN, COLOR_BLACK);
        
        font_print("\n=== System Ready ===\n", 0, 256, COLOR_WHITE, COLOR_BLACK);
        font_print("BanJiuOS is running in protected mode.\n", 0, 272, COLOR_WHITE, COLOR_BLACK);
        font_print("Starting shell...\n", 0, 288, COLOR_WHITE, COLOR_BLACK);
        
        shell_start();
    } else {
        u16* vga = (u16*)0xB8000;
        vga[0] = 0x0F << 8 | 'N';
        vga[1] = 0x0F << 8 | 'o';
        vga[2] = 0x0F << 8 | ' ';
        vga[3] = 0x0F << 8 | 'G';
        vga[4] = 0x0F << 8 | 'O';
        vga[5] = 0x0F << 8 | 'P';
        vga[6] = 0x0F << 8 | ' ';
        vga[7] = 0x0F << 8 | 'f';
        vga[8] = 0x0F << 8 | 'o';
        vga[9] = 0x0F << 8 | 'u';
        vga[10] = 0x0F << 8 | 'n';
        vga[11] = 0x0F << 8 | 'd';
    }
    
    while (1) {
        asm volatile("hlt");
    }
}

void __attribute__((section(".text.startup"), ms_abi)) _start(u64 mmap_addr, u64 mmap_size, void* system_table, FramebufferInfo* fb_info) {
    kmain(mmap_addr, mmap_size, system_table, fb_info);
    
    while (1) {
        asm volatile("hlt");
    }
}
