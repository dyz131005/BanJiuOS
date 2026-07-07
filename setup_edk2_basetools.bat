@echo off
echo Compiling EDK2 BaseTools...
echo =============================

set PYTHON_COMMAND=python
set NASM_PREFIX=D:\program\cpp\mingw64\bin\
set VS2019_PREFIX=D:\program\Microsoft Visual Studio\VC\Tools\MSVC\14.50.35717\
set VS_HOST=x64

call "D:\program\Microsoft Visual Studio\Common7\Tools\VsDevCmd.bat" -arch=x64

cd /d D:\program\EDK2\edk2

set WORKSPACE=D:\program\EDK2\edk2
set EDK_TOOLS_PATH=D:\program\EDK2\edk2\BaseTools
set BASE_TOOLS_PATH=D:\program\EDK2\edk2\BaseTools

call edksetup.bat ForceRebuild VS2026

echo.
echo BaseTools compilation completed.