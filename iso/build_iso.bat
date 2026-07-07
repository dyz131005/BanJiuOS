@echo off
chcp 65001 >nul 2>&1

echo BanJiuOS ISO Build Script
echo =========================
echo.

set "ISO_DIR=d:\Projects\BanJiuOS\iso"
set "OUTPUT_DIR=d:\Projects\BanJiuOS\output"
set "ISO_NAME=BanJiuOS-1.0.0-x86_64.iso"

echo Verifying files...
if not exist "%ISO_DIR%\EFI\BOOT\BOOTX64.EFI" (
    echo Error: BOOTX64.EFI not found!
    exit /b 1
)

if not exist "%ISO_DIR%\Installer\setup.efi" (
    echo Error: setup.efi not found!
    exit /b 1
)

echo.
echo Creating directories...
if not exist "%ISO_DIR%\EFI\BOOT\Fonts" mkdir "%ISO_DIR%\EFI\BOOT\Fonts"
if not exist "%ISO_DIR%\Boot\themes" mkdir "%ISO_DIR%\Boot\themes"
if not exist "%ISO_DIR%\System" mkdir "%ISO_DIR%\System"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo.
echo Creating kernel placeholder if not exists...
if not exist "%ISO_DIR%\System\kernel.zyxt" (
    echo BanJiuOS Kernel Placeholder > "%ISO_DIR%\System\kernel.zyxt"
)

if not exist "%ISO_DIR%\System\initramfs.img" (
    echo BanJiuOS InitRAMFS Placeholder > "%ISO_DIR%\System\initramfs.img"
)

echo.
echo Building ISO image with mkisofs...
cd /d "%ISO_DIR%"
"D:\software\VMware\VMware Workstation\mkisofs.exe" -R -f -pad -no-emul-boot -boot-load-size 4 -eltorito-alt-boot -efi-boot EFI/BOOT/BOOTX64.EFI -no-emul-boot -o "%OUTPUT_DIR%\%ISO_NAME%" .

echo.
if exist "%OUTPUT_DIR%\%ISO_NAME%" (
    echo Success! ISO created: %OUTPUT_DIR%\%ISO_NAME%
    dir "%OUTPUT_DIR%\%ISO_NAME%"
) else (
    echo Failed to create ISO!
    exit /b 1
)

echo.
pause