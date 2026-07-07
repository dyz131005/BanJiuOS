@echo off
chcp 65001
echo Creating BIOS+UEFI hybrid ISO...
echo.

set MKISOFSPATH=D:\软件\VMware\VMware Workstation\mkisofs.exe
set ISO_DIR=d:\Projects\BanJiuOS\iso
set OUTPUT=d:\Projects\BanJiuOS\output\BanJiuOS-1.0.0-x86_64.iso

cd /d "%ISO_DIR%"

echo Building hybrid ISO...
"%MKISOFSPATH%" ^
    -R ^
    -J ^
    -f ^
    -pad ^
    -no-emul-boot ^
    -boot-load-size 4 ^
    -boot-info-table ^
    -eltorito-boot EFI/BOOT/BOOTX64.EFI ^
    -eltorito-alt-boot ^
    -no-emul-boot ^
    -eltorito-platform 0xEF ^
    -o "%OUTPUT%" ^
    .

echo.
if exist "%OUTPUT%" (
    echo Success! ISO created: %OUTPUT%
    dir "%OUTPUT%"
) else (
    echo Failed!
)
pause