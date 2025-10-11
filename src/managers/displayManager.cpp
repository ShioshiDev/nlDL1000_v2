#include "displayManager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "loggingManager.h"
#include <BlockNot.h>

static const char *TAG = "DisplayManager";

extern U8G2_SH1106_128X64_NONAME_1_HW_I2C hwDisplay;

extern StatusViewModel displayViewModel;
extern DisplayMode displayMode;
extern uint8_t factoryResetSelection;
extern ServicesManager servicesManager;
extern NetworkingManager networkingManager;
extern LoggingManager loggingManager;

extern QueueHandle_t displayQueueHandle;
extern SemaphoreHandle_t displayModelMutex;
extern SemaphoreHandle_t i2cMutex;

// Menu system variables
static MenuState currentMenuState = MENU_MAIN;
static int menuSelection = 0;
static int maxMenuItems = 0;
static int mainMenuScroll = 0;     // For scrolling through main menu
static int settingsMenuScroll = 0; // For scrolling through settings menu
static int ethernetInfoScroll = 0; // For scrolling through ethernet info
static int maxEthernetLines = 0;   // Total lines of ethernet info
static int deviceInfoScroll = 0;   // For scrolling through device info
static int maxDeviceLines = 0;     // Total lines of device info
static AppSettings appSettings;

// Display power management variables
static bool displayDimmed = false;
static bool displayOff = false;

// New menu system
static MenuManager *menuManager = nullptr;
static Menu *mainMenu = nullptr;
static Menu *deviceInfoMenu = nullptr;
static Menu *ethernetInfoMenu = nullptr;
static Menu *settingsMenu = nullptr;
static Menu *loggingSettingsMenu = nullptr;

// Menu timeout timer (30 seconds)
static BlockNot menuTimeoutTimer(30, SECONDS);

// Display power management timers
static BlockNot displayDimTimer(30, SECONDS); // 30 seconds before dimming
static BlockNot displayOffTimer(60, SECONDS); // 60 seconds before turning off

void TaskDisplayUpdate(void *pvParameters)
{
    for (;;)
    {
        updateDisplay();
        vTaskDelay(DISPLAY_UPDATE_INTERVAL / portTICK_PERIOD_MS);
    }
}

void updateDisplay()
{
    // Skip display updates during OTA to prevent I2C conflicts and reduce system load
    if (displayViewModel.isOTAActive())
    {
        return;
    }
    
    // Check for menu timeout
    if (displayMode == MENU && menuTimeoutTimer.TRIGGERED)
    {
        LOG_INFO(TAG, "Menu timeout - returning to normal display");
        displayMode = NORMAL;
        currentMenuState = MENU_MAIN;
        menuSelection = 0;
        mainMenuScroll = 0;
        settingsMenuScroll = 0;
        ethernetInfoScroll = 0;
        deviceInfoScroll = 0;
        menuTimeoutTimer.STOP; // Stop the timer when timeout occurs
    }

    if (xSemaphoreTake(i2cMutex, DISPLAY_UPDATE_INTERVAL) == pdTRUE)
    {
        if (xSemaphoreTake(displayModelMutex, DISPLAY_UPDATE_INTERVAL) == pdTRUE)
        {
            // Check display power management
            checkDisplayPowerManagement();

            if (displayMode == NORMAL)
            {
                updateDisplayNormal();
            }
            else if (displayMode == MENU)
            {
                // Use new menu system if available, fall back to old system
                if (menuManager && menuManager->isMenuActive())
                {
                    // Check timeout in the new menu system
                    menuManager->checkTimeout();

                    // Draw using new menu system with safe page handling
                    hwDisplay.firstPage();
                    int pageCount = 0;
                    do
                    {
                        pageCount++;
                        if (pageCount > 10)
                        { // Safety check to prevent infinite loop
                            LOG_ERROR(TAG, "Menu draw loop exceeded safety limit, breaking");
                            break;
                        }
                        menuManager->draw();
                    } while (hwDisplay.nextPage());
                }
                else
                {
                    // Fall back to old menu system
                    updateDisplayMenu();
                }
            }
            else if (displayMode == FACTORY_RESET_CONFIRM)
            {
                updateDisplayFactoryResetConfirm();
            }
            xSemaphoreGive(displayModelMutex);
        }
        else
        {
            LOG_WARN(TAG, "Failed to take displayModelMutex");
        }
        xSemaphoreGive(i2cMutex);
    }
    else
    {
        LOG_WARN(TAG, "Failed to take i2cMutex");
    }
}

