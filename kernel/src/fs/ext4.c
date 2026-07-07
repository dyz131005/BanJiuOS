#include <fs.h>
#include <types.h>
#include <mm.h>

#define EXT4_SUPER_MAGIC 0xEF53

typedef struct {
    u32 inodes_count;
    u32 blocks_count;
    u32 r_blocks_count;
    u32 free_blocks_count;
    u32 free_inodes_count;
    u32 first_data_block;
    u32 log_block_size;
    u32 log_cluster_size;
    u32 blocks_per_group;
    u32 clusters_per_group;
    u32 inodes_per_group;
    u32 mtime;
    u32 wtime;
    u16 mnt_count;
    u16 max_mnt_count;
    u16 magic;
    u16 state;
    u16 errors;
    u16 minor_rev_level;
    u32 lastcheck;
    u32 checkinterval;
    u32 creator_os;
    u32 rev_level;
    u16 def_resuid;
    u16 def_resgid;
    u32 first_ino;
    u16 inode_size;
    u16 block_group_nr;
    u32 feature_compat;
    u32 feature_incompat;
    u32 feature_ro_compat;
    u8 uuid[16];
    char volume_name[16];
    char last_mounted[64];
    u32 algorithm_usage_bitmap;
    u8 prealloc_blocks;
    u8 prealloc_dir_blocks;
    u16 reserved_gdt_blocks;
    u8 journal_uuid[16];
    u32 journal_inum;
    u32 journal_dev;
    u32 last_orphan;
    u32 hash_seed[4];
    u8 def_hash_version;
    u8 reserved_char_pad;
    u16 reserved_word_pad;
    u32 default_mount_options;
    u32 first_meta_bg;
    u32 mkfs_time;
    u32 jnl_blocks[17];
} Ext4SuperBlock;

typedef struct {
    u32 block_bitmap;
    u32 inode_bitmap;
    u32 inode_table;
    u16 free_blocks_count;
    u16 free_inodes_count;
    u16 used_dirs_count;
    u16 pad;
    u32 reserved[3];
} Ext4BlockGroupDescriptor;

typedef struct {
    u16 mode;
    u16 uid;
    u32 size;
    u32 atime;
    u32 ctime;
    u32 mtime;
    u32 dtime;
    u16 gid;
    u16 links_count;
    u32 blocks;
    u32 flags;
    u32 osd1;
    u32 block[15];
    u32 generation;
    u32 file_acl;
    u32 dir_acl;
    u32 fragment;
    u32 osd2[3];
} Ext4Inode;

static Ext4SuperBlock* ext4_sb = NULL;
static Ext4BlockGroupDescriptor* ext4_bgdt = NULL;
static u8* ext4_block_buffer = NULL;
static u32 ext4_block_size = 1024;

static int ext4_read_block(u32 block_num, void* buffer) {
    return -1;
}

static Ext4Inode* ext4_read_inode(u32 ino) {
    if (!ext4_sb || !ext4_bgdt) {
        return NULL;
    }
    
    u32 blocks_per_group = ext4_sb->blocks_per_group;
    u32 inodes_per_group = ext4_sb->inodes_per_group;
    
    u32 group = (ino - 1) / inodes_per_group;
    u32 index = (ino - 1) % inodes_per_group;
    
    if (group >= (ext4_sb->blocks_count + blocks_per_group - 1) / blocks_per_group) {
        return NULL;
    }
    
    Ext4BlockGroupDescriptor* bgd = &ext4_bgdt[group];
    
    u32 inode_table_block = bgd->inode_table;
    u32 inode_size = ext4_sb->inode_size;
    
    u32 block_offset = (index * inode_size) / ext4_block_size;
    u32 offset_in_block = (index * inode_size) % ext4_block_size;
    
    if (!ext4_block_buffer) {
        ext4_block_buffer = kmalloc(ext4_block_size);
    }
    
    if (ext4_read_block(inode_table_block + block_offset, ext4_block_buffer) != 0) {
        return NULL;
    }
    
    Ext4Inode* inode = kmalloc(inode_size);
    if (!inode) {
        return NULL;
    }
    
    for (u32 i = 0; i < inode_size; i++) {
        ((u8*)inode)[i] = ext4_block_buffer[offset_in_block + i];
    }
    
    return inode;
}

static int ext4_mount(const char* device, const char* mount_point) {
    if (!ext4_block_buffer) {
        ext4_block_buffer = kmalloc(4096);
    }
    
    if (ext4_read_block(1, ext4_block_buffer) != 0) {
        return -1;
    }
    
    ext4_sb = (Ext4SuperBlock*)ext4_block_buffer;
    
    if (ext4_sb->magic != EXT4_SUPER_MAGIC) {
        return -2;
    }
    
    ext4_block_size = 1024 << ext4_sb->log_block_size;
    
    u32 blocks_per_group = ext4_sb->blocks_per_group;
    u32 num_groups = (ext4_sb->blocks_count + blocks_per_group - 1) / blocks_per_group;
    
    ext4_bgdt = kmalloc(num_groups * sizeof(Ext4BlockGroupDescriptor));
    
    u32 bgdt_block = 2;
    u32 bgdt_size = num_groups * sizeof(Ext4BlockGroupDescriptor);
    u32 bgdt_blocks = (bgdt_size + ext4_block_size - 1) / ext4_block_size;
    
    for (u32 i = 0; i < bgdt_blocks; i++) {
        if (ext4_read_block(bgdt_block + i, (u8*)ext4_bgdt + i * ext4_block_size) != 0) {
            return -3;
        }
    }
    
    return 0;
}

static int ext4_unmount(const char* mount_point) {
    if (ext4_bgdt) {
        kfree(ext4_bgdt);
        ext4_bgdt = NULL;
    }
    
    if (ext4_block_buffer) {
        kfree(ext4_block_buffer);
        ext4_block_buffer = NULL;
    }
    
    ext4_sb = NULL;
    
    return 0;
}

static FsNode* ext4_open(const char* path) {
    return NULL;
}

static int ext4_read(FsNode* node, u64 offset, u64 size, void* buffer) {
    return -1;
}

static int ext4_write(FsNode* node, u64 offset, u64 size, const void* buffer) {
    return -1;
}

static int ext4_close(FsNode* node) {
    return 0;
}

static int ext4_create(const char* path, u32 mode) {
    return -1;
}

static int ext4_remove(const char* path) {
    return -1;
}

static int ext4_mkdir(const char* path, u32 mode) {
    return -1;
}

static int ext4_rmdir(const char* path) {
    return -1;
}

FileSystem ext4_fs = {
    .name = "ext4",
    .type = FS_TYPE_EXT4,
    .mount = ext4_mount,
    .unmount = ext4_unmount,
    .open = ext4_open,
    .read = ext4_read,
    .write = ext4_write,
    .close = ext4_close,
    .create = ext4_create,
    .remove = ext4_remove,
    .mkdir = ext4_mkdir,
    .rmdir = ext4_rmdir
};