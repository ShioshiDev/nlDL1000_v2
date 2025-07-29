@echo off
echo Flashing ESP32-S3 with merged binary...

set COM_PORT=COM13
set BAUD_RATE=921600

rem Check if COM port was provided as an argument
if not "%~1"=="" set COM_PORT=%~1

echo Using port: %COM_PORT%
echo.

python "C:\Users\Shioshi\.platformio\packages\tool-esptoolpy@src-2ee4b59ebe071879a8d095e718e04167\esptool.py" ^
  --chip esp32s3 ^
  --port %COM_PORT% ^
  --baud %BAUD_RATE% ^
  --before default_reset ^
  --after hard_reset ^
  write_flash ^
  --flash_mode dio ^
  --flash_size 8MB ^
  --flash_freq 80m ^
  0x0 merged-flash.bin

if %ERRORLEVEL% NEQ 0 (
  echo.
  echo Error: Flash failed!
  exit /b %ERRORLEVEL%
) else (
  echo.
  echo Flash completed successfully!
)