void updateDisplayNormal()
{
    hwDisplay.firstPage();
    do
    {
        hwDisplay.setFont(u8g_font_unifont);
        hwDisplay.drawStr(0, 20, DEVICE_FRIENDLY_ID);
        hwDisplay.drawHLine(4, 22, 120);
        hwDisplay.setFont(u8g2_font_5x7_tr);
        // Display Modbus status
        ModbusMonitorStatus modbusStatus = displayViewModel.getModbusStatus();
        hwDisplay.drawStr(4, 34, "Modbus: ");
        const char* modbusStatusStr;
        switch (modbusStatus) {
            case MODBUS_INACTIVE: modbusStatusStr = "INACTIVE"; break;
            case MODBUS_ACTIVE: modbusStatusStr = "ACTIVE"; break;
            case MODBUS_VALID: modbusStatusStr = "VALID"; break;
            case MODBUS_INVALID: modbusStatusStr = "INVALID"; break;
            default: modbusStatusStr = "UNKNOWN"; break;
        }
        int strWidth = hwDisplay.getStrWidth(modbusStatusStr);
        hwDisplay.drawStr((128 - strWidth - 4), 34, modbusStatusStr);
        
        // Display generator data
        hwDisplay.drawStr(4, 44, "Gen: ");
        char genDataStr[32];
        float genWatts = modbusMonitorManager.getGeneratorTotalWatts();
        float genVoltage = modbusMonitorManager.getGeneratorL1NVoltage();
        if (genWatts != 0.0f || genVoltage != 0.0f) {
            snprintf(genDataStr, sizeof(genDataStr), "%.0fW %.1fV", genWatts, genVoltage);
        } else {
            snprintf(genDataStr, sizeof(genDataStr), "No Data");
        }
        strWidth = hwDisplay.getStrWidth(genDataStr);
        hwDisplay.drawStr((128 - strWidth - 4), 44, genDataStr);

        DeviceStatus deviceStatus = displayViewModel.getDeviceStatus();
        NetworkStatus networkStatus = displayViewModel.getNetworkStatus();
        ConnectivityStatus connectivityStatus = displayViewModel.getConnectivityStatus();
        ServicesStatus servicesStatus = displayViewModel.getServicesStatus();

        hwDisplay.setFont(u8g2_font_squeezed_b7_tr);

        if (networkStatus == NETWORK_STARTED)
        {
            hwDisplay.drawXBM(1, 48, 17, 16, bitImage_Cloud);
            hwDisplay.setDrawColor(2);
            hwDisplay.drawLine(7, 57, 12, 57);
            hwDisplay.setDrawColor(1);
            hwDisplay.drawLine(7, 55, 12, 60);
            hwDisplay.drawLine(12, 55, 7, 60);
            if (deviceStatus == DEVICE_STARTED)
                hwDisplay.drawStr(24, 58, "Network Ready...");
        }
        if ((networkStatus == NETWORK_DISCONNECTED) || (networkStatus == NETWORK_STOPPED))
        {
            hwDisplay.drawXBM(1, 48, 17, 16, bitImage_Cloud);
            hwDisplay.setDrawColor(2);
            hwDisplay.drawLine(7, 57, 12, 57);
            hwDisplay.setDrawColor(1);
            hwDisplay.drawLine(7, 55, 12, 60);
            hwDisplay.drawLine(12, 55, 7, 60);
            if (deviceStatus == DEVICE_STARTED)
                hwDisplay.drawStr(24, 58, "Not Connected");
        }
        if (networkStatus == NETWORK_CONNECTED)
        {
            hwDisplay.drawXBM(1, 48, 17, 16, bitImage_Cloud);
            hwDisplay.setDrawColor(2);
            hwDisplay.drawLine(7, 57, 12, 57);
            hwDisplay.setDrawColor(1);
            hwDisplay.drawLine(7, 55, 12, 60);
            hwDisplay.drawLine(12, 55, 7, 60);
            if (deviceStatus == DEVICE_STARTED)
                hwDisplay.drawStr(24, 58, "Connecting...");
        }
        if (networkStatus == NETWORK_LOST_IP)
        {
            hwDisplay.drawXBM(1, 48, 17, 16, bitImage_Cloud);
            hwDisplay.setDrawColor(2);
            hwDisplay.drawLine(7, 57, 12, 57);
            hwDisplay.setDrawColor(1);
            hwDisplay.drawLine(7, 55, 12, 60);
            hwDisplay.drawLine(12, 55, 7, 60);
            if (deviceStatus == DEVICE_STARTED)
                hwDisplay.drawStr(24, 58, "Lost IP...");
        }
        if ((networkStatus == NETWORK_CONNECTED_IP) && (connectivityStatus < CONNECTIVITY_ONLINE))
        {
            hwDisplay.setBitmapMode(1);
            hwDisplay.drawXBM(1, 48, 17, 16, bitImage_Cloud);
            hwDisplay.setDrawColor(2);
            hwDisplay.drawLine(6, 57, 14, 57);
            hwDisplay.setDrawColor(1);
            hwDisplay.drawEllipse(10, 57, 3, 3);
            if (deviceStatus == DEVICE_STARTED)
                hwDisplay.drawStr(24, 58, "Network Connected");
        }
        if ((networkStatus == NETWORK_CONNECTED_IP) && (connectivityStatus == CONNECTIVITY_ONLINE))
        {
            hwDisplay.setBitmapMode(1);
            hwDisplay.drawXBM(1, 48, 17, 16, bitImage_CloudSync);
            if (deviceStatus == DEVICE_STARTED)
            {
                // Get individual service statuses
                ServiceStatus novaLogicStatus = servicesManager.getNovaLogicService().getStatus();
                
#ifdef TEST_ALL_SERVICES
                ServiceStatus tagoIOStatus = servicesManager.getTagoIOService().getStatus();

                // Display detailed service status information (both services)
                if (novaLogicStatus == SERVICE_CONNECTED && tagoIOStatus == SERVICE_CONNECTED)
                {
                    hwDisplay.drawStr(24, 58, "Services Connected");
                }
                else if (novaLogicStatus == SERVICE_CONNECTING || tagoIOStatus == SERVICE_CONNECTING)
                {
                    if (novaLogicStatus == SERVICE_CONNECTING && tagoIOStatus == SERVICE_CONNECTING)
                        hwDisplay.drawStr(24, 58, "Connecting Services");
                    else if (novaLogicStatus == SERVICE_CONNECTING)
                        hwDisplay.drawStr(24, 58, "Connecting NovaLogic");
                    else
                        hwDisplay.drawStr(24, 58, "Connecting TagoIO");
                }
                else if (novaLogicStatus == SERVICE_ERROR || tagoIOStatus == SERVICE_ERROR)
                {
                    if (novaLogicStatus == SERVICE_ERROR && tagoIOStatus == SERVICE_ERROR)
                        hwDisplay.drawStr(24, 58, "Services Error");
                    else if (novaLogicStatus == SERVICE_ERROR)
                        hwDisplay.drawStr(24, 58, "NovaLogic Error");
                    else
                        hwDisplay.drawStr(24, 58, "TagoIO Error");
                }
                else if (novaLogicStatus == SERVICE_NOT_CONNECTED || tagoIOStatus == SERVICE_NOT_CONNECTED)
                {
                    if (novaLogicStatus == SERVICE_NOT_CONNECTED && tagoIOStatus == SERVICE_NOT_CONNECTED)
                        hwDisplay.drawStr(24, 58, "Services Timeout");
                    else if (novaLogicStatus == SERVICE_NOT_CONNECTED)
                        hwDisplay.drawStr(24, 58, "NovaLogic Timeout");
                    else
                        hwDisplay.drawStr(24, 58, "TagoIO Timeout");
                }
                else if (novaLogicStatus == SERVICE_CONNECTED)
                {
                    hwDisplay.drawStr(24, 58, "NovaLogic OK");
                }
                else if (tagoIOStatus == SERVICE_CONNECTED)
                {
                    hwDisplay.drawStr(24, 58, "TagoIO OK");
                }
                else
                {
                    hwDisplay.drawStr(24, 58, "Internet Connected");
                }
#else
                // Display service status information (NovaLogic only)
                if (novaLogicStatus == SERVICE_CONNECTED)
                {
                    hwDisplay.drawStr(24, 58, "NovaLogic Connected");
                }
                else if (novaLogicStatus == SERVICE_CONNECTING)
                {
                    hwDisplay.drawStr(24, 58, "Connecting NovaLogic");
                }
                else if (novaLogicStatus == SERVICE_ERROR)
                {
                    hwDisplay.drawStr(24, 58, "NovaLogic Error");
                }
                else if (novaLogicStatus == SERVICE_NOT_CONNECTED)
                {
                    hwDisplay.drawStr(24, 58, "NovaLogic Timeout");
                }
                else
                {
                    hwDisplay.drawStr(24, 58, "Internet Connected");
                }
#endif
            }
        }

        if (deviceStatus >= DEVICE_UPDATING)
        {
            hwDisplay.drawXBM(112, 48, 15, 16, bitImage_FileDownload);
            // hwDisplay.setFont(u8g2_font_5x7_tr);
            hwDisplay.drawStr(32, 58, displayViewModel.getStatusString());
        }

    } while (hwDisplay.nextPage());
}

