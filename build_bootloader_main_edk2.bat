@echo off
chcp 65001 >nul

echo BanJiuOS Boot Loader Build Script
echo ===================================

set PYTHON_COMMAND=python
set NASM_PREFIX=D:\program\cpp\mingw64\bin\

set VS2019_PREFIX=D:\program\Microsoft Visual Studio\VC\Tools\MSVC\14.50.35717\
set VS_HOST=x64

call "D:\program\Microsoft Visual Studio\Common7\Tools\VsDevCmd.bat" -arch=x64

cd /d D:\program\EDK2\edk2

echo Setting up EDK II environment...
call edksetup.bat
set PATH=%PATH%;D:\program\EDK2\edk2\BaseTools\bin\Win64

echo.
echo Cleaning old build files...
powershell -Command "if (Test-Path 'Build\BanJiuOS') { Remove-Item -Path 'Build\BanJiuOS' -Recurse -Force }"
powershell -Command "if (Test-Path 'BanJiuOS') { Remove-Item -Path 'BanJiuOS' -Recurse -Force }"

echo.
echo Copying BanJiuOS module...
powershell -Command "New-Item -ItemType Directory -Path 'BanJiuOS' -Force"
powershell -Command "Copy-Item -Path 'd:\Projects\BanJiuOS\bootloader\BanJiuBootLoader.inf' -Destination 'BanJiuOS\' -Force"
powershell -Command "Copy-Item -Path 'd:\Projects\BanJiuOS\bootloader\BanJiuInstaller.inf' -Destination 'BanJiuOS\' -Force"
powershell -Command "Copy-Item -Path 'd:\Projects\BanJiuOS\bootloader\BanJiuOS.dsc' -Destination 'BanJiuOS\' -Force"
powershell -Command "Copy-Item -Path 'd:\Projects\BanJiuOS\bootloader\src\BootLoader.c' -Destination 'BanJiuOS\' -Force"
powershell -Command "Copy-Item -Path 'd:\Projects\BanJiuOS\bootloader\src\Installer.c' -Destination 'BanJiuOS\' -Force"

echo.
echo Building with VS2019 toolchain...
build -p BanJiuOS\BanJiuOS.dsc -a X64 -t VS2019 -b RELEASE

echo.
echo Copying output...
powershell -Command "if (Test-Path 'd:\Projects\BanJiuOS\iso\EFI\BOOT\BOOTX64.EFI') { Remove-Item -Path 'd:\Projects\BanJiuOS\iso\EFI\BOOT\BOOTX64.EFI' -Force }"
powershell -Command "if (Test-Path 'd:\Projects\BanJiuOS\iso\Installer\setup.efi') { Remove-Item -Path 'd:\Projects\BanJiuOS\iso\Installer\setup.efi' -Force }"

powershell -Command "if (Test-Path 'Build\BanJiuOS\RELEASE_VS2019\X64\BanJiuOS\BanJiuBootLoader\OUTPUT\BanJiuBootLoader.efi') { Copy-Item -Path 'Build\BanJiuOS\RELEASE_VS2019\X64\BanJiuOS\BanJiuBootLoader\OUTPUT\BanJiuBootLoader.efi' -Destination 'd:\Projects\BanJiuOS\iso\EFI\BOOT\BOOTX64.EFI' -Force; Write-Host 'BootLoader copied: d:\Projects\BanJiuOS\iso\EFI\BOOT\BOOTX64.EFI' } else { Write-Host 'BootLoader build failed! Source not found' }"
powershell -Command "if (Test-Path 'Build\BanJiuOS\RELEASE_VS2019\X64\BanJiuOS\BanJiuInstaller\OUTPUT\BanJiuInstaller.efi') { Copy-Item -Path 'Build\BanJiuOS\RELEASE_VS2019\X64\BanJiuOS\BanJiuInstaller\OUTPUT\BanJiuInstaller.efi' -Destination 'd:\Projects\BanJiuOS\iso\Installer\setup.efi' -Force; Write-Host 'Installer copied: d:\Projects\BanJiuOS\iso\Installer\setup.efi' } else { Write-Host 'Installer build failed! Source not found' }"

echo.
echo Done!

pause