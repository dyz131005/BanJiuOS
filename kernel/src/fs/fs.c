#include <fs.h>
#include <types.h>
#include <mm.h>
#include <string.h>

#define COM1_BASE 0x3F8

static inline u8 inb(u16 port) {
    u8 val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outb(u16 port, u8 val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void serial_putchar(char c) {
    while (!(inb(COM1_BASE + 5) & 0x20)) {
        asm volatile("nop");
    }
    outb(COM1_BASE, c);
}

static inline void fs_log(const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}

static Mount mounts[FS_MAX_MOUNTS];
static FileSystem* registered_fs[FS_MAX_MOUNTS];
static u32 mount_count = 0;
static u32 fs_count = 0;

void fs_init(void) {
    fs_log("fs_init: started\n");
    
    fs_log("fs_init: clearing mounts array\n");
    for (u32 i = 0; i < FS_MAX_MOUNTS; i++) {
        mounts[i].device[0] = '\0';
        mounts[i].mount_point[0] = '\0';
        mounts[i].type = FS_TYPE_UNKNOWN;
        mounts[i].fs = NULL;
        mounts[i].data = NULL;
    }
    
    fs_log("fs_init: clearing registered_fs array\n");
    for (u32 i = 0; i < FS_MAX_MOUNTS; i++) {
        registered_fs[i] = NULL;
    }
    
    mount_count = 0;
    fs_count = 0;
    
    fs_log("fs_init: done - mount_count=0, fs_count=0\n");
}

int fs_register(FileSystem* fs) {
    fs_log("fs_register: started\n");
    
    if (!fs || fs_count >= FS_MAX_MOUNTS) {
        fs_log("fs_register: invalid fs or full, returning -1\n");
        return -1;
    }
    
    for (u32 i = 0; i < fs_count; i++) {
        if (registered_fs[i] == fs || 
            registered_fs[i]->type == fs->type) {
            fs_log("fs_register: fs already registered, returning -2\n");
            return -2;
        }
    }
    
    registered_fs[fs_count++] = fs;
    
    fs_log("fs_register: done - registered fs type ");
    char type_str[8];
    switch(fs->type) {
        case FS_TYPE_FAT32: fs_log("FAT32\n"); break;
        case FS_TYPE_EXT4: fs_log("EXT4\n"); break;
        default: fs_log("UNKNOWN\n"); break;
    }
    
    return 0;
}

int fs_mount(const char* device, const char* mount_point, FsType type) {
    fs_log("fs_mount: started\n");
    fs_log("fs_mount: device='");
    fs_log(device ? device : "(null)");
    fs_log("', mount_point='");
    fs_log(mount_point ? mount_point : "(null)");
    fs_log("', type=");
    switch(type) {
        case FS_TYPE_FAT32: fs_log("FAT32\n"); break;
        case FS_TYPE_EXT4: fs_log("EXT4\n"); break;
        default: fs_log("UNKNOWN\n"); break;
    }
    
    fs_log("fs_mount: checking mount_count (max=");
    char max_str[8];
    int mi = 0;
    u32 m = FS_MAX_MOUNTS;
    while (m > 0) { max_str[mi++] = '0' + (m % 10); m /= 10; }
    for (int i = mi - 1; i >= 0; i--) serial_putchar(max_str[i]);
    fs_log(")\n");
    
    if (mount_count >= FS_MAX_MOUNTS) {
        fs_log("fs_mount: mount table full, returning -1\n");
        return -1;
    }
    
    fs_log("fs_mount: searching for registered filesystem (fs_count=");
    char fc_str[8];
    int fci = 0;
    u32 fc = fs_count;
    if (fc == 0) fc_str[fci++] = '0';
    else while (fc > 0) { fc_str[fci++] = '0' + (fc % 10); fc /= 10; }
    for (int i = fci - 1; i >= 0; i--) serial_putchar(fc_str[i]);
    fs_log(")\n");
    
    FileSystem* fs = NULL;
    for (u32 i = 0; i < fs_count; i++) {
        fs_log("fs_mount: checking fs[");
        char idx_str[4];
        int ii = 0;
        u32 idx = i;
        if (idx == 0) idx_str[ii++] = '0';
        else while (idx > 0) { idx_str[ii++] = '0' + (idx % 10); idx /= 10; }
        for (int j = ii - 1; j >= 0; j--) serial_putchar(idx_str[j]);
        fs_log("] type=");
        switch(registered_fs[i]->type) {
            case FS_TYPE_FAT32: fs_log("FAT32"); break;
            case FS_TYPE_EXT4: fs_log("EXT4"); break;
            default: fs_log("UNKNOWN"); break;
        }
        fs_log("\n");
        
        if (registered_fs[i]->type == type) {
            fs = registered_fs[i];
            fs_log("fs_mount: found matching filesystem\n");
            break;
        }
    }
    
    if (!fs) {
        fs_log("fs_mount: filesystem not found, returning -2\n");
        return -2;
    }
    
    fs_log("fs_mount: calling fs->mount() method\n");
    int result = fs->mount(device, mount_point);
    fs_log("fs_mount: fs->mount() returned ");
    char res_str[8];
    int ri = 0;
    int r = result;
    if (r < 0) { res_str[ri++] = '-'; r = -r; }
    if (r == 0) res_str[ri++] = '0';
    else while (r > 0) { res_str[ri++] = '0' + (r % 10); r /= 10; }
    for (int i = ri - 1; i >= 0; i--) serial_putchar(res_str[i]);
    fs_log("\n");
    
    if (result != 0) {
        fs_log("fs_mount: mount failed, returning error\n");
        return result;
    }
    
    fs_log("fs_mount: saving mount info\n");
    Mount* mount = &mounts[mount_count];
    
    for (size_t i = 0; i < 256 && device[i]; i++) {
        mount->device[i] = device[i];
    }
    mount->device[255] = '\0';
    
    for (size_t i = 0; i < 256 && mount_point[i]; i++) {
        mount->mount_point[i] = mount_point[i];
    }
    mount->mount_point[255] = '\0';
    
    mount->type = type;
    mount->fs = fs;
    mount->data = NULL;
    
    mount_count++;
    
    fs_log("fs_mount: done - mount_count=");
    char mc_str[8];
    int mci = 0;
    u32 mc = mount_count;
    if (mc == 0) mc_str[mci++] = '0';
    else while (mc > 0) { mc_str[mci++] = '0' + (mc % 10); mc /= 10; }
    for (int i = mci - 1; i >= 0; i--) serial_putchar(mc_str[i]);
    fs_log("\n");
    
    return 0;
}

int fs_unmount(const char* mount_point) {
    for (u32 i = 0; i < mount_count; i++) {
        if (__builtin_strcmp(mounts[i].mount_point, mount_point) == 0) {
            int result = mounts[i].fs->unmount(mount_point);
            if (result != 0) {
                return result;
            }
            
            for (u32 j = i; j < mount_count - 1; j++) {
                mounts[j] = mounts[j + 1];
            }
            
            mount_count--;
            
            return 0;
        }
    }
    
    return -1;
}

FsNode* fs_open(const char* path) {
    for (u32 i = 0; i < mount_count; i++) {
        size_t len = __builtin_strlen(mounts[i].mount_point);
        if (__builtin_strncmp(path, mounts[i].mount_point, len) == 0) {
            const char* rel_path = path + len;
            if (*rel_path == '/') {
                rel_path++;
            }
            
            return mounts[i].fs->open(rel_path);
        }
    }
    
    return NULL;
}

int fs_read(FsNode* node, u64 offset, u64 size, void* buffer) {
    if (!node) {
        return -1;
    }
    
    return -1;
}

int fs_write(FsNode* node, u64 offset, u64 size, const void* buffer) {
    if (!node) {
        return -1;
    }
    
    return -1;
}

int fs_close(FsNode* node) {
    if (!node) {
        return -1;
    }
    
    return 0;
}

int fs_create(const char* path, u32 mode) {
    for (int i = 0; i < mount_count; i++) {
        Mount* m = &mounts[i];
        if (m->fs && m->fs->create) {
            return m->fs->create(path, mode);
        }
    }
    return -1;
}

int fs_remove(const char* path) {
    for (int i = 0; i < mount_count; i++) {
        Mount* m = &mounts[i];
        if (m->fs && m->fs->remove) {
            return m->fs->remove(path);
        }
    }
    return -1;
}

int fs_mkdir(const char* path, u32 mode) {
    for (int i = 0; i < mount_count; i++) {
        Mount* m = &mounts[i];
        if (m->fs && m->fs->mkdir) {
            return m->fs->mkdir(path, mode);
        }
    }
    return -1;
}

int fs_rmdir(const char* path) {
    for (int i = 0; i < mount_count; i++) {
        Mount* m = &mounts[i];
        if (m->fs && m->fs->rmdir) {
            return m->fs->rmdir(path);
        }
    }
    return -1;
}