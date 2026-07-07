@echo off
chcp 65001
echo ==============================================
echo    BanJiuOS GDB Kernel Debugging Setup
echo ==============================================
echo.
echo This script will:
echo   1. Start QEMU waiting for GDB connection
echo   2. Open GDB to connect and debug
echo.
echo Using existing bootloader (RELEASE build)
echo Kernel has DEBUG symbols (-g flag)
echo.
echo ==============================================
echo.

echo [1/2] Starting QEMU in new window...
start "BanJiuOS QEMU GDB" cmd /k "d:\Projects\BanJiuOS\run_qemu_gdb_debug.bat"

echo.
echo [2/2] Waiting for QEMU to initialize (5 seconds)...
timeout /t 5 /nobreak >nul

echo.
echo ==============================================
echo    GDB Commands Reference
echo ==============================================
echo.
echo Basic commands:
echo   c    - Continue execution
echo   n    - Next instruction (step over)
echo   s    - Step into function
echo   bt   - Show stack backtrace
echo   info registers - Show register values
echo.
echo Custom commands:
echo   kernel-info - Show kernel status
echo.
echo Breakpoints:
echo   break _start   - Break at kernel entry
echo   break kmain    - Break at kmain function
echo   break font_draw_char - Break at font drawing
echo.
echo Memory examination:
echo   x/16x 0x100000 - Examine memory at address
echo   x/4wx 0xFFFFFF8000000000 - Examine kernel memory
echo.
echo ==============================================
echo.
echo Starting GDB...
echo.

d:\program\cpp\mingw64\bin\gdb.exe -x d:/Projects/BanJiuOS/debug_kernel.gdb