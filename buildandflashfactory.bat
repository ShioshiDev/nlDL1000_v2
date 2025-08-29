@echo off
echo Build the project and flash the factory partition with latest firmware.bin...
echo.

set COM_PORT=COM13
set BAUD_RATE=921600
set BUILD_DIR=.pio\build\esp32-s3-devkitc-1

rem Check if COM port was provided as an argument
if not "%~1"=="" set COM_PORT=%~1

echo Using port: %COM_PORT%
echo Build directory: %BUILD_DIR%
echo.

rem Step 1: Build the project using PlatformIO
echo ============================================
echo Step 1: Building project with PlatformIO...
echo ============================================
platformio run

if %ERRORLEVEL% NEQ 0 (
  echo.
  echo ERROR: Build failed! Cannot proceed with flashing.
  echo Please fix compilation errors and try again.
  pause
  exit /b %ERRORLEVEL%
)

echo.
echo Build completed successfully!
echo.

rem Step 2: Check if firmware.bin exists
if not exist "%BUILD_DIR%\firmware.bin" (
  echo ERROR: firmware.bin not found in %BUILD_DIR%
  echo Build may have failed or incomplete.
  pause
  exit /b 1
)

rem Step 3: Flash the firmware to factory partition
echo ============================================
echo Step 2: Flashing firmware to factory partition...
echo ============================================
echo Target file: %BUILD_DIR%\firmware.bin
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
  echo ERROR: Flash failed!
  echo Check COM port connection and try again.
  pause
  exit /b %ERRORLEVEL%
) else (
  echo.
  echo ============================================
  echo SUCCESS: Factory partition flash completed!
  echo ============================================
  echo The device should now restart with the new firmware.
)