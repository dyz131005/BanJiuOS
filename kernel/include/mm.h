#ifndef MM_H
#define MM_H

#include <types.h>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define MEMORY_MAX 0x10000000000

typedef struct {
    u64 start;
    u64 end;
    u32 type;
} MemoryMapEntry;

typedef struct Page {
    struct Page* next;
    struct Page* prev;
    u64 flags;
} Page;

typedef struct {
    Page* free_list;
    u64 total_pages;
    u64 free_pages;
} PageAllocator;

typedef struct {
    u64* pml4;
    u64* pdp;
    u64* pd;
    u64* pt;
} PageTable;

void mm_init(MemoryMapEntry* mmap, size_t mmap_size);
void* kmalloc(size_t size);
void kfree(void* ptr);
void* kcalloc(size_t nmemb, size_t size);
void* krealloc(void* ptr, size_t size);

void* pmm_alloc_page(void);
void pmm_free_page(void* addr);
u64 pmm_get_total_pages(void);
u64 pmm_get_free_pages(void);

u64* create_page_table(void);
void map_page(u64* pml4, u64 vaddr, u64 paddr, u32 flags);
void unmap_page(u64* pml4, u64 vaddr);
u64 get_physical_address(u64* pml4, u64 vaddr);

#endif