# BanJiuOS

一个基于 x86-64 的简单操作系统，使用 UEFI 引导，支持 FAT32 文件系统。

## 项目结构

```
BanJiuOS/
├── bootloader/      # UEFI 引导加载器
├── kernel/          # 内核源代码
│   ├── src/
│   │   ├── core/    # 核心功能（shell、字体、帧缓冲等）
│   │   ├── fs/      # 文件系统（FAT32、ext4 框架）
│   │   ├── mm/      # 内存管理
│   │   └── proc/    # 进程管理
│   └── include/     # 头文件
├── iso/             # ISO 镜像构建相关
└── output/          # 构建输出
```

## 功能特性

- UEFI 引导支持
- ELF 内核加载
- 帧缓冲图形输出
- FAT32 文件系统
- 虚拟文件系统 (VFS) 抽象层
- Shell 命令行界面
- 内存管理（物理页分配、虚拟内存）

## Shell 命令

| 命令 | 说明 |
|------|------|
| `cd <dir>` | 切换目录 |
| `ls [dir]` | 列出目录内容 |
| `dir [dir]` | 同 ls |
| `mkdir <dir>` | 创建目录 |
| `rm <file>` | 删除文件 |
| `rmdir <dir>` | 删除目录 |
| `touch <file>` | 创建空文件 |
| `echo <text>` | 输出文本 |
| `sysinfo` | 显示系统信息 |
| `mount <dev> <mnt>` | 挂载文件系统 |
| `log` | 显示键盘调试日志 |
| `help` | 显示帮助信息 |

## 构建

### 内核

```bash
cd kernel
.\build_kernel_elf.bat
```

### 引导加载器

```bash
cd bootloader
.\build_elf_bootloader_edk2.bat
```

### 运行

```bash
.\run_qemu_gui.bat
```

## 技术栈

- **语言**: C / x86-64 汇编
- **编译器**: LLVM Clang
- **引导**: UEFI
- **文件系统**: FAT32
- **仿真器**: QEMU

## 许可证

MIT License
