@echo off
chcp 65001
echo ==============================================
echo          BanJiuOS QEMU UEFI Boot
echo ==============================================
echo QEMU: D:\program\qemu\qemu-system-x86_64.exe
echo UEFI: D:\program\qemu\share\edk2-x86_64-code.fd
echo VHD: d:\Projects\BanJiuOS\output\BanJiuOS-uefi-OS.vhd
echo Memory: 512MB
echo ==============================================
echo.

D:\program\qemu\qemu-system-x86_64.exe ^
    -drive if=pflash,format=raw,file=D:\program\qemu\share\edk2-x86_64-code.fd,readonly=on ^
    -drive if=pflash,format=raw,file=D:\program\qemu\share\edk2-x86_64-vars.fd ^
    -hda d:\Projects\BanJiuOS\output\BanJiuOS-uefi-OS.vhd ^
    -m 512 ^
    -net none ^
    -nographic