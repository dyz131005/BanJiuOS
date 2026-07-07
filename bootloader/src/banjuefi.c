#include "banjuefi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static banju_boot_config_t g_boot_config;
static boot_device_t *g_boot_devices = NULL;
static uint32_t g_boot_device_count = 0;

#define BOOT_SCREEN_WIDTH  80
#define BOOT_SCREEN_HEIGHT 25

static const char *g_splash_art[] = {
    "  ____  _          ____                  _   _             ",
    " |  _ \\(_) ___    / ___|__ _ ___ _ __   | \\ | | __ _ _ __ ",
    " | |_) | |/ _ \\  | |   / _` / __| '_ \\  |  \\| |/ _` | '_ \\",
    " |  __/| | (_) | | |__| (_| \\__ \\ |_) | | |\\  | (_| | | | |",
    " |_|   |_|\\___/   \\____\\__,_|___/ .__/  |_| \\_|\\__,_|_| |_|",
    "                                 |_|                       ",
    "                                                          ",
    "        Operating System Loader v" BANJU_EFI_LOADER_VERSION "                     ",
    "        Copyright (C) 2024 BanJiuOS Project              ",
};

static const char *g_boot_menu_items[] = {
    "Start BanJiuOS",
    "Safe Mode",
    "Verbose Mode",
    "Memory Test (memtest86+)",
    "Boot from USB",
    "Setup (BIOS)",
    "Reboot",
    "Shutdown",
};

void banju_efi_print(const char *message) {
    if (message) {
        printf("%s", message);
    }
}

void banju_efi_clear_screen(void) {
    printf("\033[2J");
    printf("\033[H");
}

void banju_efi_set_cursor_position(uint32_t x, uint32_t y) {
    printf("\033[%d;%dH", y + 1, x + 1);
}

void banju_efi_draw_splash(void) {
    banju_efi_clear_screen();
    banju_efi_set_cursor_position(0, 0);

    for (int i = 0; i < sizeof(g_splash_art) / sizeof(g_splash_art[0]); i++) {
        banju_efi_print(g_splash_art[i]);
        banju_efi_print("\n");
    }

    banju_efi_print("\n\n");
    banju_efi_print("    Initializing hardware...");
}

void banju_efi_draw_progress_bar(const char *message, uint32_t percent) {
    int bar_width = 50;
    int pos = (bar_width * percent) / 100;

    banju_efi_set_cursor_position(0, 20);
    banju_efi_print("    ");
    banju_efi_print(message);
    banju_efi_print("\n    [");

    for (int i = 0; i < bar_width; i++) {
        if (i < pos) {
            banju_efi_print("=");
        } else if (i == pos) {
            banju_efi_print(">");
        } else {
            banju_efi_print(" ");
        }
    }

    banju_efi_print("] ");
    printf("%3d%%", percent);
    banju_efi_print("\n");
}

