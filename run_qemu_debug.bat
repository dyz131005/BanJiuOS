@echo off
chcp 65001
echo Starting BanJiuOS in QEMU with DEBUG...
echo.
echo Command:
echo D:\program\qemu\qemu-system-x86_64.exe -L D:\program\qemu -cdrom d:\Projects\BanJiuOS\output\BanJiuOS-1.0.0-x86_64.iso -boot d -m 512 -net none -display sdl -debugcon stdio -gdb tcp::1234
echo.
D:\program\qemu\qemu-system-x86_64.exe -L D:\program\qemu -cdrom d:\Projects\BanJiuOS\output\BanJiuOS-1.0.0-x86_64.iso -boot d -m 512 -net none -display sdl -debugcon stdio -gdb tcp::1234
pause