# BanJiuOS 编译与运行指南

## 一、项目概述

BanJiuOS 是一个基于 UEFI 的操作系统项目，包含：
- **BootLoader**: UEFI 引导程序（两套：安装程序引导 + 系统引导）
- **Installer**: 中文图形化安装程序
- **Kernel**: BJOS格式内核（自定义可执行格式）

---

## 二、环境要求

### 2.1 必需软件
| 软件 | 版本 | 路径示例 |
|------|------|----------|
| EDK2 | 最新版 | `D:\program\EDK2\edk2` |
| Visual Studio | 2019/2022/2026 | `D:\program\Microsoft Visual Studio` |
| QEMU | 最新版 | `D:\program\qemu` |
| Python | 3.x | 系统PATH中 |
| MinGW-w64 | 最新版 | `D:\program\cpp\mingw64` |
| LLVM Clang | 最新版 | `D:\program\LLVM\bin` |

### 2.2 环境变量设置

```batch
set EDK_TOOLS_BIN=D:\program\EDK2\edk2\BaseTools\Bin\Win64
set PYTHON_COMMAND=python
set NASM_PREFIX=D:\program\cpp\mingw64\bin\
```

---

## 三、项目文件结构

```
d:\Projects\BanJiuOS\
├── bootloader\
│   ├── src\
│   │   ├── BootLoader.c          # 安装程序引导（旧版，只启动安装程序）
│   │   ├── Installer.c           # 中文安装程序主文件
│   │   ├── BjosBootLoader.c      # BJOS内核引导程序（新版，启动内核）
│   │   ├── ElfBootLoader.c       # ELF内核引导程序
│   │   ├── EfiElfLoader.c        # ELF加载器
│   │   └── DisablePaging.asm     # 禁用分页汇编代码
│   ├── BanJiuBootLoader.inf      # BootLoader模块定义
│   ├── BanJiuInstaller.inf       # Installer模块定义
│   ├── BjosBootLoader.inf        # BJOS引导程序模块定义
│   ├── ElfBootLoader.inf         # ELF引导程序模块定义
│   └── BanJiuOS.dsc              # 平台描述文件
├── kernel\
│   ├── src\
│   │   ├── core\                 # 核心模块
│   │   ├── mm\                   # 内存管理
│   │   ├── proc\                 # 进程管理
│   │   └── fs\                   # 文件系统
│   ├── include\                  # 头文件
│   ├── kernel.ld                 # 链接脚本
│   ├── build_kernel_bjos.bat     # BJOS内核编译脚本
│   └── build_kernel_elf.bat      # ELF内核编译脚本（LLVM Clang）
├── iso\
│   ├── EFI\BOOT\BOOTX64.EFI      # 安装程序引导程序
│   └── Installer\setup.efi       # 安装程序
├── output\
│   ├── OS\                       # 系统引导输出目录
│   │   ├── EFI\BOOT\BOOTX64.EFI  # 系统引导程序
│   │   ├── System\kernel.elf     # ELF内核
│   │   └── System\kernel.bjos    # BJOS内核
│   │   └── startup.nsh           # UEFI启动脚本
│   └── Installer\                # 安装程序引导输出目录
├── BUILD_GUIDE.md                # 本指南
└── run_qemu_dir.bat              # 目录模式启动脚本
```

---

## 四、编译步骤

### 4.1 编译内核（ELF格式）

```batch
cd /d d:\Projects\BanJiuOS\kernel
call build_kernel_elf.bat
```

**编译结果验证：**
- 输出文件: `kernel\output\kernel.elf`
- 入口点: `0x100000`
- 大小: ~77KB

### 4.2 编译ELF引导程序（EDK2）

```batch
cd /d d:\Projects\BanJiuOS\bootloader
call build_elf_bootloader_edk2.bat
```

**输出文件：**
- `output\OS\EFI\BOOT\BOOTX64.EFI` - ELF引导程序

---

## 五、编译产物验证

### 5.1 验证脚本

```powershell
Write-Host "=== 编译产物验证 ==="
Write-Host ""
Write-Host "[内核文件]"
if (Test-Path "d:\Projects\BanJiuOS\kernel\output\kernel.elf") {
    Write-Host "✅ kernel.elf 存在"
} else {
    Write-Host "❌ kernel.elf 不存在"
}
Write-Host ""
Write-Host "[系统引导]"
if (Test-Path "d:\Projects\BanJiuOS\output\OS\EFI\BOOT\BOOTX64.EFI") {
    Write-Host "✅ BOOTX64.EFI (系统引导) 存在"
} else {
    Write-Host "❌ BOOTX64.EFI (系统引导) 不存在"
}
```

### 5.2 验证结果（2026-06-24）

```
=== 编译产物验证 ===

[内核文件]
✅ kernel.elf 存在 (77,344 bytes)

[系统引导]
✅ BOOTX64.EFI (系统引导) 存在 (16,544 bytes)
```

### 5.3 文件大小对比

| 文件 | 路径 | 大小 | 说明 |
|------|------|------|------|
| kernel.elf | `kernel\output\kernel.elf` | 77,344 bytes | ELF格式内核（LLVM Clang） |
| BOOTX64.EFI (系统) | `output\OS\EFI\BOOT\BOOTX64.EFI` | 16,544 bytes | ELF引导程序（含禁用分页） |
| kernel.bjos | `kernel\output\kernel.bjos` | 68,178 bytes | BJOS格式内核（保留） |

---

## 六、启动系统

### 6.1 运行系统（使用脚本）

```batch
cd /d d:\Projects\BanJiuOS
run_qemu_dir.bat
```

---

## 七、开发日志

### 7.1 2026-06-23 开发记录

**已完成工作：**

1. ✅ **ELF格式迁移**
   - 创建ELF内核编译脚本（使用LLVM Clang）
   - 创建ELF加载器（EfiElfLoader.c/h）
   - 创建ELF引导程序（ElfBootLoader.c）
   - 创建禁用分页汇编代码（DisablePaging.asm）

2. ✅ **内核编译成功**
   - 使用LLVM Clang编译，生成真正的ELF格式
   - 魔数验证：`7F 45 4C 46` ✅

3. ✅ **引导程序编译成功**
   - EDK2 + Visual Studio编译
   - 包含禁用分页功能

### 7.2 2026-06-24 开发记录

**当前状态：**

| 组件 | 状态 | 说明 |
|------|------|------|
| ELF内核编译 | ✅ 成功 | LLVM Clang，77,344 bytes |
| ELF引导程序编译 | ✅ 成功 | EDK2，16,544 bytes |
| ELF加载验证 | ✅ 成功 | 魔数、Program Header验证通过 |
| ExitBootServices | ✅ 成功 | 内存映射缓冲区已修复 |
| 禁用分页 | ❌ 失败 | CR0.PG仍为1，分页未禁用 |
| 内核执行 | ❌ 失败 | #UD异常，RIP跳转到错误地址 |

**问题描述：**

1. **分页未禁用**
   - CR0 = 0x80010033，PG位为1
   - DisablePaging()函数调用后分页仍然启用
   - 可能原因：ExitBootServices后代码无法正确执行

2. **跳转地址错误**
   - 期望入口点：0x18D15000
   - 实际RIP：0xB0000
   - 出现#UD (Invalid Opcode)异常

---

## 八、当前问题与困境（2026-06-24）

### 8.1 问题概述

| 问题编号 | 问题描述 | 严重程度 | 状态 |
|----------|----------|----------|------|
| P001 | ExitBootServices成功，但DisablePaging()无效 | 🔴 严重 | 未解决 |
| P002 | CR0.PG位仍为1，分页未禁用 | 🔴 严重 | 未解决 |
| P003 | RIP跳转到错误地址0xB0000 | 🔴 严重 | 未解决 |
| P004 | #UD (Invalid Opcode)异常 | 🔴 严重 | 未解决 |

### 8.2 详细问题分析

#### P001: DisablePaging()无效

**现象：**
- ExitBootServices成功执行
- DisablePaging()函数被调用
- 但CR0.PG位仍然为1

**可能原因：**
1. ExitBootServices后，代码可能不在正确的内存位置
2. UEFI的页表映射导致代码无法访问
3. 需要在禁用分页前建立新的页表映射

**解决方案：**
- 需要在跳转到内核前确保所有代码在物理内存中可访问
- 或者使用不同的方法来切换到内核模式

---

## 九、版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-06-14 | 初始版本，支持中文安装程序 |
| 1.1 | 2026-06-15 | 新增BJOS内核、系统引导、启动脚本 |
| 1.2 | 2026-06-23 | 开始ELF格式迁移 |
| 1.3 | 2026-06-24 | ELF格式迁移完成，遇到分页问题 |

---

*文档创建日期: 2026-06-14*
*最后更新: 2026-06-24*
*BanJiuOS Project*
*状态: 开发中 - ELF格式迁移遇到分页问题*