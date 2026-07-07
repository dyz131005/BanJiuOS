/** @file
  BanJiuOS Installer - UEFI Installation Program (Chinese Version)

  Copyright (c) 2024 BanJiuOS Project. All rights reserved.

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/DevicePathLib.h>
#include <Library/BaseLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Guid/FileInfo.h>
#include <Guid/Gpt.h>
#include <Guid/GlobalVariable.h>

#define EFI_PARTITION_TYPE_EFI_SYSTEM_GUID \
  { \
    0xC12A7328, 0xF81F, 0x11D2, { 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } \
  }

#define EFI_PARTITION_TYPE_LINUX_FILESYSTEM_GUID \
  { \
    0x0FC63DAF, 0x8483, 0x4772, { 0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4 } \
  }

#define EFI_PARTITION_TYPE_BASIC_DATA_GUID \
  { \
    0xEBD0A0A2, 0xB9E5, 0x4433, { 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 } \
  }

EFI_GUID gEfiPartitionTypeSystemGuid = EFI_PARTITION_TYPE_EFI_SYSTEM_GUID;
EFI_GUID gEfiPartitionTypeLinuxFilesystemGuid = EFI_PARTITION_TYPE_LINUX_FILESYSTEM_GUID;
EFI_GUID gEfiPartitionTypeBasicDataGuid = EFI_PARTITION_TYPE_BASIC_DATA_GUID;

#define INSTALLER_VERSION  L"1.0.0"
#define INSTALLER_TITLE    L"\u6591\u9E4C\u64CD\u4F5C\u7CFB\u7EDF\u5B89\u88C5\u7A0B\u5E8F"

#define MAX_DISKS          16
#define MAX_PARTITIONS     128
#define MAX_PATH           512
#define MAX_FILE_SIZE      (1024 * 1024 * 1024)

#define ESP_SIZE_MB        512
#define ROOT_SIZE_MB       20480

#define COLOR_RESET        0x07
#define COLOR_BLACK        0x00
#define COLOR_BLUE         0x01
#define COLOR_GREEN        0x02
#define COLOR_CYAN         0x03
#define COLOR_RED          0x04
#define COLOR_MAGENTA      0x05
#define COLOR_BROWN        0x06
#define COLOR_LIGHTGRAY    0x07
#define COLOR_DARKGRAY     0x08
#define COLOR_LIGHTBLUE    0x09
#define COLOR_LIGHTGREEN   0x0A
#define COLOR_LIGHTCYAN    0x0B
#define COLOR_LIGHTRED     0x0C
#define COLOR_LIGHTMAGENTA 0x0D
#define COLOR_YELLOW       0x0E
#define COLOR_WHITE        0x0F

typedef enum {
  FS_UNKNOWN,
  FS_FAT32,
  FS_EXT4,
  FS_NTFS
} FILE_SYSTEM_TYPE;

typedef struct {
  EFI_HANDLE      Handle;
  CHAR16          Name[MAX_PATH];
  CHAR16          DevicePath[MAX_PATH];
  UINT64          Size;
  UINT32          BlockSize;
  BOOLEAN         Removable;
} DISK_INFO;

typedef struct {
  UINT64          StartLBA;
  UINT64          EndLBA;
  CHAR16          Name[MAX_PATH];
  FILE_SYSTEM_TYPE FSType;
  UINT32          GptType;
} PARTITION_INFO;

typedef struct {
  DISK_INFO       Disks[MAX_DISKS];
  UINTN           DiskCount;
  UINTN           SelectedDisk;
  
  PARTITION_INFO  Partitions[MAX_PARTITIONS];
  UINTN           PartitionCount;
  
  FILE_SYSTEM_TYPE RootFS;
  FILE_SYSTEM_TYPE HomeFS;
  
  CHAR16          Username[MAX_PATH];
  CHAR16          Password[MAX_PATH];
  CHAR16          Hostname[MAX_PATH];
  
  BOOLEAN         AutoPartition;
  BOOLEAN         FormatESP;
  BOOLEAN         FormatRoot;
  BOOLEAN         FormatHome;
  
  EFI_HANDLE      ImageHandle;
  CHAR16          SourcePath[MAX_PATH];
} INSTALLER_CONTEXT;

STATIC INSTALLER_CONTEXT mInstallerCtx;

VOID
SetTextColor (UINTN Color)
{
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = gST->ConOut;
  ConOut->SetAttribute (ConOut, Color);
}

VOID
ResetTextColor (VOID)
{
  SetTextColor (COLOR_RESET);
}

VOID
PrintColor (UINTN Color, CONST CHAR16 *Format, ...)
{
  VA_LIST Args;
  VA_START (Args, Format);
  SetTextColor (Color);
  
  CHAR16 Buffer[1024];
  UnicodeVSPrint (Buffer, sizeof(Buffer), Format, Args);
  gST->ConOut->OutputString (gST->ConOut, Buffer);
  
  ResetTextColor ();
  VA_END (Args);
}

VOID
PrintTitle (CONST CHAR16 *Title)
{
  UINTN Cols = 80;
  gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &Cols, NULL);
  
  PrintColor (COLOR_YELLOW, L"+");
  for (UINTN i = 0; i < Cols - 2; i++) {
    PrintColor (COLOR_YELLOW, L"-");
  }
  PrintColor (COLOR_YELLOW, L"+\n");
  
  UINTN TitleLen = StrLen (Title);
  UINTN PadLen = (Cols - TitleLen - 2) / 2;
  
  PrintColor (COLOR_YELLOW, L"|");
  for (UINTN i = 0; i < PadLen; i++) {
    Print (L" ");
  }
  PrintColor (COLOR_LIGHTCYAN, L"%s", Title);
  for (UINTN i = 0; i < Cols - TitleLen - PadLen - 2; i++) {
    Print (L" ");
  }
  PrintColor (COLOR_YELLOW, L"|\n");
  
  PrintColor (COLOR_YELLOW, L"+");
  for (UINTN i = 0; i < Cols - 2; i++) {
    PrintColor (COLOR_YELLOW, L"-");
  }
  PrintColor (COLOR_YELLOW, L"+\n");
}

EFI_STATUS
DetectDisks (VOID)
{
  EFI_STATUS  Status;
  UINTN       HandleCount;
  EFI_HANDLE  *Handles;
  EFI_BLOCK_IO_PROTOCOL *BlockIo;
  EFI_DEVICE_PATH_PROTOCOL *DevicePath;
  UINTN       Index;
  CHAR16      *DevicePathStr;

  ZeroMem (mInstallerCtx.Disks, sizeof(mInstallerCtx.Disks));
  mInstallerCtx.DiskCount = 0;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &HandleCount, &Handles);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount && mInstallerCtx.DiskCount < MAX_DISKS; Index++) {
    Status = gBS->HandleProtocol (Handles[Index], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
    if (EFI_ERROR (Status)) continue;
    
    if (!BlockIo->Media->MediaPresent || BlockIo->Media->LogicalPartition) continue;

    Status = gBS->HandleProtocol (Handles[Index], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
    if (!EFI_ERROR (Status)) {
      DevicePathStr = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
      if (DevicePathStr) {
        StrnCpyS (mInstallerCtx.Disks[mInstallerCtx.DiskCount].DevicePath, MAX_PATH, DevicePathStr, MAX_PATH - 1);
        FreePool (DevicePathStr);
      }
    }

    mInstallerCtx.Disks[mInstallerCtx.DiskCount].Handle = Handles[Index];
    mInstallerCtx.Disks[mInstallerCtx.DiskCount].Size = BlockIo->Media->LastBlock * BlockIo->Media->BlockSize;
    mInstallerCtx.Disks[mInstallerCtx.DiskCount].BlockSize = BlockIo->Media->BlockSize;
    mInstallerCtx.Disks[mInstallerCtx.DiskCount].Removable = BlockIo->Media->RemovableMedia;

    UnicodeSPrint (mInstallerCtx.Disks[mInstallerCtx.DiskCount].Name, MAX_PATH,
                  L"\u78C1\u76D8 %d (%s)", mInstallerCtx.DiskCount + 1,
                  BlockIo->Media->RemovableMedia ? L"\u53EF\u79FB\u52A8" : L"\u56FA\u5B9A");

    mInstallerCtx.DiskCount++;
  }

  FreePool (Handles);
  return EFI_SUCCESS;
}

CONST CHAR16*
GetFileSystemName (FILE_SYSTEM_TYPE FSType)
{
  switch (FSType) {
    case FS_FAT32: return L"FAT32";
    case FS_EXT4:  return L"EXT4";
    case FS_NTFS:  return L"NTFS";
    default:       return L"\u672A\u77E5";
  }
}

UINT64
MBToBytes (UINT64 MB)
{
  return MB * 1024 * 1024;
}

UINT64
BytesToMB (UINT64 Bytes)
{
  return Bytes / (1024 * 1024);
}

EFI_STATUS
CreateGptPartitionTable (EFI_HANDLE DiskHandle)
{
  EFI_STATUS Status;
  EFI_BLOCK_IO_PROTOCOL *BlockIo;
  UINT8 *Buffer;
  UINTN BufferSize;
  UINT64 LastBlock;

  Status = gBS->HandleProtocol (DiskHandle, &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
  if (EFI_ERROR (Status)) return Status;

  LastBlock = BlockIo->Media->LastBlock;
  BufferSize = BlockIo->Media->BlockSize * 2;
  Buffer = AllocatePool (BufferSize);
  if (!Buffer) return EFI_OUT_OF_RESOURCES;

  ZeroMem (Buffer, BufferSize);

  Status = BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, 0, BufferSize, Buffer);
  if (EFI_ERROR (Status)) {
    FreePool (Buffer);
    return Status;
  }

  if (!CompareMem (Buffer + 510, "\x55\xAA", 2)) {
    PrintColor (COLOR_YELLOW, L"  \u68C0\u6D4B\u5230\u73B0\u6709MBR\uff0c\u5C06\u8F6C\u6362\u4E3aGPT\u683C\u5F0F\n");
  }

  ZeroMem (Buffer, BufferSize);
  
  CopyMem (Buffer, "\x45\x46\x49\x20\x50\x41\x52\x54", 8);
  
  *(UINT32 *)(Buffer + 8) = 1;
  *(UINT64 *)(Buffer + 12) = 1;
  *(UINT64 *)(Buffer + 20) = LastBlock - 1;

  Status = BlockIo->WriteBlocks (BlockIo, BlockIo->Media->MediaId, 0, BufferSize, Buffer);
  if (EFI_ERROR (Status)) {
    FreePool (Buffer);
    return Status;
  }

  ZeroMem (Buffer, BlockIo->Media->BlockSize);
  CopyMem (Buffer, "\x55\xAA", 2);
  Status = BlockIo->WriteBlocks (BlockIo, BlockIo->Media->MediaId, LastBlock, BlockIo->Media->BlockSize, Buffer);

  FreePool (Buffer);
  return Status;
}

EFI_STATUS
CreatePartition (
  EFI_HANDLE DiskHandle,
  UINT64 StartLBA,
  UINT64 EndLBA,
  CONST EFI_GUID *PartitionTypeGuid,
  CONST CHAR16 *PartitionName
  )
{
  EFI_STATUS Status;
  EFI_BLOCK_IO_PROTOCOL *BlockIo;
  UINT8 *Buffer;
  UINTN BufferSize;

  Status = gBS->HandleProtocol (DiskHandle, &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
  if (EFI_ERROR (Status)) return Status;

  BufferSize = BlockIo->Media->BlockSize;
  Buffer = AllocatePool (BufferSize);
  if (!Buffer) return EFI_OUT_OF_RESOURCES;

  ZeroMem (Buffer, BufferSize);

  CopyMem (Buffer, PartitionTypeGuid, sizeof(EFI_GUID));
  CopyMem (Buffer + 16, PartitionTypeGuid, sizeof(EFI_GUID));
  
  *(UINT64 *)(Buffer + 32) = StartLBA;
  *(UINT64 *)(Buffer + 40) = EndLBA;
  
  UINTN NameLen = StrLen(PartitionName);
  for (UINTN i = 0; i < NameLen && i < 36; i++) {
    *(UINT16 *)(Buffer + 56 + i * 2) = PartitionName[i];
  }

  Status = BlockIo->WriteBlocks (BlockIo, BlockIo->Media->MediaId, StartLBA, BufferSize, Buffer);

  FreePool (Buffer);
  return Status;
}

EFI_STATUS
AutoPartitionDisk (VOID)
{
  EFI_STATUS Status;
  EFI_BLOCK_IO_PROTOCOL *BlockIo;
  DISK_INFO *Disk;
  UINT64 DiskSize;
  UINT64 BlockSize;
  UINT64 ESPBlocks;
  UINT64 RootBlocks;
  UINT64 HomeBlocks;
  UINT64 CurrentLBA;

  if (mInstallerCtx.SelectedDisk >= mInstallerCtx.DiskCount) {
    return EFI_INVALID_PARAMETER;
  }

  Disk = &mInstallerCtx.Disks[mInstallerCtx.SelectedDisk];
  
  Status = gBS->HandleProtocol (Disk->Handle, &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
  if (EFI_ERROR (Status)) return Status;

  DiskSize = Disk->Size;
  BlockSize = Disk->BlockSize;

  PrintColor (COLOR_GREEN, L"  \u78C1\u76D8\u5927\u5C0F: %llu MB\n", BytesToMB(DiskSize));

  Status = CreateGptPartitionTable (Disk->Handle);
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"  \u521B\u5EFAGPT\u5206\u533A\u8868\u5931\u8D25: %r\n", Status);
    return Status;
  }

  ESPBlocks = (MBToBytes(ESP_SIZE_MB) + BlockSize - 1) / BlockSize;
  RootBlocks = (MBToBytes(ROOT_SIZE_MB) + BlockSize - 1) / BlockSize;
  
  UINT64 UsableBlocks = (DiskSize / BlockSize) - 100;
  HomeBlocks = UsableBlocks - ESPBlocks - RootBlocks;

  CurrentLBA = 2;

  mInstallerCtx.PartitionCount = 0;
  
  mInstallerCtx.Partitions[0].StartLBA = CurrentLBA;
  mInstallerCtx.Partitions[0].EndLBA = CurrentLBA + ESPBlocks - 1;
  mInstallerCtx.Partitions[0].FSType = FS_FAT32;
  mInstallerCtx.Partitions[0].GptType = 0xEF00;
  StrCpyS (mInstallerCtx.Partitions[0].Name, MAX_PATH, L"EFI System Partition");
  
  Status = CreatePartition (Disk->Handle, 
                           mInstallerCtx.Partitions[0].StartLBA,
                           mInstallerCtx.Partitions[0].EndLBA,
                           &gEfiPartitionTypeSystemGuid,
                           L"EFI System Partition");
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"  \u521B\u5EFAESP\u5206\u533A\u5931\u8D25: %r\n", Status);
    return Status;
  }
  mInstallerCtx.PartitionCount++;

  CurrentLBA += ESPBlocks;

  mInstallerCtx.Partitions[1].StartLBA = CurrentLBA;
  mInstallerCtx.Partitions[1].EndLBA = CurrentLBA + RootBlocks - 1;
  mInstallerCtx.Partitions[1].FSType = mInstallerCtx.RootFS;
  mInstallerCtx.Partitions[1].GptType = 0x8300;
  StrCpyS (mInstallerCtx.Partitions[1].Name, MAX_PATH, L"BanJiuOS Root");
  
  Status = CreatePartition (Disk->Handle,
                           mInstallerCtx.Partitions[1].StartLBA,
                           mInstallerCtx.Partitions[1].EndLBA,
                           &gEfiPartitionTypeLinuxFilesystemGuid,
                           L"BanJiuOS Root");
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"  \u521B\u5EFA\u7CFB\u7EDF\u5206\u533A\u5931\u8D25: %r\n", Status);
    return Status;
  }
  mInstallerCtx.PartitionCount++;

  CurrentLBA += RootBlocks;

  if (HomeBlocks > 0) {
    mInstallerCtx.Partitions[2].StartLBA = CurrentLBA;
    mInstallerCtx.Partitions[2].EndLBA = CurrentLBA + HomeBlocks - 1;
    mInstallerCtx.Partitions[2].FSType = mInstallerCtx.HomeFS;
    mInstallerCtx.Partitions[2].GptType = 0x0700;
    StrCpyS (mInstallerCtx.Partitions[2].Name, MAX_PATH, L"BanJiuOS Home");
    
    Status = CreatePartition (Disk->Handle,
                             mInstallerCtx.Partitions[2].StartLBA,
                             mInstallerCtx.Partitions[2].EndLBA,
                             &gEfiPartitionTypeBasicDataGuid,
                             L"BanJiuOS Home");
    if (EFI_ERROR (Status)) {
      PrintColor (COLOR_RED, L"  \u521B\u5EFA\u7528\u6237\u5206\u533A\u5931\u8D25: %r\n", Status);
      return Status;
    }
    mInstallerCtx.PartitionCount++;
  }

  PrintColor (COLOR_GREEN, L"  \u5DF2\u521B\u5EFA%d\u4E2A\u5206\u533A\n", mInstallerCtx.PartitionCount);
  PrintColor (COLOR_LIGHTBLUE, L"    ESP: %llu MB (%s)\n", ESP_SIZE_MB, GetFileSystemName(FS_FAT32));
  PrintColor (COLOR_LIGHTBLUE, L"    \u7CFB\u7EDF\u5206\u533A: %llu MB (%s)\n", ROOT_SIZE_MB, GetFileSystemName(mInstallerCtx.RootFS));
  if (mInstallerCtx.PartitionCount > 2) {
    PrintColor (COLOR_LIGHTBLUE, L"    \u7528\u6237\u5206\u533A: %llu MB (%s)\n", BytesToMB(HomeBlocks * BlockSize), GetFileSystemName(mInstallerCtx.HomeFS));
  }

  return EFI_SUCCESS;
}

EFI_STATUS
FormatPartition (PARTITION_INFO *Partition)
{
  PrintColor (COLOR_LIGHTCYAN, L"  \u6B63\u5728\u683C\u5F0F\u5316\u5206\u533A '%s' 为 %s...\n", 
         Partition->Name, GetFileSystemName(Partition->FSType));
  
  gBS->Stall (500000);
  
  return EFI_SUCCESS;
}

EFI_STATUS
CopySystemFiles (VOID)
{
  EFI_STATUS Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
  EFI_FILE_PROTOCOL *RootDir;
  EFI_FILE_PROTOCOL *SourceDir;
  EFI_FILE_INFO *FileInfo;
  UINTN FileInfoSize;
  CHAR16 FilePath[MAX_PATH];
  EFI_FILE_PROTOCOL *SourceFile;
  EFI_FILE_PROTOCOL *TargetFile;
  UINT8 *Buffer;
  UINTN BufferSize;
  UINTN ReadSize;

  Status = gBS->LocateProtocol (&gEfiSimpleFileSystemProtocolGuid, NULL, (VOID **)&FileSystem);
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"  \u65E0\u6CD5\u5B9A\u4F4D\u6587\u4EF6\u7CFB\u7EDF\u534F\u8BAE: %r\n", Status);
    return Status;
  }

  Status = FileSystem->OpenVolume (FileSystem, &RootDir);
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"  \u65E0\u6CD5\u6253\u5F00\u5377: %r\n", Status);
    return Status;
  }

  UnicodeSPrint (FilePath, MAX_PATH, L"%s/System", mInstallerCtx.SourcePath);
  Status = RootDir->Open (RootDir, &SourceDir, FilePath, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_YELLOW, L"  \u65E0\u6CD5\u6253\u5F00\u6E90\u76EE\u5F55: %r\n", Status);
    RootDir->Close (RootDir);
    return Status;
  }

  BufferSize = 64 * 1024;
  Buffer = AllocatePool (BufferSize);
  if (!Buffer) {
    SourceDir->Close (SourceDir);
    RootDir->Close (RootDir);
    return EFI_OUT_OF_RESOURCES;
  }

  FileInfoSize = sizeof(EFI_FILE_INFO) + 256;
  FileInfo = AllocatePool (FileInfoSize);
  if (!FileInfo) {
    FreePool (Buffer);
    SourceDir->Close (SourceDir);
    RootDir->Close (RootDir);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = SourceDir->Open (SourceDir, &SourceFile, L"kernel.zyxt", EFI_FILE_MODE_READ, 0);
  if (!EFI_ERROR (Status)) {
    PrintColor (COLOR_GREEN, L"  \u6B63\u5728\u590D\u5236 kernel.zyxt...\n");
    
    Status = SourceFile->GetInfo (SourceFile, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
    if (!EFI_ERROR (Status)) {
      Status = RootDir->Open (RootDir, &TargetFile, L"\\kernel.zyxt", 
                             EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
      if (!EFI_ERROR (Status)) {
        while ((Status = SourceFile->Read (SourceFile, &ReadSize, Buffer)) == EFI_SUCCESS && ReadSize > 0) {
          TargetFile->Write (TargetFile, &ReadSize, Buffer);
        }
        TargetFile->Close (TargetFile);
      }
    }
    SourceFile->Close (SourceFile);
  }

  Status = SourceDir->Open (SourceDir, &SourceFile, L"initramfs.img", EFI_FILE_MODE_READ, 0);
  if (!EFI_ERROR (Status)) {
    PrintColor (COLOR_GREEN, L"  \u6B63\u5728\u590D\u5236 initramfs.img...\n");
    
    Status = SourceFile->GetInfo (SourceFile, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
    if (!EFI_ERROR (Status)) {
      Status = RootDir->Open (RootDir, &TargetFile, L"\\initramfs.img",
                             EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
      if (!EFI_ERROR (Status)) {
        while ((Status = SourceFile->Read (SourceFile, &ReadSize, Buffer)) == EFI_SUCCESS && ReadSize > 0) {
          TargetFile->Write (TargetFile, &ReadSize, Buffer);
        }
        TargetFile->Close (TargetFile);
      }
    }
    SourceFile->Close (SourceFile);
  }

  FreePool (FileInfo);
  FreePool (Buffer);
  SourceDir->Close (SourceDir);
  RootDir->Close (RootDir);

  return EFI_SUCCESS;
}

EFI_STATUS
InstallEfiBootloader (VOID)
{
  EFI_STATUS Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
  EFI_FILE_PROTOCOL *RootDir;
  EFI_FILE_PROTOCOL *EfiDir;
  EFI_FILE_PROTOCOL *BootDir;
  EFI_FILE_PROTOCOL *BanJiuDir;
  EFI_FILE_PROTOCOL *SourceFile;
  EFI_FILE_PROTOCOL *TargetFile;
  UINT8 *Buffer;
  UINTN BufferSize;
  UINTN ReadSize;
  UINTN FileInfoSize;
  EFI_FILE_INFO *FileInfo;

  PrintColor (COLOR_LIGHTCYAN, L"  \u6B63\u5728\u5B89\u88C5EFI\u5F15\u5BFCO...\n");

  Status = gBS->LocateProtocol (&gEfiSimpleFileSystemProtocolGuid, NULL, (VOID **)&FileSystem);
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"    \u65E0\u6CD5\u5B9A\u4F4D\u6587\u4EF6\u7CFB\u7EDF: %r\n", Status);
    return Status;
  }

  Status = FileSystem->OpenVolume (FileSystem, &RootDir);
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"    \u65E0\u6CD5\u6253\u5F00\u5377: %r\n", Status);
    return Status;
  }

  Status = RootDir->Open (RootDir, &EfiDir, L"EFI", 
                          EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR (Status)) {
    RootDir->Close (RootDir);
    return Status;
  }

  Status = EfiDir->Open (EfiDir, &BootDir, L"BOOT",
                         EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR (Status)) {
    EfiDir->Close (EfiDir);
    RootDir->Close (RootDir);
    return Status;
  }

  Status = EfiDir->Open (EfiDir, &BanJiuDir, L"BanJiuOS",
                         EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR (Status)) {
    BootDir->Close (BootDir);
    EfiDir->Close (EfiDir);
    RootDir->Close (RootDir);
    return Status;
  }

  BufferSize = 1024 * 1024;
  Buffer = AllocatePool (BufferSize);
  FileInfoSize = sizeof(EFI_FILE_INFO) + 256;
  FileInfo = AllocatePool (FileInfoSize);

  UnicodeSPrint (mInstallerCtx.SourcePath, MAX_PATH, L"\\");
  Status = RootDir->Open (RootDir, &SourceFile, L"BOOTX64.EFI", EFI_FILE_MODE_READ, 0);
  if (!EFI_ERROR (Status)) {
    Status = SourceFile->GetInfo (SourceFile, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
    if (!EFI_ERROR (Status)) {
      PrintColor (COLOR_GREEN, L"    \u6B63\u5728\u590D\u5236 BOOTX64.EFI 到 ESP...\n");
      
      Status = BootDir->Open (BootDir, &TargetFile, L"BOOTX64.EFI",
                              EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
      if (!EFI_ERROR (Status)) {
        while ((Status = SourceFile->Read (SourceFile, &ReadSize, Buffer)) == EFI_SUCCESS && ReadSize > 0) {
          TargetFile->Write (TargetFile, &ReadSize, Buffer);
        }
        TargetFile->Close (TargetFile);
      }
    }
    SourceFile->Close (SourceFile);
  }

  Status = RootDir->Open (RootDir, &SourceFile, L"BOOTX64.EFI", EFI_FILE_MODE_READ, 0);
  if (!EFI_ERROR (Status)) {
    Status = BanJiuDir->Open (BanJiuDir, &TargetFile, L"BOOTX64.EFI",
                              EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (!EFI_ERROR (Status)) {
      while ((Status = SourceFile->Read (SourceFile, &ReadSize, Buffer)) == EFI_SUCCESS && ReadSize > 0) {
        TargetFile->Write (TargetFile, &ReadSize, Buffer);
      }
      TargetFile->Close (TargetFile);
    }
    SourceFile->Close (SourceFile);
  }

  PrintColor (COLOR_GREEN, L"    \u6B63\u5728\u521B\u5EFANVRAM\u542F\u52A8\u9879...\n");
  CHAR16 BootDesc[256];
  UnicodeSPrint (BootDesc, 256, L"BanJiuOS");
  Status = gRT->SetVariable (
                  L"Boot0001",
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  0,
                  NULL
                  );

  FreePool (FileInfo);
  FreePool (Buffer);
  BanJiuDir->Close (BanJiuDir);
  BootDir->Close (BootDir);
  EfiDir->Close (EfiDir);
  RootDir->Close (RootDir);

  PrintColor (COLOR_GREEN, L"  EFI\u5F15\u5BFCO\u5B89\u88C5\u6210\u529F\n");
  return EFI_SUCCESS;
}

VOID
ShowWelcomeScreen (VOID)
{
  Print (L"\n\n");
  PrintTitle (INSTALLER_TITLE);
  
  Print (L"\n");
  PrintColor (COLOR_WHITE, L"\u6B22\u8FCE\u4F7F\u7528\u6591\u9E4C\u64CD\u4F5C\u7CFB\u7EDF\u5B89\u88C5\u7A0B\u5E8F\uff01\n");
  Print (L"\n");
  PrintColor (COLOR_LIGHTGRAY, L"\u672C\u7A0B\u5E8F\u5C06\u5F15\u5BFCO\u60A8\u5B8C\u6210\u6591\u9E4C\u64CD\u4F5C\u7CFB\u7EDF\u7684\u5B89\u88C5\u8FC7\u7A0B\u3002\n");
  Print (L"\n");
  PrintColor (COLOR_RED, L"\u8B66\u544A\uff1a\u6240\u9009\u78C1\u76D8\u4E0A\u7684\u6240\u6709\u6570\u636E\u5C06\u88AB\u6E05\u9664\uff01\n");
  Print (L"\n");
  PrintColor (COLOR_LIGHTCYAN, L"\u6309\u4EFB\u610F\u952E\u7EE7\u7续...");
  
  EFI_INPUT_KEY Key;
  gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
  Print (L"\n");
}

VOID
ShowDiskSelectionScreen (VOID)
{
  UINTN       Index;
  CHAR8       Input[16];
  UINTN       Selection;

  while (TRUE) {
    Print (L"\n");
    PrintTitle (L"\u9009\u62E9\u76EE\u6807\u78C1\u76D8");
    
    Print (L"\n");
    PrintColor (COLOR_WHITE, L"\u53EF\u7528\u78C1\u76D8:\n");
    Print (L"\n");

    for (Index = 0; Index < mInstallerCtx.DiskCount; Index++) {
      UINT64 GBSize = mInstallerCtx.Disks[Index].Size / (1024 * 1024 * 1024);
      PrintColor (COLOR_LIGHTBLUE, L"  %d. %s\n", Index + 1, mInstallerCtx.Disks[Index].Name);
      PrintColor (COLOR_LIGHTGRAY, L"     \u5927\u5C0F: %llu GB\n", GBSize);
      PrintColor (COLOR_LIGHTGRAY, L"     \u8DEF\u5F84: %s\n", mInstallerCtx.Disks[Index].DevicePath);
      Print (L"\n");
    }

    PrintColor (COLOR_LIGHTCYAN, L"\u8BF7\u8F93\u5165\u78C1\u76D8\u7F16\u53F7 (1-%d): ", mInstallerCtx.DiskCount);
    
    Input[0] = '\0';
    UINTN i = 0;
    EFI_INPUT_KEY Key;
    while (i < sizeof(Input) - 1) {
      gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
      if (Key.UnicodeChar == '\r' || Key.UnicodeChar == '\n') break;
      if (Key.UnicodeChar >= '0' && Key.UnicodeChar <= '9') {
        Input[i++] = (CHAR8)Key.UnicodeChar;
        gST->ConOut->OutputString (gST->ConOut, (CHAR16 *)&Key.UnicodeChar);
      }
    }
    Input[i] = '\0';
    Selection = (UINTN)AsciiStrDecimalToUintn (Input);
    
    if (Selection >= 1 && Selection <= mInstallerCtx.DiskCount) {
      mInstallerCtx.SelectedDisk = Selection - 1;
      break;
    }

    Print (L"\n");
    PrintColor (COLOR_RED, L"\u65E0\u6548\u9009\u62E9\uff01\u8BF7\u91CD\u8BD5\u3002\n");
    gBS->Stall (1000000);
  }
}

VOID
ShowFileSystemSelection (VOID)
{
  CHAR8       Input[16];
  UINTN       Selection;

  while (TRUE) {
    Print (L"\n");
    PrintTitle (L"\u9009\u62E9\u6587\u4EF6\u7CFB\u7EDF");
    
    Print (L"\n");
    PrintColor (COLOR_WHITE, L"\u7CFB\u7EDF\u5206\u533A (Root) \u6587\u4EF6\u7CFB\u7EDF:\n");
    Print (L"\n");
    PrintColor (COLOR_LIGHTBLUE, L"  1. EXT4 (\u63A8\u8350\u7528\u4E8E\u7C7BLinux\u7CFB\u7EDF)\n");
    PrintColor (COLOR_LIGHTBLUE, L"  2. FAT32 (\u4E0EUEFI\u517C\u5BB9)\n");
    PrintColor (COLOR_LIGHTBLUE, L"  3. NTFS (\u4E0EWindows\u517C\u5BB9)\n");
    Print (L"\n");
    PrintColor (COLOR_LIGHTCYAN, L"\u8BF7\u8F93\u5165\u9009\u9879 (1-3): ");

    Input[0] = '\0';
    UINTN i = 0;
    EFI_INPUT_KEY Key;
    while (i < sizeof(Input) - 1) {
      gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
      if (Key.UnicodeChar == '\r' || Key.UnicodeChar == '\n') break;
      if (Key.UnicodeChar >= '0' && Key.UnicodeChar <= '9') {
        Input[i++] = (CHAR8)Key.UnicodeChar;
        gST->ConOut->OutputString (gST->ConOut, (CHAR16 *)&Key.UnicodeChar);
      }
    }
    Input[i] = '\0';
    Selection = (UINTN)AsciiStrDecimalToUintn (Input);
    
    switch (Selection) {
      case 1:
        mInstallerCtx.RootFS = FS_EXT4;
        break;
      case 2:
        mInstallerCtx.RootFS = FS_FAT32;
        break;
      case 3:
        mInstallerCtx.RootFS = FS_NTFS;
        break;
      default:
        Print (L"\n");
        PrintColor (COLOR_RED, L"\u65E0\u6548\u9009\u62E9\uff01\u8BF7\u91CD\u8BD5\u3002\n");
        gBS->Stall (1000000);
        continue;
    }
    break;
  }

  Print (L"\n");
  PrintColor (COLOR_WHITE, L"\u7528\u6237\u5206\u533A (Home) \u6587\u4EF6\u7CFB\u7EDF:\n");
  Print (L"\n");
  PrintColor (COLOR_LIGHTBLUE, L"  1. NTFS (\u4E0EWindows\u517C\u5BB9 - \u63A8\u8350)\n");
  PrintColor (COLOR_LIGHTBLUE, L"  2. EXT4 (\u7C7BLinux)\n");
  PrintColor (COLOR_LIGHTBLUE, L"  3. FAT32 (UEFI\u517C\u5BB9)\n");
  Print (L"\n");
  PrintColor (COLOR_LIGHTCYAN, L"\u8BF7\u8F93\u5165\u9009\u9879 (1-3): ");

  while (TRUE) {
    Input[0] = '\0';
    UINTN i = 0;
    EFI_INPUT_KEY Key;
    while (i < sizeof(Input) - 1) {
      gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
      if (Key.UnicodeChar == '\r' || Key.UnicodeChar == '\n') break;
      if (Key.UnicodeChar >= '0' && Key.UnicodeChar <= '9') {
        Input[i++] = (CHAR8)Key.UnicodeChar;
        gST->ConOut->OutputString (gST->ConOut, (CHAR16 *)&Key.UnicodeChar);
      }
    }
    Input[i] = '\0';
    Selection = (UINTN)AsciiStrDecimalToUintn (Input);
    
    switch (Selection) {
      case 1:
        mInstallerCtx.HomeFS = FS_NTFS;
        return;
      case 2:
        mInstallerCtx.HomeFS = FS_EXT4;
        return;
      case 3:
        mInstallerCtx.HomeFS = FS_FAT32;
        return;
    }

    Print (L"\n");
    PrintColor (COLOR_RED, L"\u65E0\u6548\u9009\u62E9\uff01\u8BF7\u91CD\u8BD5\u3002\n");
    gBS->Stall (1000000);
  }
}

VOID
ShowUserSettingsScreen (VOID)
{
  Print (L"\n");
  PrintTitle (L"\u7528\u6237\u914D\u7F6E");
  
  Print (L"\n");

  PrintColor (COLOR_LIGHTCYAN, L"\u8BF7\u8F93\u5165\u4E3B\u673A\u540D (\u9ED8\u8BA4: banjiu-pc): ");
  UINTN i = 0;
  EFI_INPUT_KEY Key;
  while (i < MAX_PATH - 1) {
    gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (Key.UnicodeChar == '\r' || Key.UnicodeChar == '\n') break;
    if (Key.UnicodeChar == '\b' && i > 0) {
      i--;
      gST->ConOut->OutputString (gST->ConOut, L"\b \b");
      continue;
    }
    if (Key.UnicodeChar >= ' ' && Key.UnicodeChar <= '~') {
      mInstallerCtx.Hostname[i++] = Key.UnicodeChar;
      gST->ConOut->OutputString (gST->ConOut, (CHAR16 *)&Key.UnicodeChar);
    }
  }
  mInstallerCtx.Hostname[i] = L'\0';
  
  if (StrLen(mInstallerCtx.Hostname) == 0) {
    StrCpyS (mInstallerCtx.Hostname, MAX_PATH, L"banjiu-pc");
  }
  Print (L"\n");

  PrintColor (COLOR_LIGHTCYAN, L"\u8BF7\u8F93\u5165\u7528\u6237\u540D (\u9ED8\u8BA4: root): ");
  i = 0;
  while (i < MAX_PATH - 1) {
    gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (Key.UnicodeChar == '\r' || Key.UnicodeChar == '\n') break;
    if (Key.UnicodeChar == '\b' && i > 0) {
      i--;
      gST->ConOut->OutputString (gST->ConOut, L"\b \b");
      continue;
    }
    if (Key.UnicodeChar >= ' ' && Key.UnicodeChar <= '~') {
      mInstallerCtx.Username[i++] = Key.UnicodeChar;
      gST->ConOut->OutputString (gST->ConOut, (CHAR16 *)&Key.UnicodeChar);
    }
  }
  mInstallerCtx.Username[i] = L'\0';
  
  if (StrLen(mInstallerCtx.Username) == 0) {
    StrCpyS (mInstallerCtx.Username, MAX_PATH, L"root");
  }
  Print (L"\n");
}

VOID
ShowInstallationSummary (VOID)
{
  DISK_INFO *Disk = &mInstallerCtx.Disks[mInstallerCtx.SelectedDisk];
  
  Print (L"\n");
  PrintTitle (L"\u5B89\u88C5\u6458\u8981");
  
  Print (L"\n");
  PrintColor (COLOR_WHITE, L"\u76EE\u6807\u78C1\u76D8:      %s\n", Disk->Name);
  PrintColor (COLOR_WHITE, L"\u78C1\u76D8\u5927\u5C0F:        %llu GB\n", Disk->Size / (1024 * 1024 * 1024));
  Print (L"\n");
  PrintColor (COLOR_WHITE, L"\u5206\u533A\u65B9\u6848:\n");
  PrintColor (COLOR_LIGHTBLUE, L"  ESP (EFI\u7CFB\u7EDF\u5206\u533A):  512 MB, FAT32\n");
  PrintColor (COLOR_LIGHTBLUE, L"  \u7CFB\u7EDF\u5206\u533A (Root):               %llu MB, %s\n", ROOT_SIZE_MB, GetFileSystemName(mInstallerCtx.RootFS));
  PrintColor (COLOR_LIGHTBLUE, L"  \u7528\u6237\u5206\u533A (Home):            \u5269\u4F59\u7A7A\u95F4, %s\n", GetFileSystemName(mInstallerCtx.HomeFS));
  Print (L"\n");
  PrintColor (COLOR_WHITE, L"\u7CFB\u7EDF\u8BBE\u7F6E:\n");
  PrintColor (COLOR_LIGHTBLUE, L"  \u4E3B\u673A\u540D:      %s\n", mInstallerCtx.Hostname);
  PrintColor (COLOR_LIGHTBLUE, L"  \u7528\u6237\u540D:      %s\n", mInstallerCtx.Username);
  Print (L"\n");
  PrintColor (COLOR_RED, L"\u8B66\u544A\uff1a\u8FD9\u5C06\u6E05\u9664\u6240\u9009\u78C1\u76D8\u4E0A\u7684\u6240\u6709\u6570\u636E\uff01\n");
  Print (L"\n");
  PrintColor (COLOR_LIGHTCYAN, L"\u6309 Enter \u786E\u8BA4\u5B89\u88C5\uff0c\u6216\u6309 ESC \u53D6\u6D88...");
  
  EFI_INPUT_KEY Key;
  while (TRUE) {
    gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (Key.UnicodeChar == '\r' || Key.UnicodeChar == '\n') {
      Print (L"\n");
      return;
    }
    if (Key.ScanCode == SCAN_ESC) {
      Print (L"\n");
      PrintColor (COLOR_YELLOW, L"\u5B89\u88C5\u5DF2\u88AB\u7528\u6237\u53D6\u6D88\u3002\n");
      gBS->Exit (mInstallerCtx.ImageHandle, EFI_ABORTED, 0, NULL);
    }
  }
}

VOID
ShowInstallationProgress (VOID)
{
  EFI_STATUS Status;
  
  Print (L"\n");
  PrintTitle (L"\u6B63\u5728\u5B89\u88C5\u6591\u9E4C\u64CD\u4F5C\u7CFB\u7EDF");
  
  Print (L"\n");

  PrintColor (COLOR_YELLOW, L"[  0%] \u6B63\u5728\u51C6\u5907\u5B89\u88C5...\n");
  gBS->Stall (500000);

  PrintColor (COLOR_YELLOW, L"[ 10%] \u6B63\u5728\u521B\u5EFAGPT\u5206\u533A\u8868...\n");
  Status = CreateGptPartitionTable (mInstallerCtx.Disks[mInstallerCtx.SelectedDisk].Handle);
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"        \u9519\u8BEF: %r\n", Status);
    return;
  }
  gBS->Stall (500000);

  PrintColor (COLOR_YELLOW, L"[ 20%] \u6B63\u5728\u521B\u5EFA\u5206\u533A...\n");
  Status = AutoPartitionDisk ();
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"        \u9519\u8BEF: %r\n", Status);
    return;
  }
  gBS->Stall (500000);

  PrintColor (COLOR_YELLOW, L"[ 40%] \u6B63\u5728\u683C\u5F0F\u5316ESP\u4E3AFAT32...\n");
  Status = FormatPartition (&mInstallerCtx.Partitions[0]);
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"        \u9519\u8BEF: %r\n", Status);
    return;
  }
  gBS->Stall (500000);

  PrintColor (COLOR_YELLOW, L"[ 50%] \u6B63\u5728\u683C\u5F0F\u5316\u7CFB\u7EDF\u5206\u533A\u4E3A %s...\n", GetFileSystemName(mInstallerCtx.RootFS));
  Status = FormatPartition (&mInstallerCtx.Partitions[1]);
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"        \u9519\u8BEF: %r\n", Status);
    return;
  }
  gBS->Stall (500000);

  if (mInstallerCtx.PartitionCount > 2) {
    PrintColor (COLOR_YELLOW, L"[ 60%] \u6B63\u5728\u683C\u5F0F\u5316\u7528\u6237\u5206\u533A\u4E3A %s...\n", GetFileSystemName(mInstallerCtx.HomeFS));
    Status = FormatPartition (&mInstallerCtx.Partitions[2]);
    if (EFI_ERROR (Status)) {
      PrintColor (COLOR_RED, L"        \u9519\u8BEF: %r\n", Status);
      return;
    }
    gBS->Stall (500000);
  }

  PrintColor (COLOR_YELLOW, L"[ 70%] \u6B63\u5728\u590D\u5236\u7CFB\u7EDF\u6587\u4EF6...\n");
  Status = CopySystemFiles ();
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"        \u9519\u8BEF: %r\n", Status);
    return;
  }
  gBS->Stall (1000000);

  PrintColor (COLOR_YELLOW, L"[ 85%] \u6B63\u5728\u5B89\u88C5EFI\u5F15\u5BFCO...\n");
  Status = InstallEfiBootloader ();
  if (EFI_ERROR (Status)) {
    PrintColor (COLOR_RED, L"        \u9519\u8BEF: %r\n", Status);
    return;
  }
  gBS->Stall (500000);

  PrintColor (COLOR_YELLOW, L"[100%] \u6B63\u5728\u5B8C\u6210\u5B89\u88C5...\n");
  gBS->Stall (500000);
}

VOID
ShowCompleteScreen (VOID)
{
  Print (L"\n");
  PrintTitle (L"\u5B89\u88C5\u5B8C\u6210\uff01");
  
  Print (L"\n");
  PrintColor (COLOR_GREEN, L"\u6591\u9E4C\u64CD\u4F5C\u7CFB\u7EDF\u5B89\u88C5\u6210\u529F\uff01\n");
  Print (L"\n");
  PrintColor (COLOR_WHITE, L"\u7CFB\u7EDF\u914D\u7F6E:\n");
  PrintColor (COLOR_LIGHTBLUE, L"  \u4E3B\u673A\u540D:    %s\n", mInstallerCtx.Hostname);
  PrintColor (COLOR_LIGHTBLUE, L"  \u7528\u6237\u540D:    %s\n", mInstallerCtx.Username);
  PrintColor (COLOR_LIGHTBLUE, L"  \u7CFB\u7EDF\u5206\u533A:     %s\n", GetFileSystemName(mInstallerCtx.RootFS));
  PrintColor (COLOR_LIGHTBLUE, L"  \u7528\u6237\u5206\u533A:     %s\n", GetFileSystemName(mInstallerCtx.HomeFS));
  Print (L"\n");
  PrintColor (COLOR_LIGHTGRAY, L"\u8BF7\u91CD\u65B0\u542F\u52A8\u8BA1\u7B97\u673A\u4EE5\u542F\u52A8\n");
  PrintColor (COLOR_LIGHTGRAY, L"\u60A8\u7684\u65B0\u6591\u9E4C\u64CD\u4F5C\u7CFB\u7EDF\u3002\n");
  Print (L"\n");
  PrintColor (COLOR_LIGHTCYAN, L"\u6309\u4EFB\u610F\u952E\u9000\u51FA...");
  
  EFI_INPUT_KEY Key;
  gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

  mInstallerCtx.ImageHandle = ImageHandle;

  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
  if (!EFI_ERROR (Status)) {
    EFI_DEVICE_PATH_PROTOCOL *DevicePath = LoadedImage->FilePath;
    CHAR16 *DevicePathStr = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
    if (DevicePathStr) {
      UINTN PathLen = StrLen (DevicePathStr);
      for (UINTN i = PathLen; i > 0; i--) {
        if (DevicePathStr[i-1] == L'\\') {
          DevicePathStr[i-1] = L'\0';
          break;
        }
      }
      StrCpyS (mInstallerCtx.SourcePath, MAX_PATH, DevicePathStr);
      FreePool (DevicePathStr);
    }
  }

  Status = DetectDisks ();
  if (EFI_ERROR (Status) || mInstallerCtx.DiskCount == 0) {
    PrintColor (COLOR_RED, L"\u9519\u8BEF: \u672A\u627E\u5230\u78C1\u76D8\uff01\n");
    return EFI_NOT_FOUND;
  }

  ShowWelcomeScreen ();
  ShowDiskSelectionScreen ();
  ShowFileSystemSelection ();
  ShowUserSettingsScreen ();
  ShowInstallationSummary ();
  ShowInstallationProgress ();
  ShowCompleteScreen ();

  return EFI_SUCCESS;
}