void updateDisplayMenu()
{
    hwDisplay.firstPage();
    do
    {
        switch (currentMenuState)
        {
        case MENU_MAIN:
            drawMainMenu();
            break;
        case MENU_DEVICE_INFO:
            drawDeviceInfoMenu();
            break;
        case MENU_ETHERNET_INFO:
            drawEthernetInfoMenu();
            break;
        case MENU_SETTINGS:
            drawSettingsMenu();
            break;
        case MENU_SETTINGS_LOGGING:
            drawLoggingSettingsMenu();
            break;
        }
    } while (hwDisplay.nextPage());
}

void updateDisplayFactoryResetConfirm()
{
    hwDisplay.firstPage();
    do
    {
        hwDisplay.setFont(u8g_font_unifont);
        hwDisplay.drawStr(15, 15, "FACTORY RESET");

        hwDisplay.setFont(u8g2_font_5x7_tr);
        hwDisplay.drawStr(10, 28, "Restore factory firmware");
        hwDisplay.drawStr(10, 38, "and erase all data?!");

        hwDisplay.drawHLine(5, 45, 118);

        // Draw Cancel button
        if (factoryResetSelection == 0)
        {
            // Selected - draw with background
            hwDisplay.setDrawColor(1);
            hwDisplay.drawBox(15, 52, 35, 12);
            hwDisplay.setDrawColor(0);
            hwDisplay.drawStr(18, 60, "CANCEL");
            hwDisplay.setDrawColor(1);
        }
        else
        {
            // Not selected - normal text
            hwDisplay.drawStr(18, 60, "CANCEL");
        }

        // Draw Confirm button
        if (factoryResetSelection == 1)
        {
            // Selected - draw with background
            hwDisplay.setDrawColor(1);
            hwDisplay.drawBox(75, 52, 40, 12);
            hwDisplay.setDrawColor(0);
            hwDisplay.drawStr(78, 60, "CONFIRM");
            hwDisplay.setDrawColor(1);
        }
        else
        {
            // Not selected - normal text
            hwDisplay.drawStr(78, 60, "CONFIRM");
        }

    } while (hwDisplay.nextPage());
}

void startMenuTimeout()
{
    menuTimeoutTimer.START_RESET;
    LOG_DEBUG(TAG, "Menu timeout timer started (30 seconds)");
}

