#include "EfiElfLoader.h"

static BOOLEAN ValidateElfHeader(Elf64_Ehdr *ehdr) {
    if (ehdr->e_ident[0] != 0x7F ||
        ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' ||
        ehdr->e_ident[3] != 'F') {
        Print(L"Error: Invalid ELF magic\n");
        return FALSE;
    }

    if (ehdr->e_ident[4] != ELF_CLASS_64) {
        Print(L"Error: Not a 64-bit ELF (class=%d)\n", ehdr->e_ident[4]);
        return FALSE;
    }

    if (ehdr->e_ident[5] != ELF_DATA_LSB) {
        Print(L"Error: Not little-endian ELF\n");
        return FALSE;
    }

    if (ehdr->e_type != ELF_TYPE_EXEC && ehdr->e_type != ELF_TYPE_DYN) {
        Print(L"Error: Not an executable ELF (type=%d)\n", ehdr->e_type);
        return FALSE;
    }

    if (ehdr->e_machine != ELF_MACHINE_X86_64) {
        Print(L"Error: Not x86_64 ELF (machine=%d)\n", ehdr->e_machine);
        return FALSE;
    }

    return TRUE;
}

EFI_STATUS LoadElfKernel(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, CHAR16 *FilePath, VOID **KernelEntry, UINT64 *KernelBase) {
    EFI_STATUS Status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL *RootDir;
    EFI_FILE_PROTOCOL *File;
    UINTN FileSize;
    VOID *FileBuffer;
    Elf64_Ehdr *Ehdr;
    Elf64_Phdr *Phdrs;
    UINTN i;

    Print(L"Loading ELF kernel from: %s\n", FilePath);

    Status = gBS->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid, NULL, (VOID**)&FileSystem);
    if (EFI_ERROR(Status)) {
        Print(L"Error: Cannot locate file system protocol (%r)\n", Status);
        return Status;
    }

    Status = FileSystem->OpenVolume(FileSystem, &RootDir);
    if (EFI_ERROR(Status)) {
        Print(L"Error: Cannot open root directory (%r)\n", Status);
        return Status;
    }

    Status = RootDir->Open(RootDir, &File, FilePath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Error: Cannot open file %s (%r)\n", FilePath, Status);
        RootDir->Close(RootDir);
        return Status;
    }

    FileSize = 0x100000;
    Status = gBS->AllocatePool(EfiLoaderData, FileSize, (VOID**)&FileBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"Error: Cannot allocate buffer (%r)\n", Status);
        File->Close(File);
        RootDir->Close(RootDir);
        return Status;
    }

    UINTN ReadSize = FileSize;
    Status = File->Read(File, &ReadSize, FileBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"Error: Cannot read file (%r)\n", Status);
        gBS->FreePool(FileBuffer);
        File->Close(File);
        RootDir->Close(RootDir);
        return Status;
    }
    FileSize = ReadSize;

    File->Close(File);
    RootDir->Close(RootDir);

    Ehdr = (Elf64_Ehdr*)FileBuffer;

    if (!ValidateElfHeader(Ehdr)) {
        gBS->FreePool(FileBuffer);
        return EFI_INVALID_PARAMETER;
    }

    Print(L"ELF Header validated\n");
    Print(L"  Entry: 0x%lx\n", Ehdr->e_entry);
    Print(L"  Program Headers: %d\n", Ehdr->e_phnum);
    Print(L"  Section Headers: %d\n", Ehdr->e_shnum);

    Phdrs = (Elf64_Phdr*)((UINT8*)FileBuffer + Ehdr->e_phoff);

    UINT64 MinVaddr = 0xFFFFFFFFFFFFFFFF;
    UINT64 MaxVaddr = 0;
    UINT64 TotalMemSize = 0;

    for (i = 0; i < Ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = &Phdrs[i];
        if (phdr->p_type == ELF_PT_LOAD && phdr->p_memsz > 0) {
            UINT64 seg_end = phdr->p_vaddr + phdr->p_memsz;
            if (phdr->p_vaddr < MinVaddr) MinVaddr = phdr->p_vaddr;
            if (seg_end > MaxVaddr) MaxVaddr = seg_end;
            Print(L"  PH[%d]: type=LOAD, vaddr=0x%lx, filesz=0x%lx, memsz=0x%lx\n",
                  i, phdr->p_vaddr, phdr->p_filesz, phdr->p_memsz);
        }
    }

    TotalMemSize = MaxVaddr - MinVaddr;
    Print(L"\n  MinVaddr: 0x%lx, MaxVaddr: 0x%lx, TotalSize: 0x%lx\n", MinVaddr, MaxVaddr, TotalMemSize);

    EFI_PHYSICAL_ADDRESS PhysicalBase = 0x100000;
    Status = gBS->AllocatePages(AllocateAddress, EfiLoaderData,
                                EFI_SIZE_TO_PAGES(TotalMemSize),
                                &PhysicalBase);

    if (EFI_ERROR(Status)) {
        Print(L"  Warning: Cannot allocate at 0x%lx, trying any address...\n", PhysicalBase);
        PhysicalBase = 0x100000;
        Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData,
                                   EFI_SIZE_TO_PAGES(TotalMemSize),
                                   &PhysicalBase);
        if (EFI_ERROR(Status)) {
            Print(L"Error: Cannot allocate %d pages for kernel (%r)\n", EFI_SIZE_TO_PAGES(TotalMemSize), Status);
            gBS->FreePool(FileBuffer);
            return Status;
        }
    }

    Print(L"  Allocated %d bytes at physical base 0x%lx\n", TotalMemSize, PhysicalBase);

    UINT8 *DestBase = (UINT8*)(UINTN)PhysicalBase;

    for (i = 0; i < Ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = &Phdrs[i];
        if (phdr->p_type != ELF_PT_LOAD || phdr->p_memsz == 0) {
            continue;
        }

        UINT64 offset = phdr->p_vaddr - MinVaddr;
        VOID *Dest = DestBase + offset;

        if (phdr->p_filesz > 0) {
            CopyMem(Dest, (UINT8*)FileBuffer + phdr->p_offset, phdr->p_filesz);
        }

        if (phdr->p_memsz > phdr->p_filesz) {
            ZeroMem((UINT8*)Dest + phdr->p_filesz, phdr->p_memsz - phdr->p_filesz);
        }

        Print(L"  Loaded segment %d: %d bytes to offset 0x%lx (phys 0x%lx)\n",
              i, (UINTN)phdr->p_memsz, offset, (UINTN)Dest);
    }

    *KernelBase = PhysicalBase;
    *KernelEntry = (VOID*)((UINTN)Ehdr->e_entry);

    Print(L"\n  Physical base: 0x%lx\n", PhysicalBase);
    Print(L"  Virtual entry: 0x%lx\n", *KernelEntry);
    Print(L"\nELF Kernel loaded successfully!\n");

    gBS->FreePool(FileBuffer);

    return EFI_SUCCESS;
}