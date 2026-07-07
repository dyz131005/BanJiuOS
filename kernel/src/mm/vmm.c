#include <mm.h>
#include <types.h>

#define PTE_PRESENT    0x001
#define PTE_WRITE      0x002
#define PTE_USER       0x004
#define PTE_PWT        0x008
#define PTE_PCD        0x010
#define PTE_ACCESSED   0x020
#define PTE_DIRTY      0x040
#define PTE_PAT        0x080
#define PTE_GLOBAL     0x100

u64* create_page_table(void) {
    u64* pml4 = pmm_alloc_page();
    if (!pml4) {
        return NULL;
    }
    
    for (int i = 0; i < 512; i++) {
        pml4[i] = 0;
    }
    
    u64* pdp = pmm_alloc_page();
    if (!pdp) {
        pmm_free_page(pml4);
        return NULL;
    }
    
    for (int i = 0; i < 512; i++) {
        pdp[i] = 0;
    }
    
    pml4[0] = (u64)pdp | PTE_PRESENT | PTE_WRITE;
    
    return pml4;
}

void map_page(u64* pml4, u64 vaddr, u64 paddr, u32 flags) {
    u64 pml4_idx = (vaddr >> 39) & 0x1FF;
    u64 pdp_idx = (vaddr >> 30) & 0x1FF;
    u64 pd_idx = (vaddr >> 21) & 0x1FF;
    u64 pt_idx = (vaddr >> 12) & 0x1FF;
    
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        u64* pdp = pmm_alloc_page();
        for (int i = 0; i < 512; i++) {
            ((u64*)pdp)[i] = 0;
        }
        pml4[pml4_idx] = (u64)pdp | PTE_PRESENT | PTE_WRITE;
    }
    
    u64* pdp = (u64*)(pml4[pml4_idx] & ~0xFFF);
    
    if (!(pdp[pdp_idx] & PTE_PRESENT)) {
        u64* pd = pmm_alloc_page();
        for (int i = 0; i < 512; i++) {
            ((u64*)pd)[i] = 0;
        }
        pdp[pdp_idx] = (u64)pd | PTE_PRESENT | PTE_WRITE;
    }
    
    u64* pd = (u64*)(pdp[pdp_idx] & ~0xFFF);
    
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        u64* pt = pmm_alloc_page();
        for (int i = 0; i < 512; i++) {
            ((u64*)pt)[i] = 0;
        }
        pd[pd_idx] = (u64)pt | PTE_PRESENT | PTE_WRITE;
    }
    
    u64* pt = (u64*)(pd[pd_idx] & ~0xFFF);
    
    pt[pt_idx] = paddr | flags | PTE_PRESENT;
}

void unmap_page(u64* pml4, u64 vaddr) {
    u64 pml4_idx = (vaddr >> 39) & 0x1FF;
    u64 pdp_idx = (vaddr >> 30) & 0x1FF;
    u64 pd_idx = (vaddr >> 21) & 0x1FF;
    u64 pt_idx = (vaddr >> 12) & 0x1FF;
    
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        return;
    }
    
    u64* pdp = (u64*)(pml4[pml4_idx] & ~0xFFF);
    
    if (!(pdp[pdp_idx] & PTE_PRESENT)) {
        return;
    }
    
    u64* pd = (u64*)(pdp[pdp_idx] & ~0xFFF);
    
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        return;
    }
    
    u64* pt = (u64*)(pd[pd_idx] & ~0xFFF);
    
    pt[pt_idx] = 0;
}

u64 get_physical_address(u64* pml4, u64 vaddr) {
    u64 pml4_idx = (vaddr >> 39) & 0x1FF;
    u64 pdp_idx = (vaddr >> 30) & 0x1FF;
    u64 pd_idx = (vaddr >> 21) & 0x1FF;
    u64 pt_idx = (vaddr >> 12) & 0x1FF;
    
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        return 0;
    }
    
    u64* pdp = (u64*)(pml4[pml4_idx] & ~0xFFF);
    
    if (!(pdp[pdp_idx] & PTE_PRESENT)) {
        return 0;
    }
    
    u64* pd = (u64*)(pdp[pdp_idx] & ~0xFFF);
    
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        return 0;
    }
    
    u64* pt = (u64*)(pd[pd_idx] & ~0xFFF);
    
    if (!(pt[pt_idx] & PTE_PRESENT)) {
        return 0;
    }
    
    return (pt[pt_idx] & ~0xFFF) | (vaddr & 0xFFF);
}