void handleMenuKeyPress(char key)
{
    // Reset menu timeout timer on any key press
    menuTimeoutTimer.RESET;

    // Check if new menu system should handle this
    if (menuManager && menuManager->isMenuActive())
    {
        handleMenuKeyPressNew(key);
        return;
    }

    switch (key)
    {
    case 'U': // Up
        if (currentMenuState == MENU_ETHERNET_INFO)
        {
            // Handle scrolling in Ethernet info
            if (ethernetInfoScroll > 0)
            {
                ethernetInfoScroll--;
            }
        }
        else if (currentMenuState == MENU_DEVICE_INFO)
        {
            // Handle scrolling in Device info
            if (deviceInfoScroll > 0)
            {
                deviceInfoScroll--;
            }
        }
        else if (currentMenuState == MENU_MAIN)
        {
            // Handle main menu navigation with scrolling
            if (menuSelection > 0)
            {
                menuSelection--;
                // Adjust scroll if selection goes above visible area
                if (menuSelection < mainMenuScroll)
                {
                    mainMenuScroll = menuSelection;
                }
            }
            else
            {
                // Wrap to bottom
                menuSelection = maxMenuItems - 1;
                // Adjust scroll to show the last item
                int visibleItems = 3;
                if (maxMenuItems > visibleItems)
                {
                    mainMenuScroll = maxMenuItems - visibleItems;
                }
                else
                {
                    mainMenuScroll = 0;
                }
            }
        }

        else
        {
            // Normal menu navigation for other menus
            if (menuSelection > 0)
            {
                menuSelection--;
            }
            else
            {
                menuSelection = maxMenuItems - 1; // Wrap to bottom
            }
        }
        break;
    case 'D': // Down
        if (currentMenuState == MENU_ETHERNET_INFO)
        {
            // Handle scrolling in Ethernet info
            int visibleLines = 4;
            if (ethernetInfoScroll + visibleLines < maxEthernetLines)
            {
                ethernetInfoScroll++;
            }
        }
        else if (currentMenuState == MENU_DEVICE_INFO)
        {
            // Handle scrolling in Device info
            int visibleLines = 3;
            if (deviceInfoScroll + visibleLines < maxDeviceLines)
            {
                deviceInfoScroll++;
            }
        }
        else if (currentMenuState == MENU_MAIN)
        {
            // Handle main menu navigation with scrolling
            if (menuSelection < maxMenuItems - 1)
            {
                menuSelection++;
                // Adjust scroll if selection goes below visible area
                int visibleItems = 3;
                if (menuSelection >= mainMenuScroll + visibleItems)
                {
                    mainMenuScroll = menuSelection - visibleItems + 1;
                }
            }
            else
            {
                // Wrap to top
                menuSelection = 0;
                mainMenuScroll = 0;
            }
        }
        else if (currentMenuState == MENU_SETTINGS)
        {
            // Handle settings menu navigation with scrolling
            if (menuSelection < maxMenuItems - 1)
            {
                menuSelection++;
                // Adjust scroll if selection goes below visible area
                int visibleItems = 3;
                if (menuSelection >= settingsMenuScroll + visibleItems)
                {
                    settingsMenuScroll = menuSelection - visibleItems + 1;
                }
            }
            else
            {
                // Wrap to top
                menuSelection = 0;
                settingsMenuScroll = 0;
            }
        }

        else
        {
            // Normal menu navigation for other menus
            if (menuSelection < maxMenuItems - 1)
            {
                menuSelection++;
            }
            else
            {
                menuSelection = 0; // Wrap to top
            }
        }
        break;
    case 'S': // Select
        switch (currentMenuState)
        {
        case MENU_MAIN:
            switch (menuSelection)
            {
            case 0: // Device Info
                currentMenuState = MENU_DEVICE_INFO;
                menuSelection = 0;
                deviceInfoScroll = 0; // Reset scroll position
                break;
            case 1: // Ethernet Info
                currentMenuState = MENU_ETHERNET_INFO;
                menuSelection = 0;
                ethernetInfoScroll = 0; // Reset scroll position
                break;
            case 2: // Settings
                currentMenuState = MENU_SETTINGS;
                menuSelection = 0;
                settingsMenuScroll = 0;
                break;
            case 3: // Exit
                displayMode = NORMAL;
                currentMenuState = MENU_MAIN;
                menuSelection = 0;
                mainMenuScroll = 0;
                menuTimeoutTimer.STOP; // Stop timeout timer when exiting manually
                break;
            }
            break;
        case MENU_DEVICE_INFO:
            // Back to main menu
            currentMenuState = MENU_MAIN;
            menuSelection = 0;
            deviceInfoScroll = 0; // Reset scroll position
            break;
        case MENU_ETHERNET_INFO:
            // Back to main menu
            currentMenuState = MENU_MAIN;
            menuSelection = 1;
            ethernetInfoScroll = 0; // Reset scroll position
            break;
        case MENU_SETTINGS:
            switch (menuSelection)
            {
            case 0: // Logging Settings
                currentMenuState = MENU_SETTINGS_LOGGING;
                menuSelection = 0;
                break;
            case 1: // Back
                currentMenuState = MENU_MAIN;
                menuSelection = 2;
                break;
            }
            break;
        case MENU_SETTINGS_LOGGING:
            switch (menuSelection)
            {
            case 0: // Toggle Log to File
                appSettings.logToFile = !appSettings.logToFile;
                saveSettings();
                // Update logging manager with new settings
                loggingManager.updateSettings(appSettings.logToFile, appSettings.logToMQTT);
                break;
            case 1: // Toggle Log to MQTT
                appSettings.logToMQTT = !appSettings.logToMQTT;
                saveSettings();
                // Update logging manager with new settings
                loggingManager.updateSettings(appSettings.logToFile, appSettings.logToMQTT);
                break;
            case 2: // Back
                currentMenuState = MENU_SETTINGS;
                menuSelection = 0;
                settingsMenuScroll = 0;
                break;
            }
            break;

        }
        break;
    case 'M': // Menu/Back
        switch (currentMenuState)
        {
        case MENU_MAIN:
            displayMode = NORMAL;
            currentMenuState = MENU_MAIN;
            menuSelection = 0;
            mainMenuScroll = 0;
            menuTimeoutTimer.STOP; // Stop timeout timer when exiting manually
            break;
        case MENU_DEVICE_INFO:
            currentMenuState = MENU_MAIN;
            menuSelection = 0;
            deviceInfoScroll = 0; // Reset scroll position
            break;
        case MENU_ETHERNET_INFO:
            currentMenuState = MENU_MAIN;
            menuSelection = 1;
            ethernetInfoScroll = 0; // Reset scroll position
            break;
        case MENU_SETTINGS:
            currentMenuState = MENU_MAIN;
            menuSelection = 2;
            settingsMenuScroll = 0;
            break;
        case MENU_SETTINGS_LOGGING:
            currentMenuState = MENU_SETTINGS;
            menuSelection = 0;
            break;

        }
        break;
    }
}

