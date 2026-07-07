@echo off
chcp 65001
echo ==============================================
echo       BanJiuOS QEMU UEFI Boot (GUI Mode)
echo ==============================================
echo QEMU: D:\program\qemu\qemu-system-x86_64.exe
echo UEFI: D:\program\qemu\share\edk2-x86_64-code.fd
echo VHD: d:\Projects\BanJiuOS\output\BanJiuOS-uefi-OS.vhd
echo Memory: 512MB
echo Display: VGA (GUI)
echo ==============================================
echo.

echo Mounting VHD image...
powershell -Command "$image = Mount-DiskImage -ImagePath 'd:\Projects\BanJiuOS\output\BanJiuOS-uefi-OS.vhd' -StorageType VHD -PassThru; $disk = $image | Get-Disk; $partitions = $disk | Get-Partition; foreach($part in $partitions) { if($part.DriveLetter) { Write-Output $part.DriveLetter } }" > "%temp%\drive_letter.txt"
set /p DRIVE_LETTER=<"%temp%\drive_letter.txt"
if "%DRIVE_LETTER%"=="" (
    echo Error: Failed to get drive letter
    powershell -Command "Dismount-DiskImage -ImagePath 'd:\Projects\BanJiuOS\output\BanJiuOS-uefi-OS.vhd'"
    pause
    exit /b 1
)
echo VHD mounted at %DRIVE_LETTER%:

echo Creating directories...
mkdir "%DRIVE_LETTER%:\EFI\BOOT" 2>nul
mkdir "%DRIVE_LETTER%:\System" 2>nul

echo Copying files...
copy "D:\program\EDK2\edk2\Build\BanJiuOS\DEBUG_VS2019\X64\ElfBootLoader.efi" "%DRIVE_LETTER%:\EFI\BOOT\BOOTX64.EFI" /Y
copy "d:\Projects\BanJiuOS\kernel\output\kernel.elf" "%DRIVE_LETTER%:\System\kernel.elf" /Y

echo Dismounting VHD...
powershell -Command "Dismount-DiskImage -ImagePath 'd:\Projects\BanJiuOS\output\BanJiuOS-uefi-OS.vhd'"

echo.
echo Starting QEMU with VGA display...
echo Press Ctrl+Alt+G to release mouse from QEMU window
echo.

D:\program\qemu\qemu-system-x86_64.exe ^
    -drive if=pflash,format=raw,file=D:\program\qemu\share\edk2-x86_64-code.fd,readonly=on ^
    -drive if=pflash,format=raw,file=D:\program\qemu\share\edk2-x86_64-vars.fd ^
    -drive file="d:\Projects\BanJiuOS\output\BanJiuOS-uefi-OS.vhd",format=vpc,if=ide,index=0 ^
    -m 512 ^
    -net none ^
    -vga std ^
    -k en-us ^
    -serial file:d:\Projects\BanJiuOS\output\kernel_log.txt ^
    -D d:\Projects\BanJiuOS\output\qemu_debug.log ^
    -monitor stdio
