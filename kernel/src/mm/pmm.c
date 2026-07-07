#include <mm.h>
#include <types.h>

static PageAllocator page_allocator;

static u64 align_up(u64 value, u64 alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

void pmm_init(MemoryMapEntry* mmap, size_t mmap_size) {
    page_allocator.free_list = NULL;
    page_allocator.total_pages = 0;
    page_allocator.free_pages = 0;
    
    for (size_t i = 0; i < mmap_size / sizeof(MemoryMapEntry); i++) {
        MemoryMapEntry* entry = &mmap[i];
        
        if (entry->type != 1) {
            continue;
        }
        
        u64 start = align_up(entry->start, PAGE_SIZE);
        u64 end = entry->end & ~(PAGE_SIZE - 1);
        
        if (start >= end) {
            continue;
        }
        
        u64 num_pages = (end - start) / PAGE_SIZE;
        page_allocator.total_pages += num_pages;
        page_allocator.free_pages += num_pages;
        
        for (u64 j = 0; j < num_pages; j++) {
            u64 addr = start + j * PAGE_SIZE;
            Page* page = (Page*)addr;
            
            page->next = page_allocator.free_list;
            page->prev = NULL;
            page->flags = 0;
            
            if (page_allocator.free_list) {
                page_allocator.free_list->prev = page;
            }
            page_allocator.free_list = page;
        }
    }
}

void* pmm_alloc_page(void) {
    if (!page_allocator.free_list) {
        return NULL;
    }
    
    Page* page = page_allocator.free_list;
    page_allocator.free_list = page->next;
    
    if (page_allocator.free_list) {
        page_allocator.free_list->prev = NULL;
    }
    
    page->next = NULL;
    page->prev = NULL;
    page->flags = 1;
    
    page_allocator.free_pages--;
    
    return (void*)((u64)page);
}

void pmm_free_page(void* addr) {
    if (!addr) {
        return;
    }
    
    Page* page = (Page*)addr;
    page->next = page_allocator.free_list;
    page->prev = NULL;
    page->flags = 0;
    
    if (page_allocator.free_list) {
        page_allocator.free_list->prev = page;
    }
    page_allocator.free_list = page;
    
    page_allocator.free_pages++;
}

u64 pmm_get_total_pages(void) {
    return page_allocator.total_pages;
}

u64 pmm_get_free_pages(void) {
    return page_allocator.free_pages;
}