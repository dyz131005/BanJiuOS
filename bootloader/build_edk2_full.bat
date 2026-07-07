@echo off
REM BanJiuOS Boot Loader Build Script (EDK II - Windows)
REM Separates System Bootloader and Installer into different directories

setlocal

echo BanJiuOS Boot Loader Build Script (EDK II - Windows)
echo ======================================================
echo.
echo Note: System bootloader and installer will be placed in separate directories
echo.

set PYTHON_COMMAND=python
set NASM_PREFIX=D:\program\cpp\mingw64\bin\
set VS2019_PREFIX=D:\program\Microsoft Visual Studio\VC\Tools\MSVC\14.50.35717\
set VS_HOST=x64

call "D:\program\Microsoft Visual Studio\Common7\Tools\VsDevCmd.bat" -arch=x64

set EDK2_PATH=D:\program\EDK2\edk2
set BANJUOS_PATH=d:\Projects\BanJiuOS\bootloader
set BUILD_TARGET=RELEASE
set BUILD_ARCH=X64

set OUTPUT_OS_PATH=d:\Projects\BanJiuOS\output\OS
set OUTPUT_INSTALLER_PATH=d:\Projects\BanJiuOS\output\Installer

echo EDK2 Path:     %EDK2_PATH%
echo BanJiuOS Path: %BANJUOS_PATH%
echo Build Target:  %BUILD_TARGET%
echo Build Arch:    %BUILD_ARCH%
echo System Output: %OUTPUT_OS_PATH%
echo Installer Output: %OUTPUT_INSTALLER_PATH%

cd /d "%EDK2_PATH%"

echo.
echo Setting up EDK II environment...
call edksetup.bat

set EDK_TOOLS_BIN=%EDK_TOOLS_PATH%\Bin\Win64
set PATH=%EDK_TOOLS_BIN%;%PATH%

echo.
echo Copying BanJiuOS module to EDK II workspace...
if not exist "BanJiuOS" mkdir "BanJiuOS"
copy "%BANJUOS_PATH%\BanJiuBootLoader.inf" "BanJiuOS\" /Y
copy "%BANJUOS_PATH%\BanJiuInstaller.inf" "BanJiuOS\" /Y
copy "%BANJUOS_PATH%\BanJiuOS.dsc" "BanJiuOS\" /Y
copy "%BANJUOS_PATH%\src\BootLoader.c" "BanJiuOS\" /Y
copy "%BANJUOS_PATH%\src\Installer.c" "BanJiuOS\" /Y

echo.
echo Building BanJiuOS Boot Loader with VS2019...
build -p BanJiuOS\BanJiuOS.dsc -a %BUILD_ARCH% -t VS2019 -b %BUILD_TARGET%

echo.
echo Copying output files...

echo [System Bootloader]
if not exist "%OUTPUT_OS_PATH%\EFI\BOOT" mkdir "%OUTPUT_OS_PATH%\EFI\BOOT"

if exist "Build\BanJiuOS\%BUILD_TARGET%_VS2019\X64\BanJiuBootLoader\OUTPUT\BanJiuBootLoader.efi" (
    copy "Build\BanJiuOS\%BUILD_TARGET%_VS2019\X64\BanJiuBootLoader\OUTPUT\BanJiuBootLoader.efi" "%OUTPUT_OS_PATH%\EFI\BOOT\BOOTX64.EFI" /Y
    echo Success! System Bootloader: %OUTPUT_OS_PATH%\EFI\BOOT\BOOTX64.EFI
) else (
    echo Error: System Bootloader not found!
)

echo.
echo [Installer Bootloader]
if not exist "%OUTPUT_INSTALLER_PATH%" mkdir "%OUTPUT_INSTALLER_PATH%"

if exist "Build\BanJiuOS\%BUILD_TARGET%_VS2019\X64\BanJiuInstaller\OUTPUT\BanJiuInstaller.efi" (
    copy "Build\BanJiuOS\%BUILD_TARGET%_VS2019\X64\BanJiuInstaller\OUTPUT\BanJiuInstaller.efi" "%OUTPUT_INSTALLER_PATH%\setup.efi" /Y
    echo Success! Installer Bootloader: %OUTPUT_INSTALLER_PATH%\setup.efi
) else (
    echo Error: Installer Bootloader not found!
)

echo.
echo ======================================================
echo Build completed!
echo.
echo Directory structure:
echo   %OUTPUT_OS_PATH%\EFI\BOOT\BOOTX64.EFI    (系统引导)
echo   %OUTPUT_INSTALLER_PATH%\setup.efi        (安装程序引导)
echo ======================================================

cd /d "d:\Projects\BanJiuOS"

endlocal
pause