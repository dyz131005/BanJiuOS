@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ==============================================
echo        BanJiuOS VHD Update Script
echo ==============================================
echo.

set "VHD_PATH=d:\Projects\BanJiuOS\output\BanJiuOS-uefi-OS.vhd"
set "DISKPART_SCRIPT=%TEMP%\banjiuos_vhd_script.txt"
set "BOOTLOADER_SRC=D:\program\EDK2\edk2\Build\BanJiuOS\DEBUG_VS2019\X64\ElfBootLoader.efi"
set "KERNEL_SRC=d:\Projects\BanJiuOS\kernel\output\kernel.elf"
set "VDISK_LETTER="

echo [Step 1] Mounting VHD...

echo select vdisk file="%VHD_PATH%" > "%DISKPART_SCRIPT%"
echo attach vdisk >> "%DISKPART_SCRIPT%"

diskpart /s "%DISKPART_SCRIPT%" >nul

for /f "skip=8 tokens=1-7" %%a in ('diskpart /s "%DISKPART_SCRIPT%" ^| findstr /i "BANJIUOS"') do (
    set "VDISK_LETTER=%%b"
)

if not defined VDISK_LETTER (
    for /f "tokens=1-7" %%a in ('wmic logicaldisk where "VolumeName='BANJIUOS'" get DeviceID^,VolumeName /value 2^>nul') do (
        if "%%a"=="DeviceID=" set "VDISK_LETTER=%%b"
    )
)

if not defined VDISK_LETTER (
    echo ERROR: Could not find VHD drive letter!
    del "%DISKPART_SCRIPT%"
    pause
    exit /b 1
)

echo VHD mounted as %VDISK_LETTER%:
echo.

echo [Step 2] Copying files to VHD...

mkdir "%VDISK_LETTER%\EFI\BOOT" 2>nul
mkdir "%VDISK_LETTER%\System" 2>nul

if exist "%BOOTLOADER_SRC%" (
    copy "%BOOTLOADER_SRC%" "%VDISK_LETTER%\EFI\BOOT\BOOTX64.EFI" /Y
    echo Copied: BOOTX64.EFI
) else (
    echo WARNING: Bootloader not found at %BOOTLOADER_SRC%
)

if exist "%KERNEL_SRC%" (
    copy "%KERNEL_SRC%" "%VDISK_LETTER%\System\kernel.elf" /Y
    echo Copied: kernel.elf
) else (
    echo WARNING: Kernel not found at %KERNEL_SRC%
)

echo.

echo [Step 3] Removing Recycle Bin folders...

if exist "%VDISK_LETTER%\$RECYCLE.BIN" (
    rmdir /s /q "%VDISK_LETTER%\$RECYCLE.BIN"
    echo Removed: $RECYCLE.BIN
)

if exist "%VDISK_LETTER%\System\$RECYCLE.BIN" (
    rmdir /s /q "%VDISK_LETTER%\System\$RECYCLE.BIN"
    echo Removed: System\$RECYCLE.BIN
)

if exist "%VDISK_LETTER%\EFI\$RECYCLE.BIN" (
    rmdir /s /q "%VDISK_LETTER%\EFI\$RECYCLE.BIN"
    echo Removed: EFI\$RECYCLE.BIN
)

echo.

echo [Step 4] Detaching VHD...

echo select vdisk file="%VHD_PATH%" > "%DISKPART_SCRIPT%"
echo detach vdisk >> "%DISKPART_SCRIPT%"

diskpart /s "%DISKPART_SCRIPT%" >nul

del "%DISKPART_SCRIPT%"

echo VHD detached successfully!
echo.

echo [Step 5] Starting QEMU...
echo ==============================================

call d:\Projects\BanJiuOS\run_qemu_system.bat

endlocal
