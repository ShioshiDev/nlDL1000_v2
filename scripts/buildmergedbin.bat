echo Merging binaries into nlDL1000_Factory.bin...
python "C:\Users\Shioshi\.platformio\packages\tool-esptoolpy@src-2ee4b59ebe071879a8d095e718e04167\esptool.py" ^
    --chip esp32s3 ^
    merge_bin ^
    -o nlDL1000_Factory.bin ^
    --flash_mode dio ^
    --flash_size 8MB ^
    0x0000 C:\Users\Shioshi\Documents\PlatformIO\Projects\nlDL1000_Factory\.pio\build\WithPartitions\bootloader.bin ^
    0x8000 C:\Users\Shioshi\Documents\PlatformIO\Projects\nlDL1000_Factory\.pio\build\WithPartitions\partitions.bin ^
    0xe000 C:\Users\Shioshi\.platformio\packages\framework-arduinoespressif32-src-77c8e93767360b28deee4aedf5d0a1ab\tools\partitions\boot_app0.bin ^
    0x430000 .pio\build\WithPartitions\firmware.bin ^
    0x630000 .pio\build\WithPartitions\littlefs.bin