void drawMainMenu()
{
    maxMenuItems = 4;

    hwDisplay.setFont(u8g_font_unifont);
    int strWidth = hwDisplay.getStrWidth("MAIN MENU");
    hwDisplay.drawStr((128 - strWidth) / 2, 20, "MAIN MENU");
    hwDisplay.drawHLine(4, 22, 120);

    hwDisplay.setFont(u8g2_font_6x10_tr);

    // Menu items array
    const char *menuItems[] = {
        "Device Info",
        "Ethernet Info",
        "Settings",
        "Exit"};

    // Display 3 visible items with scrolling
    int visibleItems = 3;
    int startItem = mainMenuScroll;
    int endItem = min(startItem + visibleItems, maxMenuItems);

    int yPos = 25;
    int boxHeight = 12;

    for (int i = startItem; i < endItem; i++)
    {
        // Check if this item is selected
        if (i == menuSelection)
        {
            // Selected - draw with background
            hwDisplay.setDrawColor(1);
            hwDisplay.drawBox(8, yPos, 104, boxHeight);
            hwDisplay.setDrawColor(0);
            hwDisplay.drawXBM(12, yPos + 3, 3, 5, bitImage_RightArrow);
            hwDisplay.drawStr(20, yPos + 9, menuItems[i]);
            hwDisplay.setDrawColor(1);
        }
        else
        {
            // Not selected - normal text
            hwDisplay.drawXBM(12, yPos + 3, 3, 5, bitImage_RightArrow);
            hwDisplay.drawStr(20, yPos + 9, menuItems[i]);
        }
        yPos += boxHeight + 1; // Add 1 pixel spacing between items
    }

    // Show scroll indicators if needed
    if (maxMenuItems > visibleItems)
    {
        if (mainMenuScroll > 0)
        {
            hwDisplay.drawXBM(128 - 12, 28, 7, 4, bitImage_UpArrow);
        }
        if ((mainMenuScroll + visibleItems) < maxMenuItems)
        {
            hwDisplay.drawXBM(128 - 12, 56, 7, 4, bitImage_DownArrow);
        }
    }
}

void drawDeviceInfoMenu()
{
    maxMenuItems = 1;

    hwDisplay.setFont(u8g_font_unifont);
    int strWidth = hwDisplay.getStrWidth("DEVICE INFO");
    hwDisplay.drawStr((128 - strWidth) / 2, 20, "DEVICE INFO");
    hwDisplay.drawHLine(4, 22, 120);

    hwDisplay.setFont(u8g2_font_squeezed_b7_tr);

    // Define all device information lines
    String infoLines[10];
    int lineCount = 0;

    // Device friendly ID
    infoLines[lineCount++] = "Device: " + String(DEVICE_FRIENDLY_ID);

    // Firmware Version
    infoLines[lineCount++] = "Firmware: " + String(displayViewModel.getVersion());

    // MAC Address
    infoLines[lineCount++] = "MAC: " + String(displayViewModel.getMacAddress());

    // Serial Number
    infoLines[lineCount++] = "Serial: " + String(displayViewModel.getSerialNumber());

    // Device Status
    DeviceStatus deviceStatus = displayViewModel.getDeviceStatus();
    const char *deviceStatusNames[] = {"RUNNING", "UPDATING", "UPDATE_FAILED"};
    infoLines[lineCount++] = "Status: " + String(deviceStatus < 3 ? deviceStatusNames[deviceStatus] : "UNKNOWN");

    // Additional info could be added here like:
    // - Build date
    // - Chip info
    // - Memory info
    // - Temperature sensors if available

    maxDeviceLines = lineCount;

    // Display lines with scrolling (show 3 lines at a time)
    int visibleLines = 3;
    int startLine = deviceInfoScroll;
    int endLine = min(startLine + visibleLines, lineCount);

    int yPos = 34;
    for (int i = startLine; i < endLine; i++)
    {
        hwDisplay.drawStr(4, yPos, infoLines[i].c_str());
        yPos += 12;
    }

    // Show scroll indicators if needed
    if (lineCount > visibleLines)
    {
        if (deviceInfoScroll > 0)
        {
            hwDisplay.drawXBM(128 - 12, 28, 7, 4, bitImage_UpArrow);
        }
        if ((deviceInfoScroll + visibleLines) < lineCount)
        {
            hwDisplay.drawXBM(128 - 12, 56, 7, 4, bitImage_DownArrow);
        }
    }
}

void drawEthernetInfoMenu()
{
    maxMenuItems = 1;

    hwDisplay.setFont(u8g_font_unifont);
    int strWidth = hwDisplay.getStrWidth("ETHERNET INFO");
    hwDisplay.drawStr((128 - strWidth) / 2, 20, "ETHERNET INFO");
    hwDisplay.drawHLine(4, 22, 120);

    hwDisplay.setFont(u8g2_font_squeezed_b7_tr);

    // Get network status from view model
    NetworkStatus networkStatus = displayViewModel.getNetworkStatus();
    const char *stateNames[] = {"STOPPED", "STARTED", "DISCONNECTED", "LOST_IP", "CONNECTED", "CONNECTED_IP"};

    // Define all information lines
    String infoLines[12];
    int lineCount = 0;

    // Status
    infoLines[lineCount++] = "Status: " + String(networkStatus < 6 ? stateNames[networkStatus] : "UNKNOWN");

    // Link information (always show)
    infoLines[lineCount++] = "Link: " + String(networkingManager.getLinkStatus() ? "UP" : "DOWN");

    if (networkingManager.getLinkStatus())
    {
        // Physical link details
        infoLines[lineCount++] = "Speed: " + networkingManager.getLinkSpeed();
        infoLines[lineCount++] = "Duplex: " + networkingManager.getDuplexMode();
        infoLines[lineCount++] = "Auto-Neg: " + String(networkingManager.getAutoNegotiation() ? "ON" : "OFF");
    }

    // IP configuration (if connected)
    if (networkStatus == NETWORK_CONNECTED_IP)
    {
        infoLines[lineCount++] = "IP: " + networkingManager.getLocalIP();
        infoLines[lineCount++] = "Mask: " + networkingManager.getSubnetMask();
        infoLines[lineCount++] = "Gateway: " + networkingManager.getGatewayIP();
        infoLines[lineCount++] = "DNS: " + networkingManager.getDNSServerIP();
        infoLines[lineCount++] = "MAC: " + networkingManager.getMACAddress();
    }

    maxEthernetLines = lineCount;

    // Display lines with scrolling (show 3 lines at a time)
    int visibleLines = 3;
    int startLine = ethernetInfoScroll;
    int endLine = min(startLine + visibleLines, lineCount);

    int yPos = 34;
    for (int i = startLine; i < endLine; i++)
    {
        hwDisplay.drawStr(4, yPos, infoLines[i].c_str());
        yPos += 12;
    }

    // Show scroll indicators if needed
    if (lineCount > visibleLines)
    {
        if (ethernetInfoScroll > 0)
        {
            hwDisplay.drawXBM(128 - 12, 28, 7, 4, bitImage_UpArrow);
        }
        if ((ethernetInfoScroll + visibleLines) < lineCount)
        {
            hwDisplay.drawXBM(128 - 12, 56, 7, 4, bitImage_DownArrow);
        }

        // Show scroll position
        // char scrollInfo[10];
        // snprintf(scrollInfo, sizeof(scrollInfo), "%d/%d", ethernetInfoScroll + 1, max(1, lineCount - visibleLines + 1));
        // hwDisplay.drawStr(4, 58, "Scroll Page:");
        // hwDisplay.drawStr(90, 58, scrollInfo);
    }
}

