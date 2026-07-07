#ifndef BANJUEFI_H
#define BANJUEFI_H

#include <stdint.h>

#define BANJU_EFI_SIGNATURE "BJEF"

#pragma pack(push, 1)

typedef struct {
    uint8_t  signature[4];
    uint16_t version;
    uint16_t header_size;
    uint32_t flags;
    uint64_t entry_point;
    uint64_t kernel_address;
    uint64_t kernel_size;
    uint64_t ramdisk_address;
    uint64_t ramdisk_size;
    uint32_t checksum;
} banju_efi_header_t;

typedef struct {
    uint8_t  device_path[16];
    uint16_t hd_number;
    uint16_t hd_partition;
} boot_device_t;

#pragma pack(pop)

#define BANJU_EFI_BOOT_MODE_UEFI     0x0001
#define BANJU_EFI_BOOT_MODE_LEGACY   0x0002
#define BANJU_EFI_BOOT_MODE_SAFE      0x0004
#define BANJU_EFI_BOOT_MODE_VERBOSE   0x0008

#define BANJU_EFI_LOADER_VERSION "1.0.0"

typedef struct {
    const char *name;
    const char *kernel_path;
    const char *initrd_path;
    uint64_t kernel_addr;
    uint64_t initrd_addr;
} boot_entry_t;

typedef struct {
    uint8_t         signature[4];
    uint16_t        version;
    uint16_t        boot_mode;
    uint64_t        memory_limit;
    boot_device_t   boot_device;
    boot_entry_t    default_entry;
    uint32_t        timeout;
    uint8_t         quiet_boot;
    uint8_t         splash_enabled;
} banju_boot_config_t;

int banju_efi_main(void *image_handle, void *system_table);

void banju_efi_print(const char *message);
void banju_efi_clear_screen(void);
void banju_efi_set_cursor_position(uint32_t x, uint32_t y);

int banju_efi_locate_kernel(const char *path);
int banju_efi_load_kernel(const char *path, void **addr, uint64_t *size);
int banju_efi_load_initrd(const char *path, void **addr, uint64_t *size);

int banju_efi_jump_to_kernel(uint64_t entry, uint64_t kernel_addr, uint64_t kernel_size,
                              uint64_t initrd_addr, uint64_t initrd_size);

int banju_efi_read_file(const char *path, void **buffer, uint64_t *size);
void banju_efi_free_file(void *buffer);

int banju_efi_get_boot_devices(boot_device_t **devices, uint32_t *count);
int banju_efi_read_config(banju_boot_config_t *config);

void banju_efi_set_timeout(uint32_t seconds);
int banju_efi_boot_menu(void);

void banju_efi_draw_splash(void);
void banju_efi_draw_progress_bar(const char *message, uint32_t percent);

void banju_efi_panic(const char *message);

uint32_t banju_efi_calculate_checksum(void *data, uint64_t size);

#endif
