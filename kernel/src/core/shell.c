#include <shell.h>
#include <font.h>
#include <framebuffer.h>
#include <string.h>
#include <fs.h>
#include <types.h>
#include <stdarg.h>

#define SHELL_PROMPT ">"
#define SHELL_MAX_CMD 256
#define SHELL_MAX_ARGS 16
#define SHELL_MAX_PATH 512

#define COLOR_BLACK 0x00000000
#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_RED 0xFFFF0000
#define COLOR_GREEN 0xFF00FF00
#define COLOR_BLUE 0xFF0000FF
#define COLOR_YELLOW 0xFFFFFF00

static char current_dir[SHELL_MAX_PATH] = "/";
static char cmd_buffer[SHELL_MAX_CMD];
static u16 cursor_x = 0;
static u16 cursor_y = 0;

static void normalize_path(char* path) {
    char result[SHELL_MAX_PATH];
    char* p = path;
    char* r = result;
    char* last_slash = NULL;
    
    *r++ = '/';
    
    while (*p) {
        while (*p == '/') {
            p++;
        }
        
        if (!*p) {
            break;
        }
        
        if (strncmp(p, "..", 2) == 0 && (!p[2] || p[2] == '/')) {
            if (last_slash) {
                r = last_slash;
                *r = '/';
                r++;
            }
            p += 2;
            continue;
        }
        
        if (strncmp(p, ".", 1) == 0 && (!p[1] || p[1] == '/')) {
            p += 1;
            continue;
        }
        
        last_slash = r - 1;
        while (*p && *p != '/') {
            *r++ = *p++;
        }
        *r++ = '/';
    }
    
    if (r > result + 1 && *(r - 1) == '/') {
        r--;
    }
    *r = '\0';
    
    strcpy(path, result);
}

#define LOG_BUFFER_SIZE 4096
static char log_buffer[LOG_BUFFER_SIZE];
static int log_pos = 0;

static inline u8 inb(u16 port);
static inline void outb(u16 port, u8 val);

#define COM1_BASE 0x3F8

static void serial_init(void) {
    outb(COM1_BASE + 1, 0x00);
    outb(COM1_BASE + 3, 0x80);
    outb(COM1_BASE + 0, 0x10);
    outb(COM1_BASE + 1, 0x00);
    outb(COM1_BASE + 3, 0x03);
    outb(COM1_BASE + 2, 0xC7);
    outb(COM1_BASE + 4, 0x0B);
    outb(COM1_BASE + 4, 0x1E);
    outb(COM1_BASE + 0, 0xAE);
    
    while (inb(COM1_BASE + 5) & 0x01) {
        inb(COM1_BASE + 0);
    }
    
    outb(COM1_BASE + 4, 0x0F);
}

static void serial_putchar(char c) {
    while (!(inb(COM1_BASE + 5) & 0x20)) {
        asm volatile("nop");
    }
    outb(COM1_BASE + 0, c);
}

static void serial_log(const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}

static void serial_log_hex(u32 val) {
    char hex[] = "0123456789ABCDEF";
    serial_log("0x");
    for (int i = 7; i >= 0; i--) {
        serial_putchar(hex[(val >> (i * 4)) & 0xF]);
    }
}

static void serial_log_str(const char* label, const char* str) {
    serial_log(label);
    serial_log(str);
    serial_log("\n");
}

static void serial_print(const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}

static void log_print(const char* fmt, ...);
static void log_dump(void);

static void shell_print(const char* str) {
    font_print(str, cursor_x * FONT_WIDTH, cursor_y * FONT_HEIGHT, COLOR_WHITE, COLOR_BLACK);
    while (*str) {
        if (*str == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else {
            cursor_x++;
            if (cursor_x >= g_framebuffer.width / FONT_WIDTH) {
                cursor_x = 0;
                cursor_y++;
            }
        }
        str++;
    }
    if (cursor_y >= g_framebuffer.height / FONT_HEIGHT) {
        cursor_y = 0;
        font_clear_screen(COLOR_BLACK);
    }
}

static void shell_print_color(const char* str, u32 fg_color) {
    font_print(str, cursor_x * FONT_WIDTH, cursor_y * FONT_HEIGHT, fg_color, COLOR_BLACK);
    while (*str) {
        if (*str == '\n') {
            cursor_x = 0;
            cursor_y++;
        } else {
            cursor_x++;
            if (cursor_x >= g_framebuffer.width / FONT_WIDTH) {
                cursor_x = 0;
                cursor_y++;
            }
        }
        str++;
    }
    if (cursor_y >= g_framebuffer.height / FONT_HEIGHT) {
        cursor_y = 0;
        font_clear_screen(COLOR_BLACK);
    }
}

static void shell_newline(void) {
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= g_framebuffer.height / FONT_HEIGHT) {
        cursor_y = 0;
        font_clear_screen(COLOR_BLACK);
    }
}

