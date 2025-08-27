# NovaLogic DL1000 Factory Device

## Project Overview
An ESP32 Arduino PlatformIO project in C++.

## Tasks
- **Create Menu System**: Let's add the menu button functionality. If the Menu 'M' button is pressed we want to display the a basic menu using the build in MUI functionality of our u8g2 display library. We already have a place in the display manager for the menu mode code, and we just have to configure the key pad handler to then pass on the key events to a menu key handler. We are going to add a couple of options to the menu:
- Display Ethernet Information: This should display a form/screen which displays the important information from the networkingManager printEthernetStatus function for viewing at a glance.
- Settings: Here we should allow for adjusting certain application settings which should then be available throughout the application. Settings should be securely stored in NVS using best practices. Let's add here a couple of options for the logging system: Log to file (enable/disable checkbox) and Log to MQTT (enable/disable checkbox). We can then use those settings within the loggingManager to enable/disable the respective logging methods.

## Optional Tasks
- Update documentation with the overview, architecture, and technical notes of our current project.

## Future Enhancements (Not Critical)
- **Log Compression**: Compress old log files to save storage space
- **Structured Logging**: Add JSON-structured logging for better parsing and analysis
