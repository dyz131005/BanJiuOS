#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>
#include "EfiElfLoader.h"

extern void LoadCr3(UINT64 cr3);

typedef struct {
  UINT64 phys_base;
  UINT32 width;
  UINT32 height;
  UINT32 pitch;
  UINT32 pixel_format;
  UINT64 size;
  UINT8* virt_base;
} FramebufferInfo;

#define PIXEL_FORMAT_BGRA 0
#define PIXEL_FORMAT_RGBA 1

EFI_STATUS GetGopInfo(FramebufferInfo *FbInfo) {
  EFI_STATUS Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&Gop);
  if (EFI_ERROR(Status)) {
    Print(L"Warning: Cannot locate Graphics Output Protocol\n");
    return Status;
  }

  FbInfo->phys_base = Gop->Mode->FrameBufferBase;
  FbInfo->width = Gop->Mode->Info->HorizontalResolution;
  FbInfo->height = Gop->Mode->Info->VerticalResolution;
  FbInfo->pitch = Gop->Mode->Info->PixelsPerScanLine * 4;
  FbInfo->size = Gop->Mode->FrameBufferSize;
  FbInfo->virt_base = NULL;

  switch (Gop->Mode->Info->PixelFormat) {
    case PixelRedGreenBlueReserved8BitPerColor:
      FbInfo->pixel_format = PIXEL_FORMAT_RGBA;
      break;
    case PixelBlueGreenRedReserved8BitPerColor:
      FbInfo->pixel_format = PIXEL_FORMAT_BGRA;
      break;
    default:
      FbInfo->pixel_format = PIXEL_FORMAT_BGRA;
      break;
  }

  Print(L"GOP Info:\n");
  Print(L"  Framebuffer Base: 0x%lX\n", FbInfo->phys_base);
  Print(L"  Width: %d\n", FbInfo->width);
  Print(L"  Height: %d\n", FbInfo->height);
  Print(L"  Pitch: %d\n", FbInfo->pitch);
  Print(L"  Size: 0x%lX\n", FbInfo->size);
  Print(L"  Pixel Format: %d\n", FbInfo->pixel_format);

  return EFI_SUCCESS;
}

VOID SetupPagingC(UINT64 KernelPhysicalBase, UINT64 KernelVirtualBase) {
    EFI_PHYSICAL_ADDRESS PML4_phys = 0;
    EFI_PHYSICAL_ADDRESS PDP_Low_phys = 0;
    EFI_PHYSICAL_ADDRESS PD_Low_phys = 0;
    EFI_PHYSICAL_ADDRESS PDP_High_phys = 0;
    EFI_PHYSICAL_ADDRESS PD_High_phys = 0;
    EFI_PHYSICAL_ADDRESS PT_High_phys = 0;
    
    gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &PML4_phys);
    gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &PDP_Low_phys);
    gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 4, &PD_Low_phys);
    gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &PDP_High_phys);
    gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &PD_High_phys);
    gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 32, &PT_High_phys);
    
    UINT64 *PML4 = (UINT64*)PML4_phys;
    UINT64 *PDP_Low = (UINT64*)PDP_Low_phys;
    UINT64 *PD_Low = (UINT64*)PD_Low_phys;
    UINT64 *PDP_High = (UINT64*)PDP_High_phys;
    UINT64 *PD_High = (UINT64*)PD_High_phys;
    UINT64 *PT_High = (UINT64*)PT_High_phys;
    
    ZeroMem(PML4, 4096);
    ZeroMem(PDP_Low, 4096);
    ZeroMem(PD_Low, 4096 * 4);
    ZeroMem(PDP_High, 4096);
    ZeroMem(PD_High, 4096);
    ZeroMem(PT_High, 4096 * 32);
    
    // Identity map 0-4GB
    PML4[0] = (UINT64)PDP_Low | 3;
    PDP_Low[0] = (UINT64)PD_Low | 3;
    PDP_Low[1] = ((UINT64)PD_Low + 4096) | 3;
    PDP_Low[2] = ((UINT64)PD_Low + 8192) | 3;
    PDP_Low[3] = ((UINT64)PD_Low + 12288) | 3;
    
    for (int i = 0; i < 2048; i++) {
        PD_Low[i] = (i * 0x200000ull) | 0x83;
    }
    
    // Map KernelVirtualBase to KernelPhysicalBase
    UINT64 PML4_Index = (KernelVirtualBase >> 39) & 0x1FF;
    UINT64 PDP_Index = (KernelVirtualBase >> 30) & 0x1FF;
    
    PML4[PML4_Index] = (UINT64)PDP_High | 3;
    PDP_High[PDP_Index] = (UINT64)PD_High | 3;
    
    // Map 64MB for the kernel using 4KB pages
    for (int i = 0; i < 32; i++) {
        PD_High[i] = ((UINT64)PT_High + i * 4096) | 3;
    }
    
    for (int i = 0; i < 32 * 512; i++) {
        PT_High[i] = (KernelPhysicalBase + i * 4096ull) | 3;
    }
    
    LoadCr3((UINT64)PML4);
}

