#ifndef EFI_ELF_LOADER_H
#define EFI_ELF_LOADER_H

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/SimpleFileSystem.h>

#define ELF_MAGIC       0x464C457F
#define ELF_CLASS_64    2
#define ELF_DATA_LSB     1
#define ELF_TYPE_EXEC    2
#define ELF_TYPE_DYN     3
#define ELF_MACHINE_X86_64 62

#define ELF_PT_LOAD     1
#define ELF_PT_DYNAMIC  2
#define ELF_PT_INTERP   3

#define ELF_PF_X        0x1
#define ELF_PF_W        0x2
#define ELF_PF_R        0x4

#pragma pack(push, 1)

typedef struct {
    UINT8  e_ident[16];
    UINT16 e_type;
    UINT16 e_machine;
    UINT32 e_version;
    UINT64 e_entry;
    UINT64 e_phoff;
    UINT64 e_shoff;
    UINT32 e_flags;
    UINT16 e_ehsize;
    UINT16 e_phentsize;
    UINT16 e_phnum;
    UINT16 e_shentsize;
    UINT16 e_shnum;
    UINT16 e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    UINT32 p_type;
    UINT32 p_flags;
    UINT64 p_offset;
    UINT64 p_vaddr;
    UINT64 p_paddr;
    UINT64 p_filesz;
    UINT64 p_memsz;
    UINT64 p_align;
} Elf64_Phdr;

typedef struct {
    UINT32 d_tag;
    UINT64 d_val;
} Elf64_Dyn;

#pragma pack(pop)

EFI_STATUS LoadElfKernel(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, CHAR16 *FilePath, VOID **KernelEntry, UINT64 *KernelBase);

#endif