void drawSettingsMenu()
{
    maxMenuItems = 2;

    hwDisplay.setFont(u8g_font_unifont);
    int strWidth = hwDisplay.getStrWidth("SETTINGS");
    hwDisplay.drawStr((128 - strWidth) / 2, 20, "SETTINGS");
    hwDisplay.drawHLine(4, 22, 120);

    hwDisplay.setFont(u8g2_font_6x10_tr);

    // Menu items array
    const char *menuItems[] = {
        "Logging Config",
        "Back to Menu"};

    // Display 3 visible items with scrolling
    int visibleItems = 3;
    int startItem = settingsMenuScroll;
    int endItem = min(startItem + visibleItems, maxMenuItems);

    int yPos = 25;
    int boxHeight = 12;

    for (int i = startItem; i < endItem; i++)
    {
        // Check if this is the back item (last item)
        bool isBackItem = (i == maxMenuItems - 1);

        // Check if this item is selected
        if (i == menuSelection)
        {
            // Selected - draw with background
            hwDisplay.setDrawColor(1);
            hwDisplay.drawBox(8, yPos, 104, boxHeight);
            hwDisplay.setDrawColor(0);

            if (isBackItem)
            {
                // Back item with special formatting
                hwDisplay.setFont(u8g2_font_5x7_tr);
                hwDisplay.drawStr(28, yPos + 9, menuItems[i]);
                hwDisplay.setFont(u8g2_font_m2icon_9_tf);
                hwDisplay.drawStr(10, yPos + 9, "\x61");
                hwDisplay.setFont(u8g2_font_6x10_tr);
            }
            else
            {
                // Regular item
                hwDisplay.drawXBM(12, yPos + 3, 3, 5, bitImage_RightArrow);
                hwDisplay.drawStr(20, yPos + 9, menuItems[i]);
            }
            hwDisplay.setDrawColor(1);
        }
        else
        {
            // Not selected - normal text
            if (isBackItem)
            {
                // Back item with special formatting
                hwDisplay.setFont(u8g2_font_5x7_tr);
                hwDisplay.drawStr(28, yPos + 9, menuItems[i]);
                hwDisplay.setFont(u8g2_font_m2icon_9_tf);
                hwDisplay.drawStr(10, yPos + 9, "\x61");
                hwDisplay.setFont(u8g2_font_6x10_tr);
            }
            else
            {
                // Regular item
                hwDisplay.drawXBM(12, yPos + 3, 3, 5, bitImage_RightArrow);
                hwDisplay.drawStr(20, yPos + 9, menuItems[i]);
            }
        }
        yPos += boxHeight + 1; // Add 1 pixel spacing between items
    }

    // Show scroll indicators if needed
    if (maxMenuItems > visibleItems)
    {
        if (settingsMenuScroll > 0)
        {
            hwDisplay.drawXBM(128 - 12, 28, 7, 4, bitImage_UpArrow);
        }
        if ((settingsMenuScroll + visibleItems) < maxMenuItems)
        {
            hwDisplay.drawXBM(128 - 12, 56, 7, 4, bitImage_DownArrow);
        }
    }
}

void drawLoggingSettingsMenu()
{
    maxMenuItems = 3;

    hwDisplay.setFont(u8g_font_unifont);
    int strWidth = hwDisplay.getStrWidth("LOG SETTINGS");
    hwDisplay.drawStr((128 - strWidth) / 2, 20, "LOG SETTINGS");
    hwDisplay.drawHLine(4, 22, 120);

    hwDisplay.setFont(u8g2_font_6x10_tr);

    // Menu items array with settings
    String menuItems[3];
    menuItems[0] = "Log to File: ";
    menuItems[1] = "Log to MQTT: ";
    menuItems[2] = "Back to Settings";

    // Display 3 visible items with scrolling
    int visibleItems = 3;
    int startItem = 0; // No scroll needed for 3 items
    int endItem = maxMenuItems;

    int yPos = 25;
    int boxHeight = 12;

    for (int i = startItem; i < endItem; i++)
    {
        // Check if this is the back item (last item)
        bool isBackItem = (i == maxMenuItems - 1);
        bool isToggleItem = !isBackItem;

        // Check if this item is selected
        if (i == menuSelection)
        {
            // Selected - draw with background
            hwDisplay.setDrawColor(1);
            hwDisplay.drawBox(8, yPos, 104, boxHeight - 2);
            hwDisplay.setDrawColor(0);

            if (isBackItem)
            {
                // Back item with special formatting
                hwDisplay.setFont(u8g2_font_5x7_tr);
                hwDisplay.drawStr(28, yPos + 9, menuItems[i].c_str());
                hwDisplay.setFont(u8g2_font_m2icon_9_tf);
                hwDisplay.drawStr(10, yPos + 9, "\x61");
            }
            else
            {
                // Toggle item
                hwDisplay.drawStr(12, yPos + 9, menuItems[i].c_str());
                hwDisplay.setFont(u8g2_font_m2icon_9_tf);
                if (i == 0)
                {
                    hwDisplay.drawStr(128 - 24 - 2, yPos + 10, appSettings.logToFile ? "\x44" : "\x43");
                }
                else if (i == 1)
                {
                    hwDisplay.drawStr(128 - 24 - 2, yPos + 10, appSettings.logToMQTT ? "\x44" : "\x43");
                }
                hwDisplay.setFont(u8g2_font_6x10_tr);
            }
            hwDisplay.setDrawColor(1);
        }
        else
        {
            // Not selected - normal text
            if (isBackItem)
            {
                // Back item with special formatting
                hwDisplay.setFont(u8g2_font_5x7_tr);
                hwDisplay.drawStr(28, yPos + 9, menuItems[i].c_str());
                hwDisplay.setFont(u8g2_font_m2icon_9_tf);
                hwDisplay.drawStr(10, yPos + 9, "\x61");
                hwDisplay.setFont(u8g2_font_6x10_tr);
            }
            else
            {
                // Toggle item
                hwDisplay.drawStr(12, yPos + 9, menuItems[i].c_str());
                hwDisplay.setFont(u8g2_font_m2icon_9_tf);
                if (i == 0)
                {
                    hwDisplay.drawStr(128 - 24 - 2, yPos + 10, appSettings.logToFile ? "\x44" : "\x43");
                }
                else if (i == 1)
                {
                    hwDisplay.drawStr(128 - 24 - 2, yPos + 10, appSettings.logToMQTT ? "\x44" : "\x43");
                }
                hwDisplay.setFont(u8g2_font_6x10_tr);
            }
        }
        yPos += boxHeight + 1; // Add 1 pixel spacing between items
    }
}