typedef struct {
    UINT64 start;
    UINT64 end;
    UINT32 type;
} MemoryMapEntry;

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    VOID *KernelEntry = NULL;
    UINT64 KernelBase = 0;

    Print(L"\n=============================\n");
    Print(L"  BanJiuOS ELF Boot Loader\n");
    Print(L"=============================\n\n");

    Status = LoadElfKernel(ImageHandle, SystemTable, L"\\System\\kernel.elf", &KernelEntry, &KernelBase);

    if (EFI_ERROR(Status) || KernelEntry == NULL) {
        Print(L"Error: Failed to load kernel (%r)\n", Status);
        return Status;
    }

    Print(L"\nStarting BanJiuOS...\n");
    Print(L"Virtual entry: 0x%lx\n", (UINTN)KernelEntry);
    Print(L"Physical base: 0x%lx\n", KernelBase);

    SetupPagingC(KernelBase, 0xFFFFFFFF80000000ull);

    FramebufferInfo FbInfo = {0};
    Status = GetGopInfo(&FbInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Warning: GOP not available, framebuffer will be disabled\n");
    }

    UINTN MapKey = 0;
    UINTN DescriptorSize = 0;
    UINT32 DescriptorVersion = 0;
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;

    Status = gBS->GetMemoryMap(&MemoryMapSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
        Print(L"Error: GetMemoryMap failed (%r)\n", Status);
        return Status;
    }

    MemoryMapSize += DescriptorSize * 16;
    Status = gBS->AllocatePool(EfiLoaderData, MemoryMapSize, (VOID**)&MemoryMap);
    if (EFI_ERROR(Status)) {
        Print(L"Error: AllocatePool failed (%r)\n", Status);
        return Status;
    }

    Status = gBS->GetMemoryMap(&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) {
        Print(L"Error: GetMemoryMap second call failed (%r)\n", Status);
        gBS->FreePool(MemoryMap);
        return Status;
    }

    UINTN NumEntries = MemoryMapSize / DescriptorSize;
    MemoryMapEntry *KernelMmap;
    Status = gBS->AllocatePool(EfiLoaderData, 512 * sizeof(MemoryMapEntry), (VOID**)&KernelMmap);
    if (EFI_ERROR(Status)) {
        Print(L"Error: AllocatePool for KernelMmap failed (%r)\n", Status);
        return Status;
    }

    // Now get the memory map AGAIN to get the correct MapKey for ExitBootServices
    MemoryMapSize += DescriptorSize * 16;
    Status = gBS->AllocatePool(EfiLoaderData, MemoryMapSize, (VOID**)&MemoryMap);
    Status = gBS->GetMemoryMap(&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);

    // Re-calculate NumEntries and convert the final map
    NumEntries = MemoryMapSize / DescriptorSize;
    if (NumEntries > 512) NumEntries = 512; // Prevent overflow
    
    for (UINTN i = 0; i < NumEntries; i++) {
        EFI_MEMORY_DESCRIPTOR *Desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + (i * DescriptorSize));
        KernelMmap[i].start = Desc->PhysicalStart;
        KernelMmap[i].end = Desc->PhysicalStart + Desc->NumberOfPages * 4096;
        
        // Only treat conventional memory as free at handoff time.
        // Loader pages currently hold the kernel image, page tables, memory map
        // buffers and the active stack, so reclaiming them here corrupts the
        // running kernel immediately after ExitBootServices.
        if (Desc->Type == EfiConventionalMemory) {
            KernelMmap[i].type = 1; // Free
        } else {
            KernelMmap[i].type = 0; // Reserved
        }
    }

    // We can't free memory here before ExitBootServices, so just leave it

    Status = gBS->ExitBootServices(ImageHandle, MapKey);
    if (EFI_ERROR(Status)) {
        Print(L"Error: ExitBootServices failed (%r)\n", Status);
        gBS->FreePool(MemoryMap);
        return Status;
    }

    typedef void (*KERNEL_ENTRY)(UINT64, UINT64, VOID*, FramebufferInfo*);
    KERNEL_ENTRY kernel_entry = (KERNEL_ENTRY)KernelEntry;
    kernel_entry((UINT64)KernelMmap, NumEntries * sizeof(MemoryMapEntry), (VOID*)SystemTable, &FbInfo);

    while (1);

    return EFI_SUCCESS;
}
