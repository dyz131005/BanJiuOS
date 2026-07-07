@echo off
chcp 65001
echo Rebuilding ISO with proper EFI support...
echo.

set MKISOFSPATH=D:\software\VMware\VMware Workstation\mkisofs.exe
set ISO_DIR=d:\Projects\BanJiuOS\iso
set OUTPUT=d:\Projects\BanJiuOS\output\BanJiuOS-1.0.0-x86_64.iso

cd /d "%ISO_DIR%"

echo Creating ISO with EFI boot support...
"%MKISOFSPATH%" ^
    -R ^
    -J ^
    -f ^
    -pad ^
    -no-emul-boot ^
    -boot-load-size 4 ^
    -hide boot.catalog ^
    -hide-joliet boot.catalog ^
    -eltorito-platform 0xEF ^
    -eltorito-boot EFI/BOOT/BOOTX64.EFI ^
    -no-emul-boot ^
    -o "%OUTPUT%" ^
    .

echo.
if exist "%OUTPUT%" (
    echo ISO created: %OUTPUT%
    dir "%OUTPUT%"
) else (
    echo Failed to create ISO!
)
pause