// Settings management functions ----------------------------------------------------------

void loadSettings()
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("app_settings", NVS_READONLY, &nvs_handle);

    if (err == ESP_OK)
    {
        size_t required_size = 0;
        err = nvs_get_blob(nvs_handle, "settings", NULL, &required_size);

        if (err == ESP_OK && required_size == sizeof(AppSettings))
        {
            err = nvs_get_blob(nvs_handle, "settings", &appSettings, &required_size);
            if (err != ESP_OK)
            {
                LOG_WARN(TAG, "Failed to load settings, using defaults");
                // Use default values already initialized
            }
            else
            {
                LOG_INFO(TAG, "Settings loaded successfully");
                // Update logging manager with loaded settings
                loggingManager.updateSettings(appSettings.logToFile, appSettings.logToMQTT);
            }
        }
        else
        {
            LOG_INFO(TAG, "No settings found, using defaults");
            // Use default values already initialized
        }

        nvs_close(nvs_handle);
    }
    else
    {
        LOG_ERROR(TAG, "Failed to open NVS for reading settings");
        // Use default values already initialized
    }
}

void saveSettings()
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("app_settings", NVS_READWRITE, &nvs_handle);

    if (err == ESP_OK)
    {
        err = nvs_set_blob(nvs_handle, "settings", &appSettings, sizeof(AppSettings));
        if (err == ESP_OK)
        {
            err = nvs_commit(nvs_handle);
            if (err == ESP_OK)
            {
                LOG_INFO(TAG, "Settings saved successfully");
            }
            else
            {
                LOG_ERROR(TAG, "Failed to commit settings to NVS");
            }
        }
        else
        {
            LOG_ERROR(TAG, "Failed to save settings to NVS");
        }

        nvs_close(nvs_handle);
    }
    else
    {
        LOG_ERROR(TAG, "Failed to open NVS for writing settings");
    }
}

AppSettings &getAppSettings()
{
    return appSettings;
}

// Menu initialization and management functions

void initializeMenuSystem()
{
    LOG_INFO(TAG, "Initializing new menu system");

    // Create menu manager
    menuManager = new MenuManager(&hwDisplay);
    menuManager->setTimeout(30000); // 30 second timeout
    menuManager->setOnMenuExit([]()
                               {
        displayMode = NORMAL;
        currentMenuState = MENU_MAIN;
        LOG_INFO(TAG, "Menu system exited"); });
    menuManager->setOnMenuUpdate([]()
                                 {
        // Trigger display update when menu state changes
        LOG_DEBUG(TAG, "Menu update requested - triggering display refresh");
        updateDisplayActivity(); });

    // Create all menus
    createMainMenu();
    createDeviceInfoMenu();
    createEthernetInfoMenu();
    // createSettingsMenu();
    // createLoggingSettingsMenu();


    // Set root menu
    menuManager->setRootMenu(mainMenu);

    LOG_INFO(TAG, "Menu system initialized successfully");
}

void createMainMenu()
{
    mainMenu = new Menu("Main Menu");

    mainMenu->addSubmenu("Device Info", [](MenuManager *mgr)
                         { mgr->openSubmenu(deviceInfoMenu); });

    mainMenu->addSubmenu("Ethernet Info", [](MenuManager *mgr)
                         { mgr->openSubmenu(ethernetInfoMenu); });

    // mainMenu->addSubmenu("Settings", [](MenuManager *mgr)
    //                      { mgr->openSubmenu(settingsMenu); });

    mainMenu->addExit();
}

void createDeviceInfoMenu()
{
    deviceInfoMenu = new Menu("Device Info");

    // Add dynamic text items for device information with error handling
    deviceInfoMenu->addDynamicText("Device ID:", []() -> String
                                   {
        try {
            String result = "SERIAL: ";
            result += displayViewModel.getSerialNumber();
            return result;
        } catch (...) {
            String result = "SERIAL: ERROR";
            return result;
        } });

    deviceInfoMenu->addDynamicText("Firmware:", []() -> String
                                   {
        try {
            String result = "VERSION: v";
            result += FIRMWARE_VERSION;
            return result;
        } catch (...) {
            return String("VERSION: ERROR");
        } });

    // deviceInfoMenu->addDynamicText("Free Heap:", []() -> String
    //                                {
    //     try {
    //         uint32_t freeHeap = ESP.getFreeHeap();
    //         String result = "Heap: ";
    //         result += String(freeHeap);
    //         result += " bytes";
    //         return result;
    //     } catch (...) {
    //         return String("Heap: ERROR");
    //     } });

    deviceInfoMenu->addDynamicText("Uptime:", []() -> String
                                   {
        try {
            unsigned long uptime = millis() / 1000;
            unsigned long hours = uptime / 3600;
            unsigned long minutes = (uptime % 3600) / 60;
            unsigned long seconds = uptime % 60;
            String result = "Up: ";
            result += String(hours);
            result += "h ";
            result += String(minutes);
            result += "m ";
            result += String(seconds);
            result += "s";
            return result;
        } catch (...) {
            return String("Up: ERROR");
        } });

    deviceInfoMenu->addBack();
}

