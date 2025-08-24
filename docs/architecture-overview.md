# ESP32 DL1000 - Architecture Overview

## Project Status

This document outlines the current progress of refactoring the ESP32 DL1000 project from a complex manager-based architecture to a clean, robust state machine-driven architecture.

## Completed Work

### 1. StatusViewModel Refactor (✅ COMPLETE)

**Before:** Simple struct with direct pointer access
```cpp
typedef struct {
    const char* version = FIRMWARE_VERSION;
    const char* mac;
    const char* serial;
    DeviceStatus *statusDevice;
    NetworkStatus *statusNetwork;
    ServicesStatus *statusServices;
    char status[16]; 
} StatusViewModel;
```

**After:** Proper C++ class with encapsulation and dirty flag tracking
```cpp
class StatusViewModel {
public:
    // Getters and setters
    const char* getVersion() const;
    void setDeviceStatus(DeviceStatus status);
    // ... other getters/setters
    
    // Dirty flag management
    bool isDirty() const;
    void clearDirty();
    
private:
    DeviceStatus deviceStatus;
    NetworkStatus networkStatus;
    ConnectivityStatus connectivityStatus;
    ServicesStatus servicesStatus;
    bool dirty; // Efficiency flag
};
```

**Benefits:**
- **Encapsulation**: Private members prevent direct access
- **Efficiency**: Dirty flag allows managers to only update when needed  
- **Type Safety**: No more pointer dereferencing errors
- **Thread Safety**: Controlled access to status data

### 2. Backward Compatibility

Added enum aliases to prevent breaking existing code:
```cpp
// Backward compatibility aliases
typedef ServicesStatus ServiceStatus;
#define SERVICE_CONNECTED SERVICES_CONNECTED
#define NETWORK_NOT_CONNECTED NETWORK_DISCONNECTED
```

## Current Compilation Issues

The refactor has revealed several areas that need updating:

### 1. Manager Updates Required
- **ledManager.cpp**: Still using old pointer-based access pattern
- **displayManager.cpp**: Still using old struct member access
- **coreManager.cpp**: Some network function calls commented out

### 2. Macro Conflicts
- BlockNot library defines `STOPPED` and `DISCONNECTED` macros
- These conflict with our enum values
- Need proper scoping or renaming

## Next Steps

### Phase 1: Fix Compilation Issues
1. Update ledManager to use new StatusViewModel dirty flag pattern
2. Update displayManager to use getter methods
3. Resolve macro conflicts from BlockNot library

### Phase 2: State Machine Implementation  
1. Create NetworkingManager based on example templates
2. Create ServicesManager for MQTT handling
3. Extract OTA and Factory Reset to separate modules

### Phase 3: Integration
1. Update coreManager to orchestrate new managers
2. Remove legacy networking code
3. Test complete system integration

## Architecture Goals

### Clean Separation of Concerns
- **NetworkingManager**: Pure networking (Ethernet connection)
- **ConnectivityManager**: Internet connectivity validation  
- **ServicesManager**: MQTT and service coordination
- **OTAManager**: Firmware update handling
- **FactoryResetManager**: Device reset functionality

### Non-Blocking Operations
- All managers use state machines
- No blocking calls in main loops
- Efficient resource usage

### Maintainability
- Modular components
- Clear interfaces between managers
- Comprehensive documentation

## Files Modified

### Core StatusViewModel
- `src/statusViewModel.h` - New class definition
- `src/statusViewModel.cpp` - Implementation with dirty flag
- `include/definitions.h` - Removed old struct, added compatibility

### Updated for New StatusViewModel
- `src/coreManager.cpp` - Uses new setter methods
- `src/ledManager.cpp` - Partially updated (needs completion)
- `src/displayManager.cpp` - Needs updating

### Next Files to Create
- `src/networkingManager.h/cpp` - Ethernet state machine
- `src/servicesManager.h/cpp` - MQTT state machine  
- `src/otaManager.h/cpp` - OTA update module
- `src/factoryResetManager.h/cpp` - Factory reset module

## Key Design Patterns

### Dirty Flag Pattern
```cpp
// Efficient updates - only process when something changed
if (statusViewModel.isDirty()) {
    updateDisplay();
    updateLEDs();
    statusViewModel.clearDirty();
}
```

### State Machine Pattern
```cpp
class NetworkingManager {
    enum State { DISCONNECTED, CONNECTING, CONNECTED };
    void loop() {
        switch(currentState) {
            case DISCONNECTED: handleDisconnected(); break;
            case CONNECTING: handleConnecting(); break;
            case CONNECTED: handleConnected(); break;
        }
    }
};
```

## Testing Strategy

1. **Syntax Validation**: PlatformIO build success
2. **Unit Testing**: Individual state machine testing
3. **Integration Testing**: Manager coordination
4. **Hardware Testing**: Device functionality validation

---

*This document will be updated as the refactor progresses.*
