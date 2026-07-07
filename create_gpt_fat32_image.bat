@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ==============================================
echo BanJiuOS GPT FAT32 Image Creator
echo ==============================================
echo.

set "IMAGE_PATH=d:\Projects\BanJiuOS\output\BanJiuOS-uefi.img"
set "ISO_DIR=d:\Projects\BanJiuOS\iso"
set "DISKPART_SCRIPT=%TEMP%\format_banjiuos.txt"

echo Step 1: Creating diskpart script...
echo select vdisk file="%IMAGE_PATH%" > "%DISKPART_SCRIPT%"
echo attach vdisk >> "%DISKPART_SCRIPT%"
echo convert gpt >> "%DISKPART_SCRIPT%"
echo create partition primary >> "%DISKPART_SCRIPT%"
echo format fs=fat32 label=BANJIUOS quick >> "%DISKPART_SCRIPT%"
echo assign letter=Z >> "%DISKPART_SCRIPT%"
echo list volume >> "%DISKPART_SCRIPT%"

echo Step 2: Running diskpart...
echo.
echo WARNING: This requires administrator privileges!
echo If diskpart fails, please run this script as Administrator.
echo.
pause

diskpart /s "%DISKPART_SCRIPT%"

echo.
echo Step 3: Copying files to Z:\...
echo.

mkdir "Z:\EFI\BOOT" 2>nul
mkdir "Z:\Installer" 2>nul
mkdir "Z:\System" 2>nul

copy "%ISO_DIR%\EFI\BOOT\BOOTX64.EFI" "Z:\EFI\BOOT\" /Y
copy "%ISO_DIR%\Installer\*" "Z:\Installer\" /Y
copy "%ISO_DIR%\System\*" "Z:\System\" /Y

echo.
echo Step 4: Detaching virtual disk...
echo.

echo select vdisk file="%IMAGE_PATH%" > "%DISKPART_SCRIPT%"
echo detach vdisk >> "%DISKPART_SCRIPT%"

diskpart /s "%DISKPART_SCRIPT%"

del "%DISKPART_SCRIPT%"

echo.
echo ==============================================
echo BanJiuOS GPT FAT32 Image Created Successfully!
echo ==============================================
echo Image: %IMAGE_PATH%
echo ==============================================
pause