void createEthernetInfoMenu()
{
    ethernetInfoMenu = new Menu("Ethernet Info");

    ethernetInfoMenu->addDynamicText("Status:", []() -> String
                                     {
        try {
            NetworkStatus netStatus = displayViewModel.getNetworkStatus();
            String result = "Status: ";
            result += (netStatus == NETWORK_CONNECTED_IP ? "Connected" : "Disconnected");
            return result;
        } catch (...) {
            return String("Status: ERROR");
        } });

    ethernetInfoMenu->addDynamicText("IP Address:", []() -> String
                                     {
        try {
            String result = "IP: ";
            result += networkingManager.getLocalIP();
            return result;
        } catch (...) {
            return String("IP: ERROR");
        } });

    ethernetInfoMenu->addDynamicText("Subnet Mask:", []() -> String
                                     {
        try {
            String result = "Mask: ";
            result += networkingManager.getSubnetMask();
            return result;
        } catch (...) {
            return String("Mask: ERROR");
        } });

    ethernetInfoMenu->addDynamicText("Gateway:", []() -> String
                                     {
        try {
            String result = "GW: ";
            result += networkingManager.getGatewayIP();
            return result;
        } catch (...) {
            return String("GW: ERROR");
        } });

    ethernetInfoMenu->addDynamicText("DNS:", []() -> String
                                     {
        try {
            String result = "DNS: ";
            result += networkingManager.getDNSServerIP();
            return result;
        } catch (...) {
            return String("DNS: ERROR");
        } });

    ethernetInfoMenu->addBack();
}

void createSettingsMenu()
{
    settingsMenu = new Menu("Settings");

    settingsMenu->addSubmenu("Logging Settings", [](MenuManager *mgr)
                             { mgr->openSubmenu(loggingSettingsMenu); });

    settingsMenu->addBack();
}

void createLoggingSettingsMenu()
{
    loggingSettingsMenu = new Menu("Logging Settings");

    // Add toggle items for logging settings
    loggingSettingsMenu->addToggle("Log to File", &appSettings.logToFile);
    loggingSettingsMenu->addToggle("Log to MQTT", &appSettings.logToMQTT);

    loggingSettingsMenu->addAction("Save Settings", [](MenuManager *mgr)
                                   {
        saveSettings();
        loggingManager.updateSettings(appSettings.logToFile, appSettings.logToMQTT);
        LOG_INFO(TAG, "Logging settings saved"); });

    loggingSettingsMenu->addBack();
}



void showMenuSystem()
{
    if (!menuManager)
    {
        initializeMenuSystem();
    }

    displayMode = MENU;
    menuManager->showMenu();
    LOG_INFO(TAG, "Menu system activated");
}

void handleMenuKeyPressNew(char key)
{
    if (menuManager && menuManager->isMenuActive())
    {
        menuManager->handleKeyPress(key);
    }
}

void cleanupMenuSystem()
{
    if (menuManager)
    {
        delete mainMenu;
        delete deviceInfoMenu;
        delete ethernetInfoMenu;
        delete settingsMenu;
        delete loggingSettingsMenu;
        delete menuManager;

        mainMenu = nullptr;
        deviceInfoMenu = nullptr;
        ethernetInfoMenu = nullptr;
        settingsMenu = nullptr;
        loggingSettingsMenu = nullptr;
        menuManager = nullptr;

        LOG_INFO(TAG, "Menu system cleaned up");
    }
}

// Display power management functions
bool updateDisplayActivity()
{
    // Reset both power management timers on any activity
    displayDimTimer.RESET;
    displayOffTimer.RESET;
    bool wasWoken = wakeDisplay(); // Check if display was woken on activity
    LOG_DEBUG(TAG, "Display activity detected - timers reset, display %s", wasWoken ? "woken" : "already awake");
    return wasWoken;
}

bool wakeDisplay()
{
    if (displayOff)
    {
        hwDisplay.setPowerSave(0); // Turn the display back on
        setDisplayBrightness(DISPLAY_NORMAL_CONTRAST);
        displayOff = false;
        displayDimmed = false;
        LOG_DEBUG(TAG, "Display woke up from off state - power restored");
        // Display will be updated in the next scheduled update cycle
        return true; // Display was woken from off state
    }
    else if (displayDimmed)
    {
        setDisplayBrightness(DISPLAY_NORMAL_CONTRAST);
        displayDimmed = false;
        LOG_DEBUG(TAG, "Display woke up from dimmed state - brightness restored");
        // Display will be updated in the next scheduled update cycle
        return true; // Display was woken from dimmed state
    }
    return false; // Display was already awake
}

void setDisplayBrightness(uint8_t contrast)
{
    hwDisplay.setContrast(contrast);
}

void checkDisplayPowerManagement()
{
    static bool timersStarted = false;

    // Initialize timers on first call
    if (!timersStarted)
    {
        displayDimTimer.START_RESET;
        displayOffTimer.START_RESET;
        timersStarted = true;
        LOG_DEBUG(TAG, "Display power management timers started");
    }

    // Check if display should be turned off (60 second timer)
    if (displayOffTimer.TRIGGERED && !displayOff)
    {
        hwDisplay.setPowerSave(1); // Properly turn off the display
        displayOff = true;
        displayDimmed = false; // Reset dimmed state when turning off
        LOG_DEBUG(TAG, "Display turned off after 60s timeout");
        return;
    }

    // Check if display should be dimmed (30 second timer)
    if (displayDimTimer.TRIGGERED && !displayDimmed && !displayOff)
    {
        setDisplayBrightness(DISPLAY_DIM_CONTRAST);
        displayDimmed = true;
        LOG_DEBUG(TAG, "Display dimmed after 30s timeout");
        // Display will be updated in the current cycle after this function returns
        hwDisplay.updateDisplay();
        return;
    }
}
