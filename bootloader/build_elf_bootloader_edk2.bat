@echo off
REM BanJiuOS ELF Kernel Boot Loader Build Script (EDK II - Windows)

setlocal

echo BanJiuOS ELF Kernel Boot Loader Build Script
echo =============================================

set PYTHON_COMMAND=python
set NASM_PREFIX=D:\program\cpp\mingw64\bin\
set VS_ROOT=D:\program\Microsoft Visual Studio\18\Community
set VS2019_PREFIX=
set VS_HOST=x64

for /d %%D in ("%VS_ROOT%\VC\Tools\MSVC\*") do (
    if not defined VS2019_PREFIX set VS2019_PREFIX=%%~fD\
)

if not exist "%VS_ROOT%\Common7\Tools\VsDevCmd.bat" (
    echo ERROR: VsDevCmd.bat not found under %VS_ROOT%
    exit /b 1
)

call "%VS_ROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64

set EDK2_PATH=D:\program\EDK2\edk2
set BANJUOS_PATH=d:\Projects\BanJiuOS\bootloader
set BUILD_TARGET=DEBUG
set BUILD_ARCH=X64

set OUTPUT_OS_PATH=d:\Projects\BanJiuOS\output\OS

echo EDK2 Path:     %EDK2_PATH%
echo BanJiuOS Path: %BANJUOS_PATH%
echo Build Target:  %BUILD_TARGET%
echo Build Arch:    %BUILD_ARCH%
echo System Output: %OUTPUT_OS_PATH%

cd /d "%EDK2_PATH%"

echo.
echo Cleaning old build files...
if exist "Build\BanJiuOS" rmdir /s /q "Build\BanJiuOS"
if exist "BanJiuOS" rmdir /s /q "BanJiuOS"

echo.
echo Cleaning output directories...
powershell -Command "if (Test-Path '%OUTPUT_OS_PATH%\EFI\BOOT') { Remove-Item -Path '%OUTPUT_OS_PATH%\EFI\BOOT' -Recurse -Force }"

echo.
echo Setting up EDK II environment...
call edksetup.bat

set EDK_TOOLS_BIN=%EDK_TOOLS_PATH%\Bin\Win64
set PATH=%EDK_TOOLS_BIN%;%PATH%

echo.
echo Copying ELF Boot Loader module to EDK II workspace...
if not exist "BanJiuOS" mkdir "BanJiuOS"
powershell -Command "Copy-Item -Path '%BANJUOS_PATH%\ElfBootLoader.inf' -Destination 'BanJiuOS\' -Force"
powershell -Command "Copy-Item -Path '%BANJUOS_PATH%\BanJiuInstaller.inf' -Destination 'BanJiuOS\' -Force"

powershell -Command "Copy-Item -Path '%BANJUOS_PATH%\BanJiuOS.dsc' -Destination 'BanJiuOS\BanJiuOS.dsc' -Force"

powershell -Command "Copy-Item -Path '%BANJUOS_PATH%\src\ElfBootLoader.c' -Destination 'BanJiuOS\' -Force"
powershell -Command "Copy-Item -Path '%BANJUOS_PATH%\src\EfiElfLoader.c' -Destination 'BanJiuOS\' -Force"
powershell -Command "Copy-Item -Path '%BANJUOS_PATH%\src\EfiElfLoader.h' -Destination 'BanJiuOS\' -Force"
powershell -Command "Copy-Item -Path '%BANJUOS_PATH%\src\SetupPaging.asm' -Destination 'BanJiuOS\' -Force"

echo.
echo Building ELF Boot Loader with VS2019...
build -p BanJiuOS\BanJiuOS.dsc -a %BUILD_ARCH% -t VS2019 -b %BUILD_TARGET%

echo.
echo Creating output directories and copying files...
powershell -Command "New-Item -ItemType Directory -Path '%OUTPUT_OS_PATH%\EFI\BOOT' -Force | Out-Null"
powershell -Command "New-Item -ItemType Directory -Path '%OUTPUT_OS_PATH%\System' -Force | Out-Null"

set BOOTLOADER_PATH=Build\BanJiuOS\%BUILD_TARGET%_VS2019\X64\ElfBootLoader.efi

if exist "%EDK2_PATH%\%BOOTLOADER_PATH%" (
    powershell -Command "Copy-Item -Path '%EDK2_PATH%\%BOOTLOADER_PATH%' -Destination '%OUTPUT_OS_PATH%\EFI\BOOT\BOOTX64.EFI' -Force"
    echo.
    echo =============================================
    echo Build SUCCESS!
    echo =============================================
    echo System Bootloader (ELF): %OUTPUT_OS_PATH%\EFI\BOOT\BOOTX64.EFI
    echo =============================================
) else (
    echo.
    echo =============================================
    echo Build FAILED!
    echo =============================================
    echo ELF Bootloader not found in build output!
    echo Expected path: %EDK2_PATH%\%BOOTLOADER_PATH%
    echo =============================================
)

cd /d "d:\Projects\BanJiuOS"

endlocal