int banju_efi_read_file(const char *path, void **buffer, uint64_t *size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *buffer = malloc(*size);
    if (!*buffer) {
        fclose(fp);
        return -1;
    }

    if (fread(*buffer, 1, *size, fp) != *size) {
        free(*buffer);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

void banju_efi_free_file(void *buffer) {
    if (buffer) {
        free(buffer);
    }
}

int banju_efi_locate_kernel(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return -1;
    }
    fclose(fp);
    return 0;
}

int banju_efi_load_kernel(const char *path, void **addr, uint64_t *size) {
    return banju_efi_read_file(path, addr, size);
}

int banju_efi_load_initrd(const char *path, void **addr, uint64_t *size) {
    return banju_efi_read_file(path, addr, size);
}

int banju_efi_jump_to_kernel(uint64_t entry, uint64_t kernel_addr, uint64_t kernel_size,
                              uint64_t initrd_addr, uint64_t initrd_size) {
    typedef void (*kernel_entry_t)(uint64_t kaddr, uint64_t ksize,
                                    uint64_t iaddr, uint64_t isize);
    kernel_entry_t entry_fn = (kernel_entry_t)entry;

    banju_efi_print("\n\n    Transferring control to kernel...\n");

    entry_fn(kernel_addr, kernel_size, initrd_addr, initrd_size);

    return 0;
}

int banju_efi_get_boot_devices(boot_device_t **devices, uint32_t *count) {
    *devices = NULL;
    *count = 0;
    return 0;
}

int banju_efi_read_config(banju_boot_config_t *config) {
    if (!config) {
        return -1;
    }

    memset(config, 0, sizeof(banju_boot_config_t));

    memcpy(config->signature, BANJU_EFI_SIGNATURE, 4);
    config->version = 0x0100;
    config->boot_mode = BANJU_EFI_BOOT_MODE_UEFI;
    config->memory_limit = 0;
    config->timeout = 10;
    config->quiet_boot = 0;
    config->splash_enabled = 1;

    strcpy(config->default_entry.name, "BanJiuOS");
    strcpy(config->default_entry.kernel_path, "\\System\\kernel.zyxt");
    strcpy(config->default_entry.initrd_path, "\\System\\initramfs.img");

    return 0;
}

void banju_efi_set_timeout(uint32_t seconds) {
    g_boot_config.timeout = seconds;
}

static void draw_menu_item(int index, int selected) {
    if (selected) {
        banju_efi_print("  > ");
    } else {
        banju_efi_print("    ");
    }

    printf("%d. %s\n", index + 1, g_boot_menu_items[index]);
}

int banju_efi_boot_menu(void) {
    int selected = 0;
    int num_items = sizeof(g_boot_menu_items) / sizeof(g_boot_menu_items[0]);
    int done = 0;
    char input[32];

    while (!done) {
        banju_efi_clear_screen();
        banju_efi_print("BanJiuOS Boot Menu\n");
        banju_efi_print("==================\n\n");

        for (int i = 0; i < num_items; i++) {
            draw_menu_item(i, (i == selected));
        }

        banju_efi_print("\n\nUse arrow keys or 1-");
        printf("%d", num_items);
        banju_efi_print(" to select, Enter to confirm\n");

        if (fgets(input, sizeof(input), stdin) != NULL) {
            if (input[0] >= '1' && input[0] <= '9') {
                int choice = input[0] - '1';
                if (choice < num_items) {
                    selected = choice;
                }
            }

            if (input[0] == '\n' || input[0] == '\r') {
                done = 1;
            }
        }
    }

    return selected;
}

void banju_efi_panic(const char *message) {
    banju_efi_clear_screen();
    banju_efi_print("\n\n*** PANIC ***\n\n");
    banju_efi_print(message);
    banju_efi_print("\n\nSystem halted. Press any key to reboot...\n");

    while (1) { }
}

uint32_t banju_efi_calculate_checksum(void *data, uint64_t size) {
    uint32_t sum = 0;
    uint8_t *bytes = (uint8_t *)data;

    for (uint64_t i = 0; i < size; i++) {
        sum += bytes[i];
    }

    return sum;
}

static int boot_init(void) {
    memset(&g_boot_config, 0, sizeof(g_boot_config));

    if (banju_efi_read_config(&g_boot_config) != 0) {
        banju_efi_panic("Failed to read boot configuration");
        return -1;
    }

    if (banju_efi_get_boot_devices(&g_boot_devices, &g_boot_device_count) != 0) {
        banju_efi_panic("Failed to enumerate boot devices");
        return -1;
    }

    return 0;
}

static int boot_load_kernel(void) {
    void *kernel_addr = NULL;
    uint64_t kernel_size = 0;
    void *initrd_addr = NULL;
    uint64_t initrd_size = 0;
    int result = 0;

    banju_efi_draw_progress_bar("Loading kernel", 0);

    if (banju_efi_load_kernel(g_boot_config.default_entry.kernel_path,
                              &kernel_addr, &kernel_size) != 0) {
        banju_efi_print("\n\nWarning: Kernel not found at default location.\n");
        banju_efi_print("Please ensure kernel.zyxt is present in the System directory.\n");
        return -1;
    }

    banju_efi_draw_progress_bar("Loading kernel", 50);

    if (banju_efi_load_initrd(g_boot_config.default_entry.initrd_path,
                               &initrd_addr, &initrd_size) != 0) {
        banju_efi_print("\n\nWarning: Initramfs not found.\n");
    }

    banju_efi_draw_progress_bar("Loading kernel", 100);

    banju_efi_print("\n\nKernel loaded successfully.\n");
    banju_efi_print("Kernel address: ");
    printf("0x%016llX", (unsigned long long)kernel_addr);
    banju_efi_print("\n");

    return result;
}

int banju_efi_main(void *image_handle, void *system_table) {
    (void)image_handle;
    (void)system_table;

    if (boot_init() != 0) {
        return -1;
    }

    if (g_boot_config.splash_enabled) {
        banju_efi_draw_splash();
    }

    for (int i = 0; i <= 100; i += 10) {
        banju_efi_draw_progress_bar("Initializing boot environment", i);
    }

    int menu_selection = banju_efi_boot_menu();

    switch (menu_selection) {
        case 0:
            if (boot_load_kernel() == 0) {
                banju_efi_print("\nBooting BanJiuOS...\n");
            }
            break;

        case 1:
            banju_efi_print("\nEntering safe mode...\n");
            break;

        case 2:
            banju_efi_print("\nEntering verbose mode...\n");
            break;

        case 6:
            banju_efi_print("\nRebooting...\n");
            break;

        case 7:
            banju_efi_print("\nShutting down...\n");
            break;

        default:
            banju_efi_print("\nInvalid selection.\n");
            break;
    }

    return 0;
}
