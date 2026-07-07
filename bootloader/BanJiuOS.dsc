[Defines]
  PLATFORM_NAME                  = BanJiuOS
  PLATFORM_GUID                  = 87654321-4321-4321-4321-CBA987654321
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  SUPPORTED_ARCHITECTURES        = X64
  BUILD_TARGETS                  = DEBUG|RELEASE
  TOOL_CHAIN_TAG                 = VS2019

[LibraryClasses]
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  StackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf

[Components]
  BanJiuOS/ElfBootLoader.inf

[PcdsFixedAtBuild]
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000000