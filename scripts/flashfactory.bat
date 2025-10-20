@echo off
echo Flashing only factory partition with latest firmware.bin...

set COM_PORT=COM13
set BAUD_RATE=921600
set BUILD_DIR=.pio\build\WithPartitions

rem Check if COM port was provided as an argument
if not "%~1"=="" set COM_PORT=%~1

echo Using port: %COM_PORT%
echo Using firmware from: %BUILD_DIR%\firmware.bin
echo.

python "C:\Users\Shioshi\.platformio\packages\tool-esptoolpy@src-2ee4b59ebe071879a8d095e718e04167\esptool.py" ^
  --chip esp32s3 ^
  --port %COM_PORT% ^
  --baud %BAUD_RATE% ^
  --before default_reset ^
  --after hard_reset ^
  write_flash ^
  --flash_mode dio ^
  --flash_freq 80m ^
  0x430000 %BUILD_DIR%\firmware.bin

if %ERRORLEVEL% NEQ 0 (
  echo.
  echo Error: Flash failed!
  exit /b %ERRORLEVEL%
) else (
  echo.
  echo Factory partition flash completed successfully!
)