static void shell_draw_char(char c) {
    font_draw_char(c, cursor_x * FONT_WIDTH, cursor_y * FONT_HEIGHT, COLOR_WHITE, COLOR_BLACK);
    cursor_x++;
    if (cursor_x >= g_framebuffer.width / FONT_WIDTH) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= g_framebuffer.height / FONT_HEIGHT) {
            cursor_y = 0;
            font_clear_screen(COLOR_BLACK);
        }
    }
}

static inline u8 inb(u16 port) {
    u8 val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outb(u16 port, u8 val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static char scan_code_to_ascii(u8 scancode, int* is_released) {
    static const char scancode_map[0x60] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
        'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\r', 0, 0, 0, 0, 0
    };
    
    static const char scancode_map_shift[0x60] = {
        0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\r', 0, 0, 0, 0, 0
    };
    
    static int shift_pressed = 0;
    
    if (scancode & 0x80) {
        *is_released = 1;
        scancode &= 0x7F;
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 0;
        }
        return 0;
    }
    
    *is_released = 0;
    
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return 0;
    }
    
    if (scancode >= sizeof(scancode_map)) {
        return 0;
    }
    
    return shift_pressed ? scancode_map_shift[scancode] : scancode_map[scancode];
}

static void keyboard_wait_input(void) {
    int timeout = 100000;
    while ((inb(0x64) & 0x02) && timeout--) {
        asm volatile("nop");
    }
}

static u8 keyboard_wait_read(void) {
    int timeout = 1000000;
    while (!(inb(0x64) & 0x01) && timeout--) {
        asm volatile("nop");
    }
    return inb(0x60);
}

static void keyboard_init(void) {
    log_print("keyboard_init: started\n");
    
    log_print("keyboard_init: disabling keyboard (0xAD)\n");
    outb(0x64, 0xAD);
    
    log_print("keyboard_init: flushing buffer\n");
    for (int i = 0; i < 20; i++) {
        if (inb(0x64) & 0x01) {
            u8 data = inb(0x60);
            log_print("keyboard_init: flushed data 0x%x\n", data);
        } else {
            break;
        }
    }
    
    log_print("keyboard_init: sending reset via 0xD4\n");
    keyboard_wait_input();
    outb(0x64, 0xD4);
    keyboard_wait_input();
    outb(0x60, 0xFF);
    
    u8 ack = keyboard_wait_read();
    log_print("keyboard_init: got ACK 0x%x\n", ack);
    
    u8 self_test = keyboard_wait_read();
    log_print("keyboard_init: got self-test 0x%x\n", self_test);
    
    log_print("keyboard_init: setting scan code set 1\n");
    keyboard_wait_input();
    outb(0x64, 0xD4);
    keyboard_wait_input();
    outb(0x60, 0xF0);
    ack = keyboard_wait_read();
    log_print("keyboard_init: F0 ACK 0x%x\n", ack);
    
    keyboard_wait_input();
    outb(0x64, 0xD4);
    keyboard_wait_input();
    outb(0x60, 0x01);
    ack = keyboard_wait_read();
    log_print("keyboard_init: set1 ACK 0x%x\n", ack);
    
    log_print("keyboard_init: enabling keyboard (0xAE)\n");
    keyboard_wait_input();
    outb(0x64, 0xAE);
    
    log_print("keyboard_init: enabling data reporting (0xF4)\n");
    keyboard_wait_input();
    outb(0x64, 0xD4);
    keyboard_wait_input();
    outb(0x60, 0xF4);
    ack = keyboard_wait_read();
    log_print("keyboard_init: F4 ACK 0x%x\n", ack);
    
    log_print("keyboard_init: done\n");
}

static char shell_read_char(void) {
    while (1) {
        u8 status = inb(0x64);
        while (!(status & 0x01)) {
            asm volatile("nop");
            status = inb(0x64);
        }
        
        u8 scancode = inb(0x60);
        int is_released;
        char c = scan_code_to_ascii(scancode, &is_released);
        
        if (!is_released && c != 0) {
            return c;
        }
    }
}

