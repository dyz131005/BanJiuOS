@echo off
chcp 65001
echo ==============================================
echo          BanJiuOS QEMU UEFI Boot (Directory Mode)
echo ==============================================
echo QEMU: D:\program\qemu\qemu-system-x86_64.exe
echo UEFI: D:\program\qemu\share\edk2-x86_64-code.fd
echo Directory: d:\Projects\BanJiuOS\output\OS
echo Memory: 512MB
echo ==============================================
echo.

mkdir "d:\Projects\BanJiuOS\output\OS\EFI\BOOT" 2>nul
mkdir "d:\Projects\BanJiuOS\output\OS\System" 2>nul

copy "D:\program\EDK2\edk2\Build\BanJiuOS\DEBUG_VS2019\X64\ElfBootLoader.efi" "d:\Projects\BanJiuOS\output\OS\EFI\BOOT\BOOTX64.EFI" /Y
copy "d:\Projects\BanJiuOS\kernel\output\kernel.elf" "d:\Projects\BanJiuOS\output\OS\System\kernel.elf" /Y

echo Files copied successfully!
echo.

D:\program\qemu\qemu-system-x86_64.exe ^
    -drive if=pflash,format=raw,file=D:\program\qemu\share\edk2-x86_64-code.fd,readonly=on ^
    -drive if=pflash,format=raw,file=D:\program\qemu\share\edk2-x86_64-vars.fd ^
    -hda fat:rw:d:\Projects\BanJiuOS\output\OS ^
    -m 512 ^
    -net none ^
    -nographic