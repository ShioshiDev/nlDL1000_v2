python "C:\Users\Shioshi\.platformio\packages\tool-esptoolpy@src-2ee4b59ebe071879a8d095e718e04167\esptool.py" 
    --chip esp32s3 merge_bin -o merged-flash.bin 
    --flash_mode dio 
    --flash_size 8MB 
    0x0000 C:\Users\Shioshi\Documents\PlatformIO\Projects\nlDL1000_Factory\.pio\build\esp32-s3-devkitc-1\bootloader.bin 
    0x8000 C:\Users\Shioshi\Documents\PlatformIO\Projects\nlDL1000_Factory\.pio\build\esp32-s3-devkitc-1\partitions.bin 
    0xe000 C:\Users\Shioshi\.platformio\packages\framework-arduinoespressif32-src-77c8e93767360b28deee4aedf5d0a1ab\tools\partitions\boot_app0.bin 
    0x30000 .pio\build\esp32-s3-devkitc-1\firmware.bin 
    0x430000 .pio\build\esp32-s3-devkitc-1\firmware.bin 
    0x630000 .pio\build\esp32-s3-devkitc-1\littlefs.bin