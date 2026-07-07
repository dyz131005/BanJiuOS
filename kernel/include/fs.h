#ifndef FS_H
#define FS_H

#include <types.h>

#define FS_MAX_MOUNTS 16
#define FS_MAX_PATH 4096

typedef enum {
    FS_TYPE_UNKNOWN,
    FS_TYPE_FAT32,
    FS_TYPE_EXT4,
    FS_TYPE_NTFS
} FsType;

typedef struct FsNode FsNode;

struct FsNode {
    char name[256];
    u64 inode;
    u32 mode;
    u64 size;
    u64 atime;
    u64 mtime;
    u64 ctime;
    FsNode* parent;
    FsNode* children;
    FsNode* next;
    void* fs_specific;
};

typedef struct {
    const char* name;
    FsType type;
    int (*mount)(const char* device, const char* mount_point);
    int (*unmount)(const char* mount_point);
    FsNode* (*open)(const char* path);
    int (*read)(FsNode* node, u64 offset, u64 size, void* buffer);
    int (*write)(FsNode* node, u64 offset, u64 size, const void* buffer);
    int (*close)(FsNode* node);
    int (*create)(const char* path, u32 mode);
    int (*remove)(const char* path);
    int (*mkdir)(const char* path, u32 mode);
    int (*rmdir)(const char* path);
} FileSystem;

typedef struct {
    char device[256];
    char mount_point[256];
    FsType type;
    FileSystem* fs;
    void* data;
} Mount;

void fs_init(void);
int fs_register(FileSystem* fs);
int fs_mount(const char* device, const char* mount_point, FsType type);
int fs_unmount(const char* mount_point);
FsNode* fs_open(const char* path);
int fs_read(FsNode* node, u64 offset, u64 size, void* buffer);
int fs_write(FsNode* node, u64 offset, u64 size, const void* buffer);
int fs_close(FsNode* node);
int fs_create(const char* path, u32 mode);
int fs_remove(const char* path);
int fs_mkdir(const char* path, u32 mode);
int fs_rmdir(const char* path);

#endif