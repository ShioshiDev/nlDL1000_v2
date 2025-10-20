# nlDL1000 v2 - ESP32 IoT Device Platform

A modular ESP32-based IoT device platform built with PlatformIO, featuring Modbus monitoring, WiFi connectivity, display management, and comprehensive service architecture.

## 🚀 Features

- **Modular Architecture**: Clean separation of concerns with dedicated managers for different functionalities
- **Modbus Monitoring**: Real-time monitoring and data collection from Modbus devices
- **WiFi Connectivity**: Robust networking with connection management and status monitoring
- **Display Management**: Flexible display output based on status view model
- **LED Indicators**: Visual status feedback through LED management
- **Service Framework**: Extensible background services (MQTT, NovaLogic, TagoIO)
- **Menu System**: Interactive user interface for device configuration
- **Logging System**: Comprehensive logging for debugging and monitoring

## 📁 Project Structure

```
├── src/
│   ├── app.cpp                 # ESP32 boot code and main application loop
│   ├── coreApplication.cpp/h   # Core orchestration engine
│   ├── statusViewModel.cpp/h   # Central status information structure
│   ├── managers/               # Core system managers
│   │   ├── connectivityManager # Connectivity status management
│   │   ├── displayManager      # Display output management
│   │   ├── ledManager          # LED indicator control
│   │   ├── loggingManager      # System logging
│   │   ├── menuManager         # User interface menu system
│   │   ├── modbusMonitorManager# Modbus device monitoring
│   │   ├── networkingManager   # Network initialization and communication
│   │   └── servicesManager     # Background services orchestration
│   └── services/               # Background service implementations
│       ├── baseService         # Base service class
│       ├── modbusMonitorService# Modbus monitoring service
│       ├── novaLogicService    # NovaLogic integration
│       └── tagoIOService       # TagoIO cloud integration
├── include/                    # Header files and definitions
├── data/                       # Configuration and data files
├── scripts/                    # Build and deployment scripts
└── docs/                       # Project documentation
```

## 🛠️ Build System

The project uses PlatformIO with custom build scripts:

- `scripts/buildandflashfactory.bat` - Build and flash factory firmware
- `scripts/buildmergedbin.bat` - Create merged binary
- `scripts/flashfactory.bat` - Flash factory configuration
- `scripts/flashmergedbin.bat` - Flash merged binary

## 🔧 Development

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- ESP32 development board
- VS Code with PlatformIO extension

### Building

```bash
# Build the project
platformio run

# Build and upload
platformio run --target upload

# Monitor serial output
platformio device monitor
```

Or use the provided VS Code tasks:
- **PlatformIO Build** - Standard build
- **Build and Flash Factory** - Factory configuration build and flash

### Configuration

1. Copy `include/credentials.h.example` to `include/credentials.h`
2. Update WiFi and service credentials
3. Configure device settings in `data/config.json`
4. Set up zone definitions in `data/zones.json`

## 📋 Architecture

The application follows a modular design pattern:

- **Core Application**: Central orchestration and lifecycle management
- **Status View Model**: Shared state management across all components
- **Managers**: Dedicated modules for specific system functions
- **Services**: Background processing and external integrations
- **Non-blocking Design**: Ensures smooth operation and responsiveness

## 🔗 Services Integration

- **Modbus Monitoring**: Real-time data collection from industrial devices
- **MQTT**: Message queuing for IoT communication
- **NovaLogic**: Custom service integration
- **TagoIO**: Cloud data visualization and analytics

## 📚 Documentation

Detailed documentation is available in the `docs/` directory:

- [Architecture Overview](docs/architecture-overview.md)
- [Technical Specifications](docs/technical-specifications.md)
- [Menu System](docs/menu-system.md)
- [Display Configuration](docs/display.txt)
- [RTC Setup](docs/RTC.txt)

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes following the modular architecture
4. Test thoroughly on hardware
5. Submit a pull request

## 📄 License

This project is part of the nlDL1000 device platform. Please refer to the license file for terms and conditions.

## 🐛 Issues & Support

Please report issues and feature requests through the GitHub issue tracker.