echo -off
cls
echo ================================================
echo      BanJiuOS UEFI Boot Loader
echo ================================================
echo.
echo 正在搜索可引导设备...
echo.

for %d in (blk0 blk1 blk2 blk3 blk4 blk5) do (
  echo 尝试挂载 %d...
  map -r
  if exist %d:\EFI\BOOT\BOOTX64.EFI (
    echo 找到引导设备: %d:
    echo 正在启动 BanJiuOS...
    %d:\EFI\BOOT\BOOTX64.EFI
    goto :end
  )
)

echo 未找到可引导设备!
pause
:end