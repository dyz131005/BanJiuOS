@echo off
chcp 65001

echo BanJiuOS VMware GDB Debugger
echo =============================
echo.
echo 请确保VMware虚拟机已配置为允许远程调试：
echo 1. 关闭虚拟机
echo 2. 编辑虚拟机设置
echo 3. 添加调试端口，选择添加-调试端口
echo 4. 启动虚拟机（实际调试端口为8864）
echo.
pause

echo.
echo 启动GDB调试器...
"D:\program\cpp\mingw64\bin\gdb.exe" -x "d:\Projects\BanJiuOS\debug_vm.gdb"