static int shell_parse_args(char* cmd, char** args, int max_args) {
    int argc = 0;
    char* token = strtok(cmd, " \t\n");
    
    while (token && argc < max_args) {
        args[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    
    args[argc] = NULL;
    return argc;
}

static void shell_cmd_cd(int argc, char** args) {
    serial_log("=== shell_cmd_cd START ===\n");
    
    if (argc < 2) {
        shell_print(current_dir);
        shell_newline();
        serial_log("=== shell_cmd_cd END (no args) ===\n");
        return;
    }
    
    serial_log_str("args[1] = ", args[1]);
    serial_log_str("current_dir = ", current_dir);
    
    char new_path[SHELL_MAX_PATH];
    
    if (args[1][0] == '/') {
        strcpy(new_path, args[1]);
        serial_log_str("absolute path, new_path = ", new_path);
    } else {
        if (strcmp(current_dir, "/") == 0) {
            strcpy(new_path, "/");
            strcat(new_path, args[1]);
        } else {
            strcpy(new_path, current_dir);
            strcat(new_path, "/");
            strcat(new_path, args[1]);
        }
        serial_log_str("relative path, new_path = ", new_path);
    }
    
    if (new_path[0] == '\0') {
        strcpy(new_path, "/");
        serial_log("new_path was empty, set to /\n");
    }
    
    normalize_path(new_path);
    serial_log_str("after normalize_path, path = ", new_path);
    
    serial_log_str("calling fs_open with path = ", new_path);
    
    FsNode* node = fs_open(new_path);
    
    serial_log("fs_open returned: ");
    if (node) {
        serial_log("NOT NULL\n");
        serial_log("node->name = ");
        serial_log(node->name);
        serial_log("\n");
        serial_log("node->mode = ");
        serial_log_hex(node->mode);
        serial_log("\n");
        serial_log("node->mode & 0x4000 = ");
        serial_log_hex(node->mode & 0x4000);
        serial_log("\n");
    } else {
        serial_log("NULL\n");
    }
    
    if (node && (node->mode & 0x4000)) {
        strcpy(current_dir, new_path);
        serial_log_str("SUCCESS! current_dir = ", current_dir);
        fs_close(node);
    } else {
        shell_print_color("cd: directory not found\n", COLOR_RED);
        serial_log("FAILED: directory not found\n");
        if (node) {
            serial_log("NOTE: node exists but is not a directory (mode & 0x4000 == 0)\n");
            fs_close(node);
        }
    }
    
    serial_log("=== shell_cmd_cd END ===\n");
}

static void shell_cmd_mkdir(int argc, char** args) {
    serial_log("=== shell_cmd_mkdir START ===\n");
    
    if (argc < 2) {
        shell_print_color("mkdir: missing operand\n", COLOR_RED);
        serial_log("=== shell_cmd_mkdir END (no args) ===\n");
        return;
    }
    
    serial_log_str("args[1] = ", args[1]);
    serial_log_str("current_dir = ", current_dir);
    
    char new_path[FS_MAX_PATH];
    
    if (args[1][0] == '/') {
        strcpy(new_path, args[1]);
        serial_log_str("absolute path, new_path = ", new_path);
    } else {
        strcpy(new_path, current_dir);
        if (new_path[strlen(new_path) - 1] != '/') {
            strcat(new_path, "/");
        }
        strcat(new_path, args[1]);
        serial_log_str("relative path, new_path = ", new_path);
    }
    
    if (new_path[0] == '\0') {
        strcpy(new_path, "/");
        serial_log("new_path was empty, set to /\n");
    }
    
    serial_log_str("calling fs_mkdir with path = ", new_path);
    serial_log("mode = 0\n");
    
    int result = fs_mkdir(new_path, 0);
    
    serial_log("fs_mkdir returned: ");
    serial_log_hex(result);
    serial_log("\n");
    
    if (result == 0) {
        serial_log_str("SUCCESS! created directory: ", new_path);
    } else {
        shell_print_color("mkdir: cannot create directory '", COLOR_RED);
        shell_print_color(args[1], COLOR_RED);
        shell_print_color("': No such file or directory\n", COLOR_RED);
        serial_log("FAILED: fs_mkdir returned non-zero\n");
    }
    
    serial_log("=== shell_cmd_mkdir END ===\n");
}

static void shell_cmd_rm(int argc, char** args) {
    if (argc < 2) {
        shell_print_color("rm: missing operand\n", COLOR_RED);
        return;
    }
    
    char full_path[FS_MAX_PATH];
    if (args[1][0] == '/') {
        strcpy(full_path, args[1]);
    } else {
        strcpy(full_path, current_dir);
        if (full_path[strlen(full_path) - 1] != '/') {
            strcat(full_path, "/");
        }
        strcat(full_path, args[1]);
    }
    
    int result = fs_remove(full_path);
    if (result != 0) {
        shell_print_color("rm: cannot remove '", COLOR_RED);
        shell_print_color(args[1], COLOR_RED);
        shell_print_color("': No such file or directory\n", COLOR_RED);
    }
}

static void shell_cmd_rmdir(int argc, char** args) {
    if (argc < 2) {
        shell_print_color("rmdir: missing operand\n", COLOR_RED);
        return;
    }
    
    char full_path[FS_MAX_PATH];
    if (args[1][0] == '/') {
        strcpy(full_path, args[1]);
    } else {
        strcpy(full_path, current_dir);
        if (full_path[strlen(full_path) - 1] != '/') {
            strcat(full_path, "/");
        }
        strcat(full_path, args[1]);
    }
    
    int result = fs_rmdir(full_path);
    if (result != 0) {
        shell_print_color("rmdir: cannot remove '", COLOR_RED);
        shell_print_color(args[1], COLOR_RED);
        shell_print_color("': No such file or directory\n", COLOR_RED);
    }
}

static void shell_cmd_touch(int argc, char** args) {
    if (argc < 2) {
        shell_print_color("touch: missing operand\n", COLOR_RED);
        return;
    }
    
    char full_path[FS_MAX_PATH];
    if (args[1][0] == '/') {
        strcpy(full_path, args[1]);
    } else {
        strcpy(full_path, current_dir);
        if (full_path[strlen(full_path) - 1] != '/') {
            strcat(full_path, "/");
        }
        strcat(full_path, args[1]);
    }
    
    int result = fs_create(full_path, 0);
    if (result != 0) {
        shell_print_color("touch: cannot create file '", COLOR_RED);
        shell_print_color(args[1], COLOR_RED);
        shell_print_color("': No such file or directory\n", COLOR_RED);
    }
}

static void shell_cmd_ls(int argc, char** args) {
    const char* path = (argc >= 2) ? args[1] : current_dir;
    
    FsNode* node = fs_open(path);
    if (!node) {
        shell_print_color("ls: cannot access '", COLOR_RED);
        shell_print_color(path, COLOR_RED);
        shell_print_color("': No such file or directory\n", COLOR_RED);
        return;
    }
    
    if (!(node->mode & 0x4000)) {
        shell_print(node->name);
        shell_newline();
        fs_close(node);
        return;
    }
    
    FsNode* child = node->children;
    if (!child) {
        shell_print("(empty directory)\n");
        fs_close(node);
        return;
    }
    
    while (child) {
        if (child->mode & 0x4000) {
            shell_print_color(child->name, COLOR_BLUE);
            shell_print_color("/", COLOR_BLUE);
        } else {
            shell_print(child->name);
        }
        shell_print("  ");
        child = child->next;
    }
    shell_newline();
    
    fs_close(node);
}

static void shell_cmd_echo(int argc, char** args) {
    for (int i = 1; i < argc; i++) {
        shell_print(args[i]);
        if (i < argc - 1) {
            shell_print(" ");
        }
    }
    shell_newline();
}

static void shell_cmd_sysinfo(int argc, char** args) {
    shell_print_color("=== BanJiuOS System Information ===\n", COLOR_YELLOW);
    shell_print("Version: 1.0\n");
    shell_print("Kernel: BanJiuOS Kernel\n");
    shell_print("Architecture: x86_64\n");
    shell_print("Current Directory: ");
    shell_print(current_dir);
    shell_newline();
    shell_print("Framebuffer: ");
    char fb_str[64];
    sprintf(fb_str, "%dx%d", g_framebuffer.width, g_framebuffer.height);
    shell_print(fb_str);
    shell_newline();
    shell_print_color("===================================\n", COLOR_YELLOW);
}

static void shell_cmd_log(int argc, char** args) {
    log_dump();
}

static void shell_cmd_mount(int argc, char** args) {
    if (argc < 3) {
        shell_print_color("Usage: mount <device> <mount_point> [fat32|ext4]\n", COLOR_YELLOW);
        return;
    }
    
    FsType type = FS_TYPE_FAT32;
    if (argc >= 4) {
        if (strcmp(args[3], "ext4") == 0) {
            type = FS_TYPE_EXT4;
        } else if (strcmp(args[3], "fat32") != 0) {
            shell_print_color("Unknown filesystem type: ", COLOR_RED);
            shell_print_color(args[3], COLOR_RED);
            shell_print_color("\n", COLOR_RED);
            return;
        }
    }
    
    int result = fs_mount(args[1], args[2], type);
    if (result == 0) {
        shell_print("Mounted ");
        shell_print(args[1]);
        shell_print(" to ");
        shell_print(args[2]);
        shell_newline();
    } else {
        shell_print_color("mount: failed to mount ", COLOR_RED);
        shell_print_color(args[1], COLOR_RED);
        shell_print_color("\n", COLOR_RED);
    }
}

static void shell_cmd_help(int argc, char** args) {
    shell_print_color("Available commands:\n", COLOR_YELLOW);
    shell_print("  cd <dir>          - Change directory\n");
    shell_print("  ls [dir]          - List directory contents\n");
    shell_print("  dir [dir]         - Same as ls\n");
    shell_print("  mkdir <dir>       - Create directory\n");
    shell_print("  rm <file>         - Remove file\n");
    shell_print("  rmdir <dir>       - Remove directory\n");
    shell_print("  touch <file>      - Create empty file\n");
    shell_print("  echo <text>       - Print text\n");
    shell_print("  sysinfo           - Show system information\n");
    shell_print("  mount <dev> <mnt> - Mount filesystem\n");
    shell_print("  log               - Show keyboard debug log\n");
    shell_print("  help              - Show this help\n");
}

static void shell_execute(char* cmd) {
    if (cmd[0] == '\0') {
        return;
    }
    
    char* args[SHELL_MAX_ARGS];
    int argc = shell_parse_args(cmd, args, SHELL_MAX_ARGS);
    
    if (argc == 0) {
        return;
    }
    
    if (strcmp(args[0], "cd") == 0) {
        shell_cmd_cd(argc, args);
    } else if (strcmp(args[0], "ls") == 0 || strcmp(args[0], "dir") == 0) {
        shell_cmd_ls(argc, args);
    } else if (strcmp(args[0], "echo") == 0) {
        shell_cmd_echo(argc, args);
    } else if (strcmp(args[0], "sysinfo") == 0) {
        shell_cmd_sysinfo(argc, args);
    } else if (strcmp(args[0], "log") == 0) {
        shell_cmd_log(argc, args);
    } else if (strcmp(args[0], "mount") == 0) {
        shell_cmd_mount(argc, args);
    } else if (strcmp(args[0], "mkdir") == 0) {
        shell_cmd_mkdir(argc, args);
    } else if (strcmp(args[0], "rm") == 0) {
        shell_cmd_rm(argc, args);
    } else if (strcmp(args[0], "rmdir") == 0) {
        shell_cmd_rmdir(argc, args);
    } else if (strcmp(args[0], "touch") == 0) {
        shell_cmd_touch(argc, args);
    } else if (strcmp(args[0], "help") == 0) {
        shell_cmd_help(argc, args);
    } else {
        shell_print_color(args[0], COLOR_RED);
        shell_print_color(": command not found\n", COLOR_RED);
    }
}

static void log_print(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    
    int i = 0;
    while (*fmt && i < 255) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'x') {
                u32 val = va_arg(args, u32);
                char hex[] = "0123456789ABCDEF";
                buf[i++] = '0';
                buf[i++] = 'x';
                for (int j = 7; j >= 0; j--) {
                    buf[i++] = hex[(val >> (j * 4)) & 0xF];
                }
            } else if (*fmt == 'd') {
                int val = va_arg(args, int);
                char num[16];
                int ni = 0;
                if (val < 0) {
                    buf[i++] = '-';
                    val = -val;
                }
                if (val == 0) {
                    num[ni++] = '0';
                } else {
                    while (val > 0) {
                        num[ni++] = '0' + (val % 10);
                        val /= 10;
                    }
                }
                for (int j = ni - 1; j >= 0; j--) {
                    buf[i++] = num[j];
                }
            } else if (*fmt == 'c') {
                buf[i++] = va_arg(args, int);
            } else if (*fmt == 's') {
                const char* s = va_arg(args, const char*);
                while (*s && i < 255) {
                    buf[i++] = *s++;
                }
            }
            fmt++;
        } else {
            buf[i++] = *fmt++;
        }
    }
    buf[i] = '\0';
    va_end(args);
    
    for (int j = 0; buf[j] && log_pos < LOG_BUFFER_SIZE - 1; j++) {
        log_buffer[log_pos++] = buf[j];
    }
    log_buffer[log_pos] = '\0';
    
    serial_print(buf);
}

static void log_dump(void) {
    shell_print_color("\n=== Keyboard Debug Log ===\n", COLOR_YELLOW);
    shell_print(log_buffer);
    shell_print_color("=== End Log ===\n", COLOR_YELLOW);
}

void shell_start(void) {
    serial_init();
    
    keyboard_init();
    
    cursor_x = 0;
    cursor_y = 0;
    font_clear_screen(COLOR_BLACK);
    shell_print_color("=== BanJiuOS Shell ===\n", COLOR_YELLOW);
    shell_print("Type 'help' for available commands.\n\n");
    
    shell_print("Mounting root filesystem... ");
    log_print("shell_start: before fs_mount('hda', '/', FAT32)\n");
    int mount_result = fs_mount("hda", "/", FS_TYPE_FAT32);
    
    log_print("shell_start: MOUNT RETURNED, mount_result=");
    if (mount_result >= 0) {
        char tmp_num[8];
        int tni = 0;
        int tn = mount_result;
        if (tn == 0) tmp_num[tni++] = '0';
        else while (tn > 0) { tmp_num[tni++] = '0' + (tn % 10); tn /= 10; }
        for (int i = tni - 1; i >= 0; i--) serial_putchar(tmp_num[i]);
    } else {
        serial_putchar('-');
        char tmp_num[8];
        int tni = 0;
        int tn = -mount_result;
        if (tn == 0) tmp_num[tni++] = '0';
        else while (tn > 0) { tmp_num[tni++] = '0' + (tn % 10); tn /= 10; }
        for (int i = tni - 1; i >= 0; i--) serial_putchar(tmp_num[i]);
    }
    serial_putchar('\n');
    
    log_print("shell_start: after fs_mount, result=");
    if (mount_result >= 0) {
        char num[8];
        int ni = 0;
        int n = mount_result;
        if (n == 0) num[ni++] = '0';
        else while (n > 0) { num[ni++] = '0' + (n % 10); n /= 10; }
        for (int i = ni - 1; i >= 0; i--) {
            serial_putchar(num[i]);
        }
    } else {
        serial_putchar('-');
        char num[8];
        int ni = 0;
        int n = -mount_result;
        if (n == 0) num[ni++] = '0';
        else while (n > 0) { num[ni++] = '0' + (n % 10); n /= 10; }
        for (int i = ni - 1; i >= 0; i--) {
            serial_putchar(num[i]);
        }
    }
    serial_putchar('\n');
    
    log_print("shell_start: step1 - before if(mount_result)\n");
    
    if (mount_result == 0) {
        log_print("shell_start: step2 - before shell_print_color OK\n");
        shell_print_color("OK\n", COLOR_GREEN);
        log_print("shell_start: step3 - after shell_print_color OK\n");
    } else {
        shell_print_color("FAILED\n", COLOR_RED);
        shell_print("Use 'mount hda / fat32' to mount manually.\n\n");
    }
    
    log_print("shell_start: step4 - before while(1)\n");
    
    while (1) {
        log_print("shell_start: step5 - before shell_print_color prompt\n");
        shell_print_color(current_dir, COLOR_BLUE);
        shell_print(" ");
        shell_print_color(SHELL_PROMPT, COLOR_GREEN);
        log_print("shell_start: step6 - after shell_print_color prompt\n");
        shell_print(" ");
        
        int cmd_len = 0;
        while (cmd_len < SHELL_MAX_CMD - 1) {
            char c = shell_read_char();
            
            if (c == '\n' || c == '\r') {
                shell_newline();
                cmd_buffer[cmd_len] = '\0';
                break;
            } else if (c == '\b' || c == 0x7F) {
                if (cmd_len > 0) {
                    cmd_len--;
                    cursor_x--;
                    font_draw_char(' ', cursor_x * FONT_WIDTH, cursor_y * FONT_HEIGHT, COLOR_BLACK, COLOR_BLACK);
                }
            } else if (c >= 0x20 && c <= 0x7E) {
                cmd_buffer[cmd_len++] = c;
                shell_draw_char(c);
            }
        }
        
        if (cmd_len > 0) {
            cmd_buffer[cmd_len] = '\0';
            shell_execute(cmd_buffer);
        }
        
        shell_newline();
    }
}