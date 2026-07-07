#include <mm.h>
#include <types.h>

#define COM1_BASE 0x3F8

static inline u8 inb(u16 port) {
    u8 val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outb(u16 port, u8 val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void mm_log(const char* str) {
    while (*str) {
        if (*str == '\n') {
            while (!(inb(COM1_BASE + 5) & 0x20)) asm volatile("nop");
            outb(COM1_BASE, '\r');
        }
        while (!(inb(COM1_BASE + 5) & 0x20)) asm volatile("nop");
        outb(COM1_BASE, *str++);
    }
}

typedef struct Block {
    size_t size;
    struct Block* next;
    struct Block* prev;
    int free;
} Block;

static Block* heap_start = NULL;
static Block* heap_end = NULL;
static const size_t HEAP_SIZE = 0x1000000;

void kmalloc_init(void) {
    mm_log("kmalloc_init: started\n");
    void* heap = pmm_alloc_page();
    mm_log("kmalloc_init: pmm_alloc_page returned ");
    if (heap) {
        mm_log("OK\n");
    } else {
        mm_log("NULL\n");
        return;
    }
    
    heap_start = (Block*)heap;
    heap_start->size = PAGE_SIZE - sizeof(Block);
    heap_start->next = NULL;
    heap_start->prev = NULL;
    heap_start->free = 1;
    heap_end = heap_start;
    mm_log("kmalloc_init: done\n");
}

void* kmalloc(size_t size) {
    mm_log("kmalloc: size=");
    char num[16];
    int ni = 0;
    size_t s = size;
    if (s == 0) num[ni++] = '0';
    else while (s > 0) { num[ni++] = '0' + (s % 10); s /= 10; }
    for (int i = ni - 1; i >= 0; i--) { while (!(inb(COM1_BASE + 5) & 0x20)) asm volatile("nop"); outb(COM1_BASE, num[i]); }
    mm_log("\n");
    
    if (size == 0) {
        mm_log("kmalloc: size 0, returning NULL\n");
        return NULL;
    }
    
    size = (size + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1);
    
    Block* current = heap_start;
    mm_log("kmalloc: heap_start=");
    if (current) {
        char hex[20] = "0x";
        u64 addr = (u64)current;
        for (int i = 17; i >= 2; i--) {
            hex[i] = "0123456789ABCDEF"[addr & 0xF];
            addr >>= 4;
        }
        mm_log(hex);
    } else {
        mm_log("NULL");
    }
    mm_log("\n");
    
    while (current) {
        mm_log("kmalloc: checking block at ");
        char hex[20] = "0x";
        u64 addr = (u64)current;
        for (int i = 17; i >= 2; i--) {
            hex[i] = "0123456789ABCDEF"[addr & 0xF];
            addr >>= 4;
        }
        mm_log(hex);
        mm_log(", free=");
        if (current->free) mm_log("1"); else mm_log("0");
        mm_log(", size=");
        char sz[16];
        int si = 0;
        size_t szv = current->size;
        if (szv == 0) sz[si++] = '0';
        else while (szv > 0) { sz[si++] = '0' + (szv % 10); szv /= 10; }
        for (int i = si - 1; i >= 0; i--) { while (!(inb(COM1_BASE + 5) & 0x20)) asm volatile("nop"); outb(COM1_BASE, sz[i]); }
        mm_log("\n");
        
        if (current->free && current->size >= size) {
            if (current->size - size > sizeof(Block)) {
                Block* new_block = (Block*)((u8*)current + sizeof(Block) + size);
                new_block->size = current->size - size - sizeof(Block);
                new_block->next = current->next;
                new_block->prev = current;
                new_block->free = 1;
                
                if (current->next) {
                    current->next->prev = new_block;
                } else {
                    heap_end = new_block;
                }
                
                current->size = size;
                current->next = new_block;
            }
            
            current->free = 0;
            mm_log("kmalloc: found block, returning ");
            char ret_hex[20] = "0x";
            u64 ret_addr = (u64)((u8*)current + sizeof(Block));
            for (int i = 17; i >= 2; i--) {
                ret_hex[i] = "0123456789ABCDEF"[ret_addr & 0xF];
                ret_addr >>= 4;
            }
            mm_log(ret_hex);
            mm_log("\n");
            return (void*)((u8*)current + sizeof(Block));
        }
        current = current->next;
    }
    
    mm_log("kmalloc: no block found, allocating new page\n");
    Block* new_page = pmm_alloc_page();
    mm_log("kmalloc: pmm_alloc_page returned ");
    if (new_page) {
        char hex[20] = "0x";
        u64 addr = (u64)new_page;
        for (int i = 17; i >= 2; i--) {
            hex[i] = "0123456789ABCDEF"[addr & 0xF];
            addr >>= 4;
        }
        mm_log(hex);
    } else {
        mm_log("NULL");
    }
    mm_log("\n");
    
    if (!new_page) {
        mm_log("kmalloc: pmm_alloc_page failed\n");
        return NULL;
    }
    
    Block* new_block = (Block*)new_page;
    new_block->size = PAGE_SIZE - sizeof(Block);
    new_block->next = NULL;
    new_block->prev = heap_end;
    new_block->free = 1;
    
    mm_log("kmalloc: new_block->size=");
    char sz2[16];
    int si2 = 0;
    size_t szv2 = new_block->size;
    if (szv2 == 0) sz2[si2++] = '0';
    else while (szv2 > 0) { sz2[si2++] = '0' + (szv2 % 10); szv2 /= 10; }
    for (int i = si2 - 1; i >= 0; i--) { while (!(inb(COM1_BASE + 5) & 0x20)) asm volatile("nop"); outb(COM1_BASE, sz2[i]); }
    mm_log("\n");
    
    if (heap_end) {
        mm_log("kmalloc: linking to heap_end\n");
        heap_end->next = new_block;
    } else {
        mm_log("kmalloc: setting heap_start\n");
        heap_start = new_block;
    }
    heap_end = new_block;
    
    mm_log("kmalloc: after adding new block, heap_start=");
    if (heap_start) {
        char hex[20] = "0x";
        u64 addr = (u64)heap_start;
        for (int i = 17; i >= 2; i--) {
            hex[i] = "0123456789ABCDEF"[addr & 0xF];
            addr >>= 4;
        }
        mm_log(hex);
    } else {
        mm_log("NULL");
    }
    mm_log(", heap_end=");
    if (heap_end) {
        char hex[20] = "0x";
        u64 addr = (u64)heap_end;
        for (int i = 17; i >= 2; i--) {
            hex[i] = "0123456789ABCDEF"[addr & 0xF];
            addr >>= 4;
        }
        mm_log(hex);
    } else {
        mm_log("NULL");
    }
    mm_log("\n");
    
    mm_log("kmalloc: directly returning new block\n");
    new_block->free = 0;
    char ret_hex[20] = "0x";
    u64 ret_addr = (u64)((u8*)new_block + sizeof(Block));
    for (int i = 17; i >= 2; i--) {
        ret_hex[i] = "0123456789ABCDEF"[ret_addr & 0xF];
        ret_addr >>= 4;
    }
    mm_log("kmalloc: returning ");
    mm_log(ret_hex);
    mm_log("\n");
    return (void*)((u8*)new_block + sizeof(Block));
}

void kfree(void* ptr) {
    if (!ptr) {
        return;
    }
    
    Block* block = (Block*)((u8*)ptr - sizeof(Block));
    block->free = 1;
    
    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(Block) + block->size;
        block->prev->next = block->next;
        
        if (block->next) {
            block->next->prev = block->prev;
        } else {
            heap_end = block->prev;
        }
        
        block = block->prev;
    }
    
    if (block->next && block->next->free) {
        block->size += sizeof(Block) + block->next->size;
        block->next = block->next->next;
        
        if (block->next) {
            block->next->prev = block;
        } else {
            heap_end = block;
        }
    }
}

void* kcalloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* ptr = kmalloc(total);
    
    if (ptr) {
        for (size_t i = 0; i < total; i++) {
            ((u8*)ptr)[i] = 0;
        }
    }
    
    return ptr;
}

void* krealloc(void* ptr, size_t size) {
    if (!ptr) {
        return kmalloc(size);
    }
    
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    Block* block = (Block*)((u8*)ptr - sizeof(Block));
    
    if (block->size >= size) {
        return ptr;
    }
    
    void* new_ptr = kmalloc(size);
    if (!new_ptr) {
        return NULL;
    }
    
    size_t copy_size = block->size < size ? block->size : size;
    for (size_t i = 0; i < copy_size; i++) {
        ((u8*)new_ptr)[i] = ((u8*)ptr)[i];
    }
    
    kfree(ptr);
    return new_ptr;
}