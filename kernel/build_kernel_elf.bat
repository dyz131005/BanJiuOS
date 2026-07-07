@echo off
setlocal enabledelayedexpansion

set "KERNEL_DIR=%~dp0"
set "OUTPUT_DIR=%KERNEL_DIR%output"
set "CLANG_PATH=D:\program\LLVM\bin"
set "GCC_PATH=D:\program\cpp\mingw64\bin"

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

set "CFLAGS=-target x86_64-unknown-linux-gnu -ffreestanding -fno-pic -fno-pie -m64 -mno-red-zone -fno-stack-protector -mcmodel=large -fno-exceptions -Wall -Wextra -g"
set "LDFLAGS=-target x86_64-unknown-linux-gnu -T %KERNEL_DIR%kernel.ld -nostdlib -static -Wl,--no-dynamic-linker"

echo ============================================
echo Building BanJiuOS Kernel with ELF Format (LLVM Clang)
echo ============================================
echo.

echo [Step 1] Cleaning old output files...
if exist "%OUTPUT_DIR%\kernel.elf" del /q "%OUTPUT_DIR%\kernel.elf"
if exist "%OUTPUT_DIR%\*.o" del /q "%OUTPUT_DIR%\*.o"
if exist "%OUTPUT_DIR%\*.elf.o" del /q "%OUTPUT_DIR%\*.elf.o"


echo [Step 2] Compiling kernel sources with LLVM Clang...

"%CLANG_PATH%\clang.exe" %CFLAGS% -I "%KERNEL_DIR%include" -c "%KERNEL_DIR%src\core\main.c" -o "%OUTPUT_DIR%\main.o"
if %errorlevel% neq 0 goto :error

"%CLANG_PATH%\clang.exe" %CFLAGS% -I "%KERNEL_DIR%include" -c "%KERNEL_DIR%src\core\string.c" -o "%OUTPUT_DIR%\string.o"
if %errorlevel% neq 0 goto :error

"%CLANG_PATH%\clang.exe" %CFLAGS% -I "%KERNEL_DIR%include" -c "%KERNEL_DIR%src\core\font.c" -o "%OUTPUT_DIR%\font.o"
if %errorlevel% neq 0 goto :error

"%CLANG_PATH%\clang.exe" %CFLAGS% -I "%KERNEL_DIR%include" -c "%KERNEL_DIR%src\core\framebuffer.c" -o "%OUTPUT_DIR%\framebuffer.o"
if %errorlevel% neq 0 goto :error

"%CLANG_PATH%\clang.exe" %CFLAGS% -I "%KERNEL_DIR%include" -c "%KERNEL_DIR%src\core\shell.c" -o "%OUTPUT_DIR%\shell.o"
if %errorlevel% neq 0 goto :error

"%CLANG_PATH%\clang.exe" %CFLAGS% -I "%KERNEL_DIR%include" -c "%KERNEL_DIR%src\mm\pmm.c" -o "%OUTPUT_DIR%\pmm.o"
if %errorlevel% neq 0 goto :error

"%CLANG_PATH%\clang.exe" %CFLAGS% -I "%KERNEL_DIR%include" -c "%KERNEL_DIR%src\mm\vmm.c" -o "%OUTPUT_DIR%\vmm.o"
if %errorlevel% neq 0 goto :error

"%CLANG_PATH%\clang.exe" %CFLAGS% -I "%KERNEL_DIR%include" -c "%KERNEL_DIR%src\mm\kmalloc.c" -o "%OUTPUT_DIR%\kmalloc.o"
if %errorlevel% neq 0 goto :error

"%CLANG_PATH%\clang.exe" %CFLAGS% -I "%KERNEL_DIR%include" -c "%KERNEL_DIR%src\proc\process.c" -o "%OUTPUT_DIR%\process.o"
if %errorlevel% neq 0 goto :error

"%CLANG_PATH%\clang.exe" %CFLAGS% -I "%KERNEL_DIR%include" -c "%KERNEL_DIR%src\fs\fs.c" -o "%OUTPUT_DIR%\fs.o"
if %errorlevel% neq 0 goto :error

"%CLANG_PATH%\clang.exe" %CFLAGS% -I "%KERNEL_DIR%include" -c "%KERNEL_DIR%src\fs\ext4.c" -o "%OUTPUT_DIR%\ext4.o"
if %errorlevel% neq 0 goto :error

"%CLANG_PATH%\clang.exe" %CFLAGS% -I "%KERNEL_DIR%include" -c "%KERNEL_DIR%src\fs\fat32.c" -o "%OUTPUT_DIR%\fat32.o"
if %errorlevel% neq 0 goto :error

echo [Step 3] Linking ELF kernel...

"%CLANG_PATH%\clang.exe" %LDFLAGS% -o "%OUTPUT_DIR%\kernel.elf" ^
    "%OUTPUT_DIR%\main.o" ^
    "%OUTPUT_DIR%\string.o" ^
    "%OUTPUT_DIR%\font.o" ^
    "%OUTPUT_DIR%\framebuffer.o" ^
    "%OUTPUT_DIR%\shell.o" ^
    "%OUTPUT_DIR%\pmm.o" ^
    "%OUTPUT_DIR%\vmm.o" ^
    "%OUTPUT_DIR%\kmalloc.o" ^
    "%OUTPUT_DIR%\process.o" ^
    "%OUTPUT_DIR%\fs.o" ^
    "%OUTPUT_DIR%\ext4.o" ^
    "%OUTPUT_DIR%\fat32.o"

if %errorlevel% neq 0 goto :error

echo [Step 4] Verifying ELF kernel...

if not exist "%OUTPUT_DIR%\kernel.elf" (
    echo ERROR: kernel.elf was not created
    goto :error
)

for %%A in ("%OUTPUT_DIR%\kernel.elf") do set "SIZE=%%~zA"
echo ELF kernel size: %SIZE% bytes

echo.
echo ============================================
echo BanJiuOS ELF Kernel built successfully!
echo Output: %OUTPUT_DIR%\kernel.elf
echo ============================================

goto :end

:error
echo.
echo Build failed with error %errorlevel%
exit /b %errorlevel%

:end
endlocal
