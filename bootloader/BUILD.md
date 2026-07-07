# BanJiuOS UEFI Boot Loader 编译指南

## 概述

本引导程序是真正的UEFI应用程序，使用UEFI API，可在VMware和真实硬件上运行。

## 文件结构

```
bootloader/
├── src/
│   ├── BootLoader.c           # EDK II 版本
│   └── BootLoaderGnuEfi.c     # GNU-EFI 版本
├── BanJiuBootLoader.inf       # EDK II 模块信息文件
├── BanJiuOS.dsc               # EDK II 平台描述文件
├── Makefile.gnu-efi           # GNU-EFI 编译脚本
├── build_edk2.sh              # EDK II 编译脚本
└── BUILD.md                   # 本文档
```

## 编译方法

### 方法一：使用 GNU-EFI（推荐，简单）

**环境要求：**
- Linux 系统（Ubuntu/Debian/Fedora/Arch）
- GCC 编译器
- GNU-EFI 库

**安装 GNU-EFI：**

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install gnu-efi

# Fedora
sudo dnf install gnu-efi

# Arch Linux
sudo pacman -S gnu-efi

# 或从源码编译
git clone https://sourceforge.net/projects/gnu-efi/
cd gnu-efi
make
sudo make install
```

**编译：**

```bash
cd bootloader

# 使用默认路径
make -f Makefile.gnu-efi

# 或指定 GNU-EFI 路径
make -f Makefile.gnu-efi GNUEFI_LIB=/usr/lib/gnu-efi

# 安装到 iso 目录
make -f Makefile.gnu-efi install
```

**输出：**
- `build/EFI/BOOT/BOOTX64.EFI`

### 方法二：使用 EDK II（官方，完整）

**环境要求：**
- Linux 或 Windows 系统
- EDK II 源码

**获取 EDK II：**

```bash
git clone https://github.com/tianocore/edk2.git
cd edk2

# 初始化环境
. edksetup.sh

# 编译 BaseTools
make -C BaseTools
```

**编译 BanJiuOS Boot Loader：**

```bash
# 设置 EDK2 路径
export EDK2_PATH=/path/to/edk2

# 运行编译脚本
cd bootloader
bash build_edk2.sh
```

**或手动编译：**

```bash
cd edk2
. edksetup.sh

# 复制 BanJiuOS 模块到 EDK II
mkdir -p BanJiuOS
cp /path/to/bootloader/BanJiuBootLoader.inf BanJiuOS/
cp /path/to/bootloader/BanJiuOS.dsc BanJiuOS/
cp /path/to/bootloader/src/BootLoader.c BanJiuOS/

# 编译
build -p BanJiuOS/BanJiuOS.dsc -a X64 -t RELEASE
```

**输出：**
- `Build/BanJiuOS/RELEASE_X64/BanJiuBootLoader.efi`

### 方法三：使用 Windows EDK II

**环境要求：**
- Windows 系统
- Visual Studio 2019 或更高版本
- EDK II Windows 版本

**步骤：**

1. 下载 EDK II：https://github.com/tianocore/edk2

2. 设置环境：
```powershell
cd edk2
edksetup.bat
```

3. 编译 BaseTools：
```powershell
cd BaseTools
nmake
```

4. 复制 BanJiuOS 模块到 edk2 目录

5. 编译：
```powershell
build -p BanJiuOS\BanJiuOS.dsc -a X64 -t RELEASE
```

## UEFI API 使用说明

本引导程序使用以下 UEFI API：

### 启动服务 (EFI_BOOT_SERVICES)

| API | 功能 |
|-----|------|
| `HandleProtocol` | 获取协议接口 |
| `LocateHandleBuffer` | 查找支持协议的句柄 |
| `OpenProtocol` | 打开协议 |
| `AllocatePool` | 分配内存 |
| `FreePool` | 释放内存 |
| `GetMemoryMap` | 获取内存映射 |
| `ExitBootServices` | 退出启动服务 |
| `Stall` | 延时 |

### 运行服务 (EFI_RUNTIME_SERVICES)

| API | 功能 |
|-----|------|
| `GetTime` | 获取时间 |
| `SetTime` | 设置时间 |
| `ResetSystem` | 重启/关机 |

### 文件协议 (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL)

| API | 功能 |
|-----|------|
| `OpenVolume` | 打开卷 |
| `Open` | 打开文件 |
| `Read` | 读取文件 |
| `GetInfo` | 获取文件信息 |
| `Close` | 关闭文件 |

### 图形协议 (EFI_GRAPHICS_OUTPUT_PROTOCOL)

| API | 功能 |
|-----|------|
| `Blt` | 位块传输（绘图） |

## 在 VMware 中测试

1. 将编译好的 `BOOTX64.EFI` 复制到 ISO 的 `EFI/BOOT/` 目录

2. 创建 ISO 镜像：
```bash
xorriso -as mkisofs -R -f -pad -no-emul-boot \
        -eltorito-alt-boot -e EFI/BOOT/BOOTX64.EFI -no-emul-boot \
        -o BanJiuOS.iso iso/
```

3. 在 VMware 中挂载 ISO 并启动

4. 使用 GDB 调试（需要配置 .vmx 文件）：
```bash
gdb
(gdb) target remote localhost:1234
```

## 注意事项

1. **不使用 Windows API**：所有代码使用纯 UEFI API

2. **内存管理**：使用 `AllocatePool` 和 `FreePool`

3. **文件路径**：UEFI 使用 `\` 作为路径分隔符（不是 `/`）

4. **字符编码**：UEFI 使用 UTF-16 (CHAR16)

5. **退出启动服务**：必须在跳转到内核前调用 `ExitBootServices`

6. **物理地址**：内核加载地址必须是物理地址

## 常见问题

**Q: 编译时找不到 gnu-efi 库？**
A: 安装 gnu-efi 或设置 `GNUEFI_LIB` 环境变量

**Q: VMware 启动后黑屏？**
A: 检查 `BOOTX64.EFI` 是否正确放置在 `EFI/BOOT/` 目录

**Q: 如何调试？**
A: 在 .vmx 文件中添加 `debugStub.listen.guest64 = "TRUE"`

**Q: 内核加载失败？**
A: 确保 `kernel.zyxt` 文件存在于 ISO 的 `\System\` 目录