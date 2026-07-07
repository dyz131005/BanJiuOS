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

static inline void outl(u16 port, u32 val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline void serial_putchar(char c) {
    while (!(inb(COM1_BASE + 5) & 0x20)) {
        asm volatile("nop");
    }
    outb(COM1_BASE, c);
}

static inline void serial_log(const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}

static inline u16 inw(u16 port) {
    u16 val;
    asm volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

#define FAT32_SIGNATURE 0x28
#define FAT32_SIGNATURE_ALT 0x29

typedef struct {
    u8 jmp_boot[3];
    u8 oem_name[8];
    u16 bytes_per_sector;
    u8 sectors_per_cluster;
    u16 reserved_sectors;
    u8 fat_count;
    u16 root_entries;
    u16 total_sectors_16;
    u8 media_type;
    u16 sectors_per_fat_16;
    u16 sectors_per_track;
    u16 heads;
    u32 hidden_sectors;
    u32 total_sectors_32;
    
    u32 sectors_per_fat_32;
    u16 extended_flags;
    u16 fs_version;
    u32 root_cluster;
    u16 fs_info_sector;
    u16 backup_boot_sector;
    u8 reserved[12];
    u8 drive_number;
    u8 reserved1;
    u8 boot_signature;
    u32 volume_id;
    u8 volume_label[11];
    u8 fs_type[8];
    u8 boot_code[420];
    u16 boot_sector_signature;
} __attribute__((packed)) Fat32BootSector;

typedef struct {
    u8 filename[8];
    u8 extension[3];
    u8 attributes;
    u8 reserved;
    u8 create_time_tenths;
    u16 create_time;
    u16 create_date;
    u16 last_access_date;
    u16 first_cluster_high;
    u16 last_modified_time;
    u16 last_modified_date;
    u16 first_cluster_low;
    u32 file_size;
} __attribute__((packed)) Fat32DirectoryEntry;

static Fat32BootSector fat32_bs_data = {0};
static Fat32BootSector* fat32_bs = &fat32_bs_data;
static u8* fat32_buffer = NULL;
static u32 fat32_sector_size = 512;
static u32 fat32_cluster_size = 4096;
static u32 fat32_sectors_per_cluster = 1;
static u32 fat32_fat_start = 0;
static u32 fat32_data_start = 0;
static u32 fat32_root_cluster = 2;

static int fat32_read_sector(u32 sector_num, void* buffer);

typedef struct {
    u8 signature[8];
    u32 header_size;
    u32 version;
    u32 crc32;
    u32 reserved;
    u64 current_lba;
    u64 backup_lba;
    u64 first_usable_lba;
    u64 last_usable_lba;
    u8 disk_guid[16];
    u64 partition_table_lba;
    u32 num_partitions;
    u32 partition_entry_size;
    u32 partition_table_crc32;
} __attribute__((packed)) GptHeader;

typedef struct {
    u8 type_guid[16];
    u8 partition_guid[16];
    u64 start_lba;
    u64 end_lba;
    u64 attributes;
    u16 name[36];
} __attribute__((packed)) GptPartitionEntry;

static u64 fat32_partition_start_lba = 0;

static int fat32_parse_gpt() {
    serial_log("fat32_parse_gpt: started\n");
    
    if (fat32_read_sector(1, fat32_buffer) != 0) {
        serial_log("fat32_parse_gpt: failed to read GPT header\n");
        return -1;
    }
    
    GptHeader* gpt = (GptHeader*)fat32_buffer;
    
    if (memcmp(gpt->signature, "EFI PART", 8) != 0) {
        serial_log("fat32_parse_gpt: invalid GPT signature\n");
        return -2;
    }
    
    serial_log("fat32_parse_gpt: GPT header OK\n");
    serial_log("fat32_parse_gpt: num_partitions=");
    char np[8];
    int npi = 0;
    u32 npv = gpt->num_partitions;
    if (npv == 0) np[npi++] = '0';
    else while (npv > 0) { np[npi++] = '0' + (npv % 10); npv /= 10; }
    for (int i = npi - 1; i >= 0; i--) serial_putchar(np[i]);
    serial_log("\n");
    
    for (u32 i = 0; i < gpt->num_partitions; i++) {
        u64 entry_lba = gpt->partition_table_lba + (i * gpt->partition_entry_size) / 512;
        u32 entry_offset = (i * gpt->partition_entry_size) % 512;
        
        if (fat32_read_sector((u32)entry_lba, fat32_buffer) != 0) {
            serial_log("fat32_parse_gpt: failed to read partition entry\n");
            continue;
        }
        
        GptPartitionEntry* entry = (GptPartitionEntry*)(fat32_buffer + entry_offset);
        
        if (entry->type_guid[3] == 0xEB && entry->type_guid[2] == 0xD0 && 
            entry->type_guid[1] == 0xA0 && entry->type_guid[0] == 0xA2) {
            
            serial_log("fat32_parse_gpt: found FAT32 partition\n");
            fat32_partition_start_lba = entry->start_lba;
            serial_log("fat32_parse_gpt: start_lba=");
            char sl[20];
            int sli = 0;
            u64 slv = entry->start_lba;
            if (slv == 0) sl[sli++] = '0';
            else while (slv > 0) { sl[sli++] = '0' + (slv % 10); slv /= 10; }
            for (int i = sli - 1; i >= 0; i--) serial_putchar(sl[i]);
            serial_log("\n");
            return 0;
        }
    }
    
    serial_log("fat32_parse_gpt: no FAT32 partition found\n");
    return -3;
}

static int fat32_detect_disk(void) {
    serial_log("fat32_detect_disk: started\n");
    
    serial_log("fat32_detect_disk: sending IDENTIFY\n");
    outb(0x1F6, 0xE0);
    outb(0x1F7, 0xEC);
    
    u32 timeout = 0;
    serial_log("fat32_detect_disk: waiting for DRDY\n");
    while (!(inb(0x1F7) & 0x08)) {
        timeout++;
        if (timeout > 100000) {
            serial_log("fat32_detect_disk: timeout waiting for DRDY\n");
            return -1;
        }
        asm volatile("nop");
    }
    
    u8 status = inb(0x1F7);
    if (status & 0x01) {
        serial_log("fat32_detect_disk: error in status\n");
        return -2;
    }
    
    serial_log("fat32_detect_disk: reading IDENTIFY data\n");
    u16 data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = inw(0x1F0);
    }
    
    serial_log("fat32_detect_disk: done\n");
    return 0;
}

static int fat32_read_sector(u32 sector_num, void* buffer) {
    serial_log("fat32_read_sector: sector ");
    char num[16];
    int ni = 0;
    u32 s = sector_num;
    if (s == 0) num[ni++] = '0';
    else while (s > 0) { num[ni++] = '0' + (s % 10); s /= 10; }
    for (int i = ni - 1; i >= 0; i--) serial_putchar(num[i]);
    serial_log("\n");
    
    outb(0x1F6, 0xE0 | ((sector_num >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, sector_num & 0xFF);
    outb(0x1F4, (sector_num >> 8) & 0xFF);
    outb(0x1F5, (sector_num >> 16) & 0xFF);
    outb(0x1F7, 0x20);
    
    serial_log("fat32_read_sector: waiting for DRDY\n");
    u32 timeout = 0;
    while (!(inb(0x1F7) & 0x08)) {
        timeout++;
        if (timeout > 100000) {
            serial_log("fat32_read_sector: timeout\n");
            return -1;
        }
        asm volatile("nop");
    }
    
    u8 status = inb(0x1F7);
    if (status & 0x01) {
        serial_log("fat32_read_sector: error\n");
        return -2;
    }
    
    serial_log("fat32_read_sector: reading data\n");
    for (int i = 0; i < 256; i++) {
        ((u16*)buffer)[i] = inw(0x1F0);
    }
    
    serial_log("fat32_read_sector: done\n");
    return 0;
}

static int fat32_read_cluster(u32 cluster_num, void* buffer) {
    if (!buffer) {
        return -1;
    }
    
    u32 first_sector = (u32)fat32_partition_start_lba + fat32_data_start + (cluster_num - 2) * fat32_sectors_per_cluster;
    
    for (u32 i = 0; i < fat32_sectors_per_cluster; i++) {
        if (fat32_read_sector(first_sector + i, buffer + i * fat32_sector_size) != 0) {
            return -1;
        }
    }
    
    return 0;
}

static u32 fat32_get_next_cluster(u32 cluster) {
    if (!fat32_buffer) {
        return 0;
    }
    
    if (cluster == 0 || cluster == 1) {
        return 0;
    }
    
    u32 fat_offset = cluster * 4;
    u32 fat_sector = (u32)fat32_partition_start_lba + fat32_fat_start + (fat_offset / fat32_sector_size);
    u32 offset_in_sector = fat_offset % fat32_sector_size;
    
    if (fat32_read_sector(fat_sector, fat32_buffer) != 0) {
        return 0;
    }
    
    u32 next_cluster = *(u32*)(fat32_buffer + offset_in_sector);
    next_cluster &= 0x0FFFFFFF;
    
    if (next_cluster >= 0x0FFFFFF8 || next_cluster == 0 || next_cluster == 1) {
        return 0;
    }
    
    return next_cluster;
}

static int fat32_write_sector(u32 sector_num, void* buffer);

static u32 fat32_allocate_cluster(void) {
    if (!fat32_buffer) {
        return 0;
    }
    
    u32 fat_sectors = fat32_bs->sectors_per_fat_32;
    
    for (u32 sector = 0; sector < fat_sectors; sector++) {
        u32 fat_sector_num = (u32)fat32_partition_start_lba + fat32_fat_start + sector;
        
        if (fat32_read_sector(fat_sector_num, fat32_buffer) != 0) {
            continue;
        }
        
        for (u32 i = 0; i < fat32_sector_size / 4; i++) {
            u32 entry = *(u32*)(fat32_buffer + i * 4);
            entry &= 0x0FFFFFFF;
            
            if (entry == 0) {
                u32 cluster = sector * (fat32_sector_size / 4) + i;
                
                *(u32*)(fat32_buffer + i * 4) = 0x0FFFFFFF;
                
                if (fat32_write_sector(fat_sector_num, fat32_buffer) != 0) {
                    return 0;
                }
                
                if (fat32_bs->fat_count > 1) {
                    u32 mirror_sector = fat_sector_num + fat_sectors;
                    if (fat32_read_sector(mirror_sector, fat32_buffer) == 0) {
                        *(u32*)(fat32_buffer + i * 4) = 0x0FFFFFFF;
                        fat32_write_sector(mirror_sector, fat32_buffer);
                    }
                }
                
                return cluster;
            }
        }
    }
    
    return 0;
}

static int fat32_write_fat_entry(u32 cluster, u32 value) {
    if (!fat32_buffer) {
        return -1;
    }
    
    char cl[20];
    sprintf(cl, "%x", cluster);
    serial_log("fat32_write_fat_entry: cluster=");
    serial_log(cl);
    serial_log(", value=");
    char vl[20];
    sprintf(vl, "%x", value);
    serial_log(vl);
    serial_log("\n");
    
    u32 fat_offset = cluster * 4;
    u32 fat_sector = (u32)fat32_partition_start_lba + fat32_fat_start + (fat_offset / fat32_sector_size);
    u32 offset_in_sector = fat_offset % fat32_sector_size;
    
    char fs[20];
    sprintf(fs, "%x", fat_sector);
    serial_log("fat32_write_fat_entry: fat_sector=");
    serial_log(fs);
    serial_log("\n");
    
    if (fat32_read_sector(fat_sector, fat32_buffer) != 0) {
        return -1;
    }
    
    *(u32*)(fat32_buffer + offset_in_sector) = value;
    
    if (fat32_write_sector(fat_sector, fat32_buffer) != 0) {
        return -1;
    }
    
    if (fat32_bs->fat_count > 1) {
        u32 mirror_sector = fat_sector + fat32_bs->sectors_per_fat_32;
        if (fat32_read_sector(mirror_sector, fat32_buffer) == 0) {
            *(u32*)(fat32_buffer + offset_in_sector) = value;
            fat32_write_sector(mirror_sector, fat32_buffer);
        }
    }
    
    return 0;
}

static int fat32_write_sector(u32 sector_num, void* buffer) {
    serial_log("fat32_write_sector: sector ");
    char hex_buf[20];
    sprintf(hex_buf, "%x", sector_num);
    serial_log(hex_buf);
    serial_log("\n");
    
    outb(0x1F6, 0xE0 | ((sector_num >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, sector_num & 0xFF);
    outb(0x1F4, (sector_num >> 8) & 0xFF);
    outb(0x1F5, (sector_num >> 16) & 0xFF);
    outb(0x1F7, 0x30);
    
    serial_log("fat32_write_sector: waiting for BSY\n");
    u32 timeout = 0;
    while (inb(0x1F7) & 0x80) {
        timeout++;
        if (timeout > 100000) {
            serial_log("fat32_write_sector: timeout\n");
            return -1;
        }
        asm volatile("nop");
    }
    
    u8 status = inb(0x1F7);
    if (status & 0x01) {
        serial_log("fat32_write_sector: error\n");
        return -2;
    }
    
    serial_log("fat32_write_sector: writing data\n");
    for (u32 i = 0; i < fat32_sector_size / 4; i++) {
        outl(0x1F0, *(u32*)((u8*)buffer + i * 4));
    }
    
    serial_log("fat32_write_sector: waiting for write complete\n");
    timeout = 0;
    while (inb(0x1F7) & 0x80) {
        timeout++;
        if (timeout > 100000) {
            serial_log("fat32_write_sector: timeout\n");
            return -1;
        }
        asm volatile("nop");
    }
    
    serial_log("fat32_write_sector: done\n");
    return 0;
}

static int fat32_mount(const char* device, const char* mount_point) {
    serial_log("fat32_mount: started\n");
    
    if (!fat32_buffer) {
        serial_log("fat32_mount: allocating buffer\n");
        fat32_buffer = kmalloc(4096);
        if (!fat32_buffer) {
            serial_log("fat32_mount: kmalloc failed\n");
            return -1;
        }
    }
    
    serial_log("fat32_mount: detecting disk\n");
    if (fat32_detect_disk() != 0) {
        serial_log("fat32_mount: disk detect failed\n");
        return -3;
    }
    
    serial_log("fat32_mount: parsing GPT partition table\n");
    int gpt_result = fat32_parse_gpt();
    if (gpt_result != 0) {
        serial_log("fat32_mount: GPT parse failed, trying MBR\n");
        fat32_partition_start_lba = 0;
    }
    
    serial_log("fat32_mount: reading boot sector\n");
    if (fat32_read_sector((u32)fat32_partition_start_lba, fat32_buffer) != 0) {
        serial_log("fat32_mount: read sector failed\n");
        return -1;
    }
    
    memcpy(&fat32_bs_data, fat32_buffer, sizeof(Fat32BootSector));
    
    serial_log("fat32_mount: checking signature\n");
    serial_log("fat32_mount: boot_signature=0x");
    char sig1[4];
    int si1 = 0;
    u8 s1 = fat32_bs->boot_signature;
    sig1[si1++] = "0123456789ABCDEF"[(s1 >> 4) & 0xF];
    sig1[si1++] = "0123456789ABCDEF"[s1 & 0xF];
    for (int i = 0; i < si1; i++) serial_putchar(sig1[i]);
    serial_log("\n");
    serial_log("fat32_mount: boot_sector_signature=0x");
    char sig2[6];
    int si2 = 0;
    u16 s2 = fat32_bs->boot_sector_signature;
    sig2[si2++] = "0123456789ABCDEF"[(s2 >> 12) & 0xF];
    sig2[si2++] = "0123456789ABCDEF"[(s2 >> 8) & 0xF];
    sig2[si2++] = "0123456789ABCDEF"[(s2 >> 4) & 0xF];
    sig2[si2++] = "0123456789ABCDEF"[s2 & 0xF];
    for (int i = 0; i < si2; i++) serial_putchar(sig2[i]);
    serial_log("\n");
    
    if ((fat32_bs->boot_signature != FAT32_SIGNATURE && 
         fat32_bs->boot_signature != FAT32_SIGNATURE_ALT) &&
        fat32_bs->boot_sector_signature != 0xAA55) {
        serial_log("fat32_mount: signature mismatch, returning -2\n");
        return -2;
    }
    serial_log("fat32_mount: signature OK\n");
    
    serial_log("fat32_mount: BPB info:\n");
    serial_log("  bytes_per_sector=");
    char bps[8];
    int bpsi = 0;
    u16 bpsv = fat32_bs->bytes_per_sector;
    if (bpsv == 0) bps[bpsi++] = '0';
    else while (bpsv > 0) { bps[bpsi++] = '0' + (bpsv % 10); bpsv /= 10; }
    for (int i = bpsi - 1; i >= 0; i--) serial_putchar(bps[i]);
    serial_log("\n");
    
    serial_log("  sectors_per_cluster=");
    char spc[8];
    int spci = 0;
    u8 spcv = fat32_bs->sectors_per_cluster;
    if (spcv == 0) spc[spci++] = '0';
    else while (spcv > 0) { spc[spci++] = '0' + (spcv % 10); spcv /= 10; }
    for (int i = spci - 1; i >= 0; i--) serial_putchar(spc[i]);
    serial_log("\n");
    
    serial_log("  reserved_sectors=");
    char rs[8];
    int rsi = 0;
    u16 rsv = fat32_bs->reserved_sectors;
    if (rsv == 0) rs[rsi++] = '0';
    else while (rsv > 0) { rs[rsi++] = '0' + (rsv % 10); rsv /= 10; }
    for (int i = rsi - 1; i >= 0; i--) serial_putchar(rs[i]);
    serial_log("\n");
    
    serial_log("  fat_count=");
    char fc[8];
    int fci = 0;
    u8 fcv = fat32_bs->fat_count;
    if (fcv == 0) fc[fci++] = '0';
    else while (fcv > 0) { fc[fci++] = '0' + (fcv % 10); fcv /= 10; }
    for (int i = fci - 1; i >= 0; i--) serial_putchar(fc[i]);
    serial_log("\n");
    
    serial_log("  sectors_per_fat_32=");
    char spf[12];
    int spfi = 0;
    u32 spfv = fat32_bs->sectors_per_fat_32;
    if (spfv == 0) spf[spfi++] = '0';
    else while (spfv > 0) { spf[spfi++] = '0' + (spfv % 10); spfv /= 10; }
    for (int i = spfi - 1; i >= 0; i--) serial_putchar(spf[i]);
    serial_log("\n");
    
    serial_log("  root_cluster=");
    char rc[12];
    int rci = 0;
    u32 rcv = fat32_bs->root_cluster;
    if (rcv == 0) rc[rci++] = '0';
    else while (rcv > 0) { rc[rci++] = '0' + (rcv % 10); rcv /= 10; }
    for (int i = rci - 1; i >= 0; i--) serial_putchar(rc[i]);
    serial_log("\n");
    
    if (fat32_bs->bytes_per_sector == 0 || fat32_bs->sectors_per_cluster == 0 || 
        fat32_bs->root_cluster == 0) {
        serial_log("fat32_mount: BPB invalid, using defaults\n");
        fat32_sector_size = 512;
        fat32_sectors_per_cluster = 8;
        fat32_cluster_size = fat32_sector_size * fat32_sectors_per_cluster;
        fat32_fat_start = 32;
        fat32_data_start = fat32_fat_start + 2 * 4096;
        fat32_root_cluster = 2;
    } else {
        fat32_sector_size = fat32_bs->bytes_per_sector;
        fat32_sectors_per_cluster = fat32_bs->sectors_per_cluster;
        fat32_cluster_size = fat32_sector_size * fat32_sectors_per_cluster;
        fat32_fat_start = fat32_bs->reserved_sectors;
        fat32_data_start = fat32_fat_start + fat32_bs->fat_count * fat32_bs->sectors_per_fat_32;
        fat32_root_cluster = fat32_bs->root_cluster;
    }
    
    return 0;
}

static int fat32_unmount(const char* mount_point) {
    if (fat32_buffer) {
        kfree(fat32_buffer);
        fat32_buffer = NULL;
    }
    
    fat32_bs = NULL;
    
    return 0;
}

static void fat32_trim_name(char* name, int max_len) {
    int len = max_len;
    while (len > 0 && name[len - 1] == ' ') {
        len--;
    }
    name[len] = '\0';
}

static void fat32_read_directory(FsNode* dir_node) {
    if (!dir_node || !(dir_node->mode & 0x4000)) {
        return;
    }
    
    u32 current_cluster = (u32)(uintptr_t)dir_node->fs_specific;
    FsNode* last_child = NULL;
    
    while (current_cluster != 0) {
        if (fat32_read_cluster(current_cluster, fat32_buffer) != 0) {
            break;
        }
        
        Fat32DirectoryEntry* entry = (Fat32DirectoryEntry*)fat32_buffer;
        
        for (u32 i = 0; i < fat32_cluster_size / sizeof(Fat32DirectoryEntry); i++) {
            if (entry[i].filename[0] == 0x00) {
                break;
            }
            
            if (entry[i].filename[0] == 0xE5) {
                continue;
            }
            
            if ((entry[i].attributes & 0x0F) == 0x0F) {
                continue;
            }
            
            char filename[9];
            memset(filename, 0, 9);
            memcpy(filename, entry[i].filename, 8);
            fat32_trim_name(filename, 8);
            
            char ext[4];
            memset(ext, 0, 4);
            memcpy(ext, entry[i].extension, 3);
            fat32_trim_name(ext, 3);
            
            char name[13];
            memset(name, 0, 13);
            strcpy(name, filename);
            if (ext[0] != '\0') {
                strcat(name, ".");
                strcat(name, ext);
            }
            
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                continue;
            }
            
            FsNode* child = kmalloc(sizeof(FsNode));
            if (!child) {
                return;
            }
            
            memset(child, 0, sizeof(FsNode));
            strncpy(child->name, name, 255);
            child->size = entry[i].file_size;
            child->mode = (entry[i].attributes & 0x10) ? 0x4000 : 0x8000;
            child->parent = dir_node;
            u32 cluster = ((u32)entry[i].first_cluster_high << 16) | entry[i].first_cluster_low;
            child->fs_specific = (void*)(uintptr_t)cluster;
            
            if (!dir_node->children) {
                dir_node->children = child;
            } else {
                last_child->next = child;
            }
            last_child = child;
        }
        
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
}

static FsNode* fat32_open(const char* path) {
    if (!fat32_buffer || !path) {
        return NULL;
    }
    
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        FsNode* node = kmalloc(sizeof(FsNode));
        if (!node) {
            return NULL;
        }
        memset(node, 0, sizeof(FsNode));
        strncpy(node->name, "/", 255);
        node->size = 0;
        node->mode = 0x4000;
        node->fs_specific = (void*)(uintptr_t)fat32_root_cluster;
        fat32_read_directory(node);
        return node;
    }
    
    u32 current_cluster = fat32_root_cluster;
    const char* remaining = path;
    
    while (*remaining && current_cluster != 0) {
        while (*remaining == '/') {
            remaining++;
        }
        if (!*remaining) {
            break;
        }
        
        if (fat32_read_cluster(current_cluster, fat32_buffer) != 0) {
            return NULL;
        }
        
        Fat32DirectoryEntry* entry = (Fat32DirectoryEntry*)fat32_buffer;
        int found = 0;
        
        for (u32 i = 0; i < fat32_cluster_size / sizeof(Fat32DirectoryEntry); i++) {
            if (entry[i].filename[0] == 0x00) {
                break;
            }
            
            if (entry[i].filename[0] == 0xE5) {
                continue;
            }
            
            char filename[9];
            memset(filename, 0, 9);
            memcpy(filename, entry[i].filename, 8);
            fat32_trim_name(filename, 8);
            
            char ext[4];
            memset(ext, 0, 4);
            memcpy(ext, entry[i].extension, 3);
            fat32_trim_name(ext, 3);
            
            char name[13];
            memset(name, 0, 13);
            strcpy(name, filename);
            if (ext[0] != '\0') {
                strcat(name, ".");
                strcat(name, ext);
            }
            
            const char* next_slash = strchr(remaining, '/');
            size_t name_len = next_slash ? (size_t)(next_slash - remaining) : strlen(remaining);
            
            char cmp_name[13];
            memset(cmp_name, 0, 13);
            strncpy(cmp_name, remaining, name_len);
            
            if (strcasecmp(name, cmp_name) == 0) {
                u32 cluster = ((u32)entry[i].first_cluster_high << 16) | entry[i].first_cluster_low;
                
                if (next_slash) {
                    current_cluster = cluster;
                    remaining = next_slash;
                    found = 1;
                    break;
                } else {
                    FsNode* node = kmalloc(sizeof(FsNode));
                    if (!node) {
                        return NULL;
                    }
                    
                    memset(node, 0, sizeof(FsNode));
                    strncpy(node->name, name, 255);
                    node->size = entry[i].file_size;
                    node->mode = (entry[i].attributes & 0x10) ? 0x4000 : 0x8000;
                    node->fs_specific = (void*)(uintptr_t)cluster;
                    
                    if (node->mode & 0x4000) {
                        fat32_read_directory(node);
                    }
                    
                    return node;
                }
            }
        }
        
        if (!found) {
            return NULL;
        }
    }
    
    return NULL;
}

static int fat32_read(FsNode* node, u64 offset, u64 size, void* buffer) {
    if (!node || !buffer || !fat32_bs) {
        return -1;
    }
    
    u32 start_cluster = (u32)(uintptr_t)node->fs_specific;
    if (start_cluster == 0) {
        return -1;
    }
    
    u8* buf = (u8*)buffer;
    u64 bytes_read = 0;
    u32 current_cluster = start_cluster;
    
    u32 cluster_offset = offset / fat32_cluster_size;
    u32 offset_in_cluster = offset % fat32_cluster_size;
    
    for (u32 i = 0; i < cluster_offset && current_cluster != 0; i++) {
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    while (bytes_read < size && current_cluster != 0) {
        if (fat32_read_cluster(current_cluster, fat32_buffer) != 0) {
            return -1;
        }
        
        u32 to_read = fat32_cluster_size - offset_in_cluster;
        if (to_read > size - bytes_read) {
            to_read = size - bytes_read;
        }
        
        memcpy(buf + bytes_read, fat32_buffer + offset_in_cluster, to_read);
        bytes_read += to_read;
        offset_in_cluster = 0;
        
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    return bytes_read;
}

static int fat32_write(FsNode* node, u64 offset, u64 size, const void* buffer) {
    return -1;
}

static int fat32_close(FsNode* node) {
    if (node) {
        kfree(node);
    }
    return 0;
}

static void fat32_format_name(const char* src, char* dest) {
    memset(dest, ' ', 11);
    const char* dot = strchr(src, '.');
    size_t name_len = dot ? (size_t)(dot - src) : strlen(src);
    size_t ext_len = dot ? strlen(dot + 1) : 0;
    
    for (size_t i = 0; i < name_len && i < 8; i++) {
        dest[i] = src[i] >= 'a' && src[i] <= 'z' ? src[i] - 32 : src[i];
    }
    
    if (dot && ext_len > 0) {
        for (size_t i = 0; i < ext_len && i < 3; i++) {
            dest[8 + i] = dot[1 + i] >= 'a' && dot[1 + i] <= 'z' ? dot[1 + i] - 32 : dot[1 + i];
        }
    }
}

static int fat32_mkdir(const char* path, u32 mode) {
    serial_log("=== fat32_mkdir START ===\n");
    
    if (!fat32_buffer || !path) {
        serial_log("fat32_mkdir: buffer or path is NULL\n");
        return -1;
    }
    
    while (*path == '/') {
        path++;
    }
    
    serial_log("fat32_mkdir: path after stripping slashes = ");
    serial_log(path);
    serial_log("\n");
    
    const char* last_slash = strrchr(path, '/');
    char parent_path[256];
    char dir_name[256];
    
    if (last_slash) {
        strncpy(parent_path, path, last_slash - path);
        parent_path[last_slash - path] = '\0';
        strcpy(dir_name, last_slash + 1);
    } else {
        strcpy(parent_path, "");
        strcpy(dir_name, path);
    }
    
    serial_log("fat32_mkdir: parent_path = ");
    serial_log(parent_path);
    serial_log(", dir_name = ");
    serial_log(dir_name);
    serial_log("\n");
    
    u32 parent_cluster = fat32_root_cluster;
    char pc[20];
    sprintf(pc, "%x", parent_cluster);
    serial_log("fat32_mkdir: parent_cluster = ");
    serial_log(pc);
    serial_log("\n");
    
    if (strlen(parent_path) > 0) {
        const char* remaining = parent_path;
        while (*remaining && parent_cluster != 0) {
            while (*remaining == '/') {
                remaining++;
            }
            if (!*remaining) {
                break;
            }
            
            if (fat32_read_cluster(parent_cluster, fat32_buffer) != 0) {
                return -1;
            }
            
            Fat32DirectoryEntry* entry = (Fat32DirectoryEntry*)fat32_buffer;
            int found = 0;
            
            for (u32 i = 0; i < fat32_cluster_size / sizeof(Fat32DirectoryEntry); i++) {
                if (entry[i].filename[0] == 0x00) {
                    break;
                }
                if (entry[i].filename[0] == 0xE5) {
                    continue;
                }
                
                char filename[9];
                memset(filename, 0, 9);
                memcpy(filename, entry[i].filename, 8);
                fat32_trim_name(filename, 8);
                
                char ext[4];
                memset(ext, 0, 4);
                memcpy(ext, entry[i].extension, 3);
                fat32_trim_name(ext, 3);
                
                char name[13];
                memset(name, 0, 13);
                strcpy(name, filename);
                if (ext[0] != '\0') {
                    strcat(name, ".");
                    strcat(name, ext);
                }
                
                const char* next_slash = strchr(remaining, '/');
                size_t name_len = next_slash ? (size_t)(next_slash - remaining) : strlen(remaining);
                
                char cmp_name[13];
                memset(cmp_name, 0, 13);
                strncpy(cmp_name, remaining, name_len);
                
                if (strcasecmp(name, cmp_name) == 0) {
                    parent_cluster = ((u32)entry[i].first_cluster_high << 16) | entry[i].first_cluster_low;
                    remaining = next_slash;
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                return -1;
            }
        }
    }
    
    if (fat32_read_cluster(parent_cluster, fat32_buffer) != 0) {
        return -1;
    }
    
    Fat32DirectoryEntry* entry = (Fat32DirectoryEntry*)fat32_buffer;
    
    for (u32 i = 0; i < fat32_cluster_size / sizeof(Fat32DirectoryEntry); i++) {
        if (entry[i].filename[0] == 0x00) {
            break;
        }
        if (entry[i].filename[0] == 0xE5) {
            continue;
        }
        
        char filename[9];
        memset(filename, 0, 9);
        memcpy(filename, entry[i].filename, 8);
        fat32_trim_name(filename, 8);
        
        char ext[4];
        memset(ext, 0, 4);
        memcpy(ext, entry[i].extension, 3);
        fat32_trim_name(ext, 3);
        
        char name[13];
        memset(name, 0, 13);
        strcpy(name, filename);
        if (ext[0] != '\0') {
            strcat(name, ".");
            strcat(name, ext);
        }
        
        if (strcasecmp(name, dir_name) == 0) {
            return -1;
        }
    }
    
    u8 parent_dir_buffer[4096];
    memcpy(parent_dir_buffer, fat32_buffer, fat32_cluster_size);
    
    u32 new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0) {
        return -1;
    }
    
    memcpy(fat32_buffer, parent_dir_buffer, fat32_cluster_size);
    
    u8 new_dir_buffer[4096];
    memset(new_dir_buffer, 0, 4096);
    
    Fat32DirectoryEntry* new_entry = (Fat32DirectoryEntry*)new_dir_buffer;
    
    memset(new_entry[0].filename, 0, 8);
    new_entry[0].filename[0] = 0x2E;
    memset(new_entry[0].extension, 0, 3);
    new_entry[0].attributes = 0x10;
    new_entry[0].first_cluster_low = new_cluster & 0xFFFF;
    new_entry[0].first_cluster_high = (new_cluster >> 16) & 0xFFFF;
    
    memset(new_entry[1].filename, 0, 8);
    new_entry[1].filename[0] = 0x2E;
    new_entry[1].filename[1] = 0x2E;
    memset(new_entry[1].extension, 0, 3);
    new_entry[1].attributes = 0x10;
    new_entry[1].first_cluster_low = parent_cluster & 0xFFFF;
    new_entry[1].first_cluster_high = (parent_cluster >> 16) & 0xFFFF;
    
    for (u32 i = 0; i < fat32_sectors_per_cluster; i++) {
        u32 sector = (u32)fat32_partition_start_lba + fat32_data_start + (new_cluster - 2) * fat32_sectors_per_cluster + i;
        fat32_write_sector(sector, new_dir_buffer + i * fat32_sector_size);
    }
    
    for (u32 i = 0; i < fat32_cluster_size / sizeof(Fat32DirectoryEntry); i++) {
        serial_log("fat32_mkdir: checking entry ");
        char ei[8];
        int eii = 0;
        if (i == 0) ei[eii++] = '0';
        else { u32 iv = i; while (iv > 0) { ei[eii++] = '0' + (iv % 10); iv /= 10; } }
        for (int k = eii - 1; k >= 0; k--) serial_putchar(ei[k]);
        serial_log(", filename[0]=0x");
        char fn[4];
        int fni = 0;
        u8 fnv = entry[i].filename[0];
        fn[fni++] = "0123456789ABCDEF"[(fnv >> 4) & 0xF];
        fn[fni++] = "0123456789ABCDEF"[fnv & 0xF];
        for (int k = 0; k < fni; k++) serial_putchar(fn[k]);
        serial_log("\n");
        
        if (entry[i].filename[0] == 0x00 || entry[i].filename[0] == 0xE5) {
            serial_log("fat32_mkdir: found empty entry at index ");
            int eii2 = 0;
            char ei2[8];
            if (i == 0) ei2[eii2++] = '0';
            else { u32 iv = i; while (iv > 0) { ei2[eii2++] = '0' + (iv % 10); iv /= 10; } }
            for (int k = eii2 - 1; k >= 0; k--) serial_putchar(ei2[k]);
            serial_log("\n");
            
            char name_buf[11];
            fat32_format_name(dir_name, name_buf);
            
            memcpy(entry[i].filename, name_buf, 8);
            memcpy(entry[i].extension, name_buf + 8, 3);
            entry[i].attributes = 0x10;
            entry[i].first_cluster_low = new_cluster & 0xFFFF;
            entry[i].first_cluster_high = (new_cluster >> 16) & 0xFFFF;
            entry[i].file_size = 0;
            
            serial_log("fat32_mkdir: writing parent directory sectors\n");
            for (u32 j = 0; j < fat32_sectors_per_cluster; j++) {
                u32 sector = (u32)fat32_partition_start_lba + fat32_data_start + (parent_cluster - 2) * fat32_sectors_per_cluster + j;
                fat32_write_sector(sector, fat32_buffer + j * fat32_sector_size);
            }
            
            serial_log("fat32_mkdir: success, returning 0\n");
            return 0;
        }
    }
    
    serial_log("fat32_mkdir: no empty entry found, rolling back cluster\n");
    fat32_write_fat_entry(new_cluster, 0);
    return -1;
}

static int fat32_create(const char* path, u32 mode) {
    if (!fat32_buffer || !path) {
        return -1;
    }
    
    while (*path == '/') {
        path++;
    }
    
    const char* last_slash = strrchr(path, '/');
    char parent_path[256];
    char file_name[256];
    
    if (last_slash) {
        strncpy(parent_path, path, last_slash - path);
        parent_path[last_slash - path] = '\0';
        strcpy(file_name, last_slash + 1);
    } else {
        strcpy(parent_path, "");
        strcpy(file_name, path);
    }
    
    u32 parent_cluster = fat32_root_cluster;
    
    if (strlen(parent_path) > 0) {
        const char* remaining = parent_path;
        while (*remaining && parent_cluster != 0) {
            while (*remaining == '/') {
                remaining++;
            }
            if (!*remaining) {
                break;
            }
            
            if (fat32_read_cluster(parent_cluster, fat32_buffer) != 0) {
                return -1;
            }
            
            Fat32DirectoryEntry* entry = (Fat32DirectoryEntry*)fat32_buffer;
            int found = 0;
            
            for (u32 i = 0; i < fat32_cluster_size / sizeof(Fat32DirectoryEntry); i++) {
                if (entry[i].filename[0] == 0x00) {
                    break;
                }
                if (entry[i].filename[0] == 0xE5) {
                    continue;
                }
                
                char filename[9];
                memset(filename, 0, 9);
                memcpy(filename, entry[i].filename, 8);
                fat32_trim_name(filename, 8);
                
                char ext[4];
                memset(ext, 0, 4);
                memcpy(ext, entry[i].extension, 3);
                fat32_trim_name(ext, 3);
                
                char name[13];
                memset(name, 0, 13);
                strcpy(name, filename);
                if (ext[0] != '\0') {
                    strcat(name, ".");
                    strcat(name, ext);
                }
                
                const char* next_slash = strchr(remaining, '/');
                size_t name_len = next_slash ? (size_t)(next_slash - remaining) : strlen(remaining);
                
                char cmp_name[13];
                memset(cmp_name, 0, 13);
                strncpy(cmp_name, remaining, name_len);
                
                if (strcasecmp(name, cmp_name) == 0) {
                    parent_cluster = ((u32)entry[i].first_cluster_high << 16) | entry[i].first_cluster_low;
                    remaining = next_slash;
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                return -1;
            }
        }
    }
    
    if (fat32_read_cluster(parent_cluster, fat32_buffer) != 0) {
        return -1;
    }
    
    Fat32DirectoryEntry* entry = (Fat32DirectoryEntry*)fat32_buffer;
    
    for (u32 i = 0; i < fat32_cluster_size / sizeof(Fat32DirectoryEntry); i++) {
        if (entry[i].filename[0] == 0x00) {
            break;
        }
        if (entry[i].filename[0] == 0xE5) {
            continue;
        }
        
        char filename[9];
        memset(filename, 0, 9);
        memcpy(filename, entry[i].filename, 8);
        fat32_trim_name(filename, 8);
        
        char ext[4];
        memset(ext, 0, 4);
        memcpy(ext, entry[i].extension, 3);
        fat32_trim_name(ext, 3);
        
        char name[13];
        memset(name, 0, 13);
        strcpy(name, filename);
        if (ext[0] != '\0') {
            strcat(name, ".");
            strcat(name, ext);
        }
        
        if (strcasecmp(name, file_name) == 0) {
            return -1;
        }
    }
    
    u8 parent_dir_buffer[4096];
    memcpy(parent_dir_buffer, fat32_buffer, fat32_cluster_size);
    
    u32 new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0) {
        return -1;
    }
    
    memcpy(fat32_buffer, parent_dir_buffer, fat32_cluster_size);
    
    for (u32 i = 0; i < fat32_cluster_size / sizeof(Fat32DirectoryEntry); i++) {
        if (entry[i].filename[0] == 0x00 || entry[i].filename[0] == 0xE5) {
            char name_buf[11];
            fat32_format_name(file_name, name_buf);
            
            memcpy(entry[i].filename, name_buf, 8);
            memcpy(entry[i].extension, name_buf + 8, 3);
            entry[i].attributes = 0x20;
            entry[i].first_cluster_low = new_cluster & 0xFFFF;
            entry[i].first_cluster_high = (new_cluster >> 16) & 0xFFFF;
            entry[i].file_size = 0;
            
            for (u32 j = 0; j < fat32_sectors_per_cluster; j++) {
                u32 sector = (u32)fat32_partition_start_lba + fat32_data_start + (parent_cluster - 2) * fat32_sectors_per_cluster + j;
                fat32_write_sector(sector, fat32_buffer + j * fat32_sector_size);
            }
            
            return 0;
        }
    }
    
    fat32_write_fat_entry(new_cluster, 0);
    return -1;
}

static int fat32_remove_entry(u32 parent_cluster, const char* name, int is_dir) {
    if (fat32_read_cluster(parent_cluster, fat32_buffer) != 0) {
        return -1;
    }
    
    Fat32DirectoryEntry* entry = (Fat32DirectoryEntry*)fat32_buffer;
    u8 parent_dir_buffer[4096];
    memcpy(parent_dir_buffer, fat32_buffer, fat32_cluster_size);
    
    for (u32 i = 0; i < fat32_cluster_size / sizeof(Fat32DirectoryEntry); i++) {
        if (entry[i].filename[0] == 0x00) {
            break;
        }
        if (entry[i].filename[0] == 0xE5) {
            continue;
        }
        
        char filename[9];
        memset(filename, 0, 9);
        memcpy(filename, entry[i].filename, 8);
        fat32_trim_name(filename, 8);
        
        char ext[4];
        memset(ext, 0, 4);
        memcpy(ext, entry[i].extension, 3);
        fat32_trim_name(ext, 3);
        
        char entry_name[13];
        memset(entry_name, 0, 13);
        strcpy(entry_name, filename);
        if (ext[0] != '\0') {
            strcat(entry_name, ".");
            strcat(entry_name, ext);
        }
        
        if (strcasecmp(entry_name, name) == 0) {
            u32 cluster = ((u32)entry[i].first_cluster_high << 16) | entry[i].first_cluster_low;
            
            while (cluster != 0) {
                u32 next_cluster = fat32_get_next_cluster(cluster);
                fat32_write_fat_entry(cluster, 0);
                cluster = next_cluster;
            }
            
            memcpy(fat32_buffer, parent_dir_buffer, fat32_cluster_size);
            entry = (Fat32DirectoryEntry*)fat32_buffer;
            entry[i].filename[0] = 0xE5;
            
            for (u32 j = 0; j < fat32_sectors_per_cluster; j++) {
                u32 sector = (u32)fat32_partition_start_lba + fat32_data_start + (parent_cluster - 2) * fat32_sectors_per_cluster + j;
                fat32_write_sector(sector, fat32_buffer + j * fat32_sector_size);
            }
            
            return 0;
        }
    }
    
    return -1;
}

static int fat32_remove(const char* path) {
    if (!fat32_buffer || !path) {
        return -1;
    }
    
    while (*path == '/') {
        path++;
    }
    
    const char* last_slash = strrchr(path, '/');
    char parent_path[256];
    char file_name[256];
    
    if (last_slash) {
        strncpy(parent_path, path, last_slash - path);
        parent_path[last_slash - path] = '\0';
        strcpy(file_name, last_slash + 1);
    } else {
        strcpy(parent_path, "");
        strcpy(file_name, path);
    }
    
    u32 parent_cluster = fat32_root_cluster;
    
    if (strlen(parent_path) > 0) {
        const char* remaining = parent_path;
        while (*remaining && parent_cluster != 0) {
            while (*remaining == '/') {
                remaining++;
            }
            if (!*remaining) {
                break;
            }
            
            if (fat32_read_cluster(parent_cluster, fat32_buffer) != 0) {
                return -1;
            }
            
            Fat32DirectoryEntry* entry = (Fat32DirectoryEntry*)fat32_buffer;
            int found = 0;
            
            for (u32 i = 0; i < fat32_cluster_size / sizeof(Fat32DirectoryEntry); i++) {
                if (entry[i].filename[0] == 0x00) {
                    break;
                }
                if (entry[i].filename[0] == 0xE5) {
                    continue;
                }
                
                char filename[9];
                memset(filename, 0, 9);
                memcpy(filename, entry[i].filename, 8);
                fat32_trim_name(filename, 8);
                
                char ext[4];
                memset(ext, 0, 4);
                memcpy(ext, entry[i].extension, 3);
                fat32_trim_name(ext, 3);
                
                char name[13];
                memset(name, 0, 13);
                strcpy(name, filename);
                if (ext[0] != '\0') {
                    strcat(name, ".");
                    strcat(name, ext);
                }
                
                const char* next_slash = strchr(remaining, '/');
                size_t name_len = next_slash ? (size_t)(next_slash - remaining) : strlen(remaining);
                
                char cmp_name[13];
                memset(cmp_name, 0, 13);
                strncpy(cmp_name, remaining, name_len);
                
                if (strcasecmp(name, cmp_name) == 0) {
                    parent_cluster = ((u32)entry[i].first_cluster_high << 16) | entry[i].first_cluster_low;
                    remaining = next_slash;
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                return -1;
            }
        }
    }
    
    return fat32_remove_entry(parent_cluster, file_name, 0);
}

static int fat32_rmdir(const char* path) {
    if (!fat32_buffer || !path) {
        return -1;
    }
    
    while (*path == '/') {
        path++;
    }
    
    const char* last_slash = strrchr(path, '/');
    char parent_path[256];
    char dir_name[256];
    
    if (last_slash) {
        strncpy(parent_path, path, last_slash - path);
        parent_path[last_slash - path] = '\0';
        strcpy(dir_name, last_slash + 1);
    } else {
        strcpy(parent_path, "");
        strcpy(dir_name, path);
    }
    
    u32 parent_cluster = fat32_root_cluster;
    
    if (strlen(parent_path) > 0) {
        const char* remaining = parent_path;
        while (*remaining && parent_cluster != 0) {
            while (*remaining == '/') {
                remaining++;
            }
            if (!*remaining) {
                break;
            }
            
            if (fat32_read_cluster(parent_cluster, fat32_buffer) != 0) {
                return -1;
            }
            
            Fat32DirectoryEntry* entry = (Fat32DirectoryEntry*)fat32_buffer;
            int found = 0;
            
            for (u32 i = 0; i < fat32_cluster_size / sizeof(Fat32DirectoryEntry); i++) {
                if (entry[i].filename[0] == 0x00) {
                    break;
                }
                if (entry[i].filename[0] == 0xE5) {
                    continue;
                }
                
                char filename[9];
                memset(filename, 0, 9);
                memcpy(filename, entry[i].filename, 8);
                fat32_trim_name(filename, 8);
                
                char ext[4];
                memset(ext, 0, 4);
                memcpy(ext, entry[i].extension, 3);
                fat32_trim_name(ext, 3);
                
                char name[13];
                memset(name, 0, 13);
                strcpy(name, filename);
                if (ext[0] != '\0') {
                    strcat(name, ".");
                    strcat(name, ext);
                }
                
                const char* next_slash = strchr(remaining, '/');
                size_t name_len = next_slash ? (size_t)(next_slash - remaining) : strlen(remaining);
                
                char cmp_name[13];
                memset(cmp_name, 0, 13);
                strncpy(cmp_name, remaining, name_len);
                
                if (strcasecmp(name, cmp_name) == 0) {
                    parent_cluster = ((u32)entry[i].first_cluster_high << 16) | entry[i].first_cluster_low;
                    remaining = next_slash;
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                return -1;
            }
        }
    }
    
    if (fat32_read_cluster(parent_cluster, fat32_buffer) != 0) {
        return -1;
    }
    
    Fat32DirectoryEntry* entry = (Fat32DirectoryEntry*)fat32_buffer;
    
    for (u32 i = 0; i < fat32_cluster_size / sizeof(Fat32DirectoryEntry); i++) {
        if (entry[i].filename[0] == 0x00) {
            break;
        }
        if (entry[i].filename[0] == 0xE5) {
            continue;
        }
        
        char filename[9];
        memset(filename, 0, 9);
        memcpy(filename, entry[i].filename, 8);
        fat32_trim_name(filename, 8);
        
        char ext[4];
        memset(ext, 0, 4);
        memcpy(ext, entry[i].extension, 3);
        fat32_trim_name(ext, 3);
        
        char name[13];
        memset(name, 0, 13);
        strcpy(name, filename);
        if (ext[0] != '\0') {
            strcat(name, ".");
            strcat(name, ext);
        }
        
        if (strcasecmp(name, dir_name) == 0) {
            u32 dir_cluster = ((u32)entry[i].first_cluster_high << 16) | entry[i].first_cluster_low;
            
            if (fat32_read_cluster(dir_cluster, fat32_buffer) != 0) {
                return -1;
            }
            
            Fat32DirectoryEntry* dir_entry = (Fat32DirectoryEntry*)fat32_buffer;
            int entry_count = 0;
            
            for (u32 j = 0; j < fat32_cluster_size / sizeof(Fat32DirectoryEntry); j++) {
                if (dir_entry[j].filename[0] == 0x00) {
                    break;
                }
                if (dir_entry[j].filename[0] == 0xE5) {
                    continue;
                }
                entry_count++;
            }
            
            if (entry_count != 2) {
                return -1;
            }
            
            return fat32_remove_entry(parent_cluster, dir_name, 1);
        }
    }
    
    return -1;
}

FileSystem fat32_fs = {
    .name = "fat32",
    .type = FS_TYPE_FAT32,
    .mount = fat32_mount,
    .unmount = fat32_unmount,
    .open = fat32_open,
    .read = fat32_read,
    .write = fat32_write,
    .close = fat32_close,
    .create = fat32_create,
    .remove = fat32_remove,
    .mkdir = fat32_mkdir,
    .rmdir = fat32_rmdir
};