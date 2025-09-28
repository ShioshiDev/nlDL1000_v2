#include "menuManager.h"
#include "managers/loggingManager.h"
#include <algorithm>

MenuManager::MenuManager(U8G2* display) 
    : display(display), currentMenu(nullptr), rootMenu(nullptr), 
      isActive(false), timeoutStart(0), timeoutDuration(30000), onMenuExit(nullptr), onMenuUpdate(nullptr) 
{
    LOG_DEBUG("MenuManager", "Menu manager initialized");
}

MenuManager::~MenuManager() {
    LOG_DEBUG("MenuManager", "Menu manager destroyed");
}

void MenuManager::setRootMenu(Menu* menu) {
    rootMenu = menu;
    LOG_DEBUG("MenuManager", "Root menu set: %s", menu ? menu->title.c_str() : "null");
}

void MenuManager::showMenu(Menu* menu) {
    if (!menu && !rootMenu) {
        LOG_ERROR("MenuManager", "No menu to show");
        return;
    }
    
    currentMenu = menu ? menu : rootMenu;
    isActive = true;
    resetTimeout();
    resetSelection();
    
    LOG_DEBUG("MenuManager", "Showing menu: %s", currentMenu->title.c_str());
}

void MenuManager::hideMenu() {
    isActive = false;
    currentMenu = nullptr;
    LOG_DEBUG("MenuManager", "Menu hidden");
    
    if (onMenuExit) {
        onMenuExit();
    }
}

void MenuManager::requestRedraw() {
    if (onMenuUpdate) {
        onMenuUpdate();
    }
}

void MenuManager::handleKeyPress(char key) {
    if (!isActive || !currentMenu) return;
    
    resetTimeout();
    
    switch (key) {
        case 'U': // Up
            navigateUp();
            break;
        case 'D': // Down
            navigateDown();
            break;
        case 'L': // Left
            navigateLeft();
            break;
        case 'R': // Right
            navigateRight();
            break;
        case 'S': // Select
            selectItem();
            break;
        case 'M': // Back (if supported)
            goBack();
            break;
        default:
            LOG_DEBUG("MenuManager", "Unhandled key: %c", key);
            break;
    }
}

void MenuManager::navigateUp() {
    if (!currentMenu || currentMenu->items.empty()) return;
    
    // Check if current item is a hex byte being edited
    MenuItem& item = currentMenu->getCurrentItem();
    if (item.type == MENU_ITEM_HEX_BYTE && item.uint8Ptr) {
        // Handle hex nibble increment
        uint8_t currentValue = *item.uint8Ptr;
        uint8_t nibble;
        
        if (item.editPosition == 0) {
            // Editing high nibble (left digit)
            nibble = (currentValue >> 4) & 0x0F;
            nibble = (nibble + 1) & 0x0F; // Increment and wrap at 0xF
            *item.uint8Ptr = (currentValue & 0x0F) | (nibble << 4);
        } else {
            // Editing low nibble (right digit)
            nibble = currentValue & 0x0F;
            nibble = (nibble + 1) & 0x0F; // Increment and wrap at 0xF
            *item.uint8Ptr = (currentValue & 0xF0) | nibble;
        }
        
        LOG_DEBUG("MenuManager", "Hex up - value: 0x%02X, position: %d", *item.uint8Ptr, item.editPosition);
        requestRedraw();
        return;
    }
    
    // Normal menu navigation
    if (currentMenu->currentSelection > 0) {
        currentMenu->currentSelection--;
        updateScrollPosition();
    } else {
        // Wrap to bottom
        currentMenu->currentSelection = currentMenu->items.size() - 1;
        if (currentMenu->useScrolling) {
            // Adjust scroll to show the last item
            int itemCount = currentMenu->items.size();
            if (itemCount > VISIBLE_ITEMS) {
                currentMenu->scrollPosition = itemCount - VISIBLE_ITEMS;
            } else {
                currentMenu->scrollPosition = 0;
            }
        }
    }
    
    LOG_DEBUG("MenuManager", "Navigate up - selection: %d, scroll: %d", 
             currentMenu->currentSelection, currentMenu->scrollPosition);
             
    // Trigger display update after navigation
    requestRedraw();
}

void MenuManager::navigateDown() {
    if (!currentMenu || currentMenu->items.empty()) return;
    
    // Check if current item is a hex byte being edited
    MenuItem& item = currentMenu->getCurrentItem();
    if (item.type == MENU_ITEM_HEX_BYTE && item.uint8Ptr) {
        // Handle hex nibble decrement
        uint8_t currentValue = *item.uint8Ptr;
        uint8_t nibble;
        
        if (item.editPosition == 0) {
            // Editing high nibble (left digit)
            nibble = (currentValue >> 4) & 0x0F;
            nibble = (nibble == 0) ? 0x0F : nibble - 1; // Decrement and wrap at 0
            *item.uint8Ptr = (currentValue & 0x0F) | (nibble << 4);
        } else {
            // Editing low nibble (right digit)
            nibble = currentValue & 0x0F;
            nibble = (nibble == 0) ? 0x0F : nibble - 1; // Decrement and wrap at 0
            *item.uint8Ptr = (currentValue & 0xF0) | nibble;
        }
        
        LOG_DEBUG("MenuManager", "Hex down - value: 0x%02X, position: %d", *item.uint8Ptr, item.editPosition);
        requestRedraw();
        return;
    }
    
    // Normal menu navigation
    if (currentMenu->currentSelection < currentMenu->items.size() - 1) {
        currentMenu->currentSelection++;
        updateScrollPosition();
    } else {
        // Wrap to top
        currentMenu->currentSelection = 0;
        currentMenu->scrollPosition = 0;
    }
    
    LOG_DEBUG("MenuManager", "Navigate down - selection: %d, scroll: %d", 
             currentMenu->currentSelection, currentMenu->scrollPosition);
             
    // Trigger display update after navigation
    requestRedraw();
}

void MenuManager::navigateLeft() {
    if (!currentMenu || currentMenu->items.empty()) return;
    
    MenuItem& item = currentMenu->getCurrentItem();
    
    switch (item.type) {
        case MENU_ITEM_BAUDRATE:
            if (item.uint32Ptr) {
                // Cycle through baudrate values backward
                uint32_t baudrateList[] = {9600, 19200, 38400, 57600, 115200};
                int currentIndex = -1;
                
                // Find current baudrate index
                for (int i = 0; i < 5; i++) {
                    if (*item.uint32Ptr == baudrateList[i]) {
                        currentIndex = i;
                        break;
                    }
                }
                
                // Move to previous baudrate (wrap around)
                if (currentIndex > 0) {
                    *item.uint32Ptr = baudrateList[currentIndex - 1];
                } else {
                    *item.uint32Ptr = baudrateList[4]; // Wrap to 115200
                }
                
                LOG_DEBUG("MenuManager", "Changed %s baudrate to %d", item.text.c_str(), *item.uint32Ptr);
                requestRedraw();
            }
            break;
            
        case MENU_ITEM_HEX_BYTE:
            if (item.uint8Ptr) {
                // Move edit cursor left (between high and low nibbles)
                item.editPosition = (item.editPosition == 0) ? 1 : 0;
                LOG_DEBUG("MenuManager", "Hex edit position: %d", item.editPosition);
                requestRedraw();
            }
            break;
            
        default:
            // For other item types, left does nothing
            break;
    }
}

void MenuManager::navigateRight() {
    if (!currentMenu || currentMenu->items.empty()) return;
    
    MenuItem& item = currentMenu->getCurrentItem();
    
    switch (item.type) {
        case MENU_ITEM_BAUDRATE:
            if (item.uint32Ptr) {
                // Cycle through baudrate values forward
                uint32_t baudrateList[] = {9600, 19200, 38400, 57600, 115200};
                int currentIndex = -1;
                
                // Find current baudrate index
                for (int i = 0; i < 5; i++) {
                    if (*item.uint32Ptr == baudrateList[i]) {
                        currentIndex = i;
                        break;
                    }
                }
                
                // Move to next baudrate (wrap around)
                if (currentIndex >= 0 && currentIndex < 4) {
                    *item.uint32Ptr = baudrateList[currentIndex + 1];
                } else {
                    *item.uint32Ptr = baudrateList[0]; // Wrap to 9600
                }
                
                LOG_DEBUG("MenuManager", "Changed %s baudrate to %d", item.text.c_str(), *item.uint32Ptr);
                requestRedraw();
            }
            break;
            
        case MENU_ITEM_HEX_BYTE:
            if (item.uint8Ptr) {
                // Move edit cursor right (between high and low nibbles)
                item.editPosition = (item.editPosition == 0) ? 1 : 0;
                LOG_DEBUG("MenuManager", "Hex edit position: %d", item.editPosition);
                requestRedraw();
            }
            break;
            
        default:
            // For other item types, right does nothing
            break;
    }
}

void MenuManager::selectItem() {
    if (!currentMenu || currentMenu->items.empty()) return;
    
    MenuItem& item = currentMenu->getCurrentItem();
    LOG_DEBUG("MenuManager", "Selected item: %s (type: %d)", item.text.c_str(), item.type);
    
    // Reset timeout before executing action
    resetTimeout();
    
    switch (item.type) {
        case MENU_ITEM_ACTION:
        case MENU_ITEM_SUBMENU:
            if (item.action) {
                LOG_DEBUG("MenuManager", "Executing action for: %s", item.text.c_str());
                try {
                    item.action(this);
                    LOG_DEBUG("MenuManager", "Action completed for: %s", item.text.c_str());
                } catch (...) {
                    LOG_ERROR("MenuManager", "Exception in action callback for: %s", item.text.c_str());
                }
                requestRedraw();
            }
            break;
            
        case MENU_ITEM_BACK:
            goBack();
            break;
            
        case MENU_ITEM_EXIT:
            exitMenu();
            break;
            
        case MENU_ITEM_TOGGLE:
            if (item.boolPtr) {
                *item.boolPtr = !(*item.boolPtr);
                LOG_DEBUG("MenuManager", "Toggled %s to %s", 
                         item.text.c_str(), *item.boolPtr ? "true" : "false");
                // Trigger display update after toggle change
                requestRedraw();
            }
            break;
            
        case MENU_ITEM_VALUE:
            // For now, just cycle through values (could be enhanced with left/right navigation)
            if (item.data) {
                int* valuePtr = static_cast<int*>(item.data);
                (*valuePtr)++;
                if (*valuePtr > item.maxValue) {
                    *valuePtr = item.minValue;
                }
                LOG_DEBUG("MenuManager", "Changed %s value to %d", item.text.c_str(), *valuePtr);
                // Trigger display update after value change
                requestRedraw();
            }
            break;
            
        case MENU_ITEM_BAUDRATE:
            // Baudrate selection is handled by left/right keys
            // Select does nothing for this item type
            break;
            
        case MENU_ITEM_HEX_BYTE:
            if (item.uint8Ptr) {
                // Up/Down changes the current nibble value
                uint8_t currentValue = *item.uint8Ptr;
                uint8_t nibble;
                
                if (item.editPosition == 0) {
                    // Editing high nibble (left digit)
                    nibble = (currentValue >> 4) & 0x0F;
                    nibble = (nibble + 1) & 0x0F; // Increment and wrap at 0xF
                    *item.uint8Ptr = (currentValue & 0x0F) | (nibble << 4);
                } else {
                    // Editing low nibble (right digit)
                    nibble = currentValue & 0x0F;
                    nibble = (nibble + 1) & 0x0F; // Increment and wrap at 0xF
                    *item.uint8Ptr = (currentValue & 0xF0) | nibble;
                }
                
                LOG_DEBUG("MenuManager", "Changed %s hex value to 0x%02X", item.text.c_str(), *item.uint8Ptr);
                requestRedraw();
            }
            break;
            
        case MENU_ITEM_TEXT:
            // Text items don't do anything on select
            break;
    }
}

void MenuManager::goBack() {
    if (currentMenu && currentMenu->parent) {
        returnToParent();
    } else {
        exitMenu();
    }
}

void MenuManager::exitMenu() {
    LOG_DEBUG("MenuManager", "Exiting menu system");
    hideMenu();
}

void MenuManager::openSubmenu(Menu* submenu) {
    if (!submenu) {
        LOG_ERROR("MenuManager", "Attempted to open null submenu");
        return;
    }
    
    if (!isActive) {
        LOG_ERROR("MenuManager", "Attempted to open submenu when menu system is inactive");
        return;
    }
    
    // Validate submenu has items
    if (submenu->items.empty()) {
        LOG_ERROR("MenuManager", "Attempted to open empty submenu: %s", submenu->title.c_str());
        return;
    }
    
    LOG_DEBUG("MenuManager", "Opening submenu: %s with %d items", submenu->title.c_str(), submenu->items.size());
    
    submenu->parent = currentMenu;
    currentMenu = submenu;
    resetSelection();
    resetTimeout();
    
    LOG_DEBUG("MenuManager", "Successfully opened submenu: %s", submenu->title.c_str());
    
    // Trigger display update after menu state change
    if (onMenuUpdate) {
        LOG_DEBUG("MenuManager", "Triggering menu update callback");
    }
    requestRedraw();
}

void MenuManager::returnToParent() {
    if (currentMenu && currentMenu->parent) {
        Menu* parent = currentMenu->parent;
        currentMenu = parent;
        LOG_DEBUG("MenuManager", "Returned to parent menu: %s", parent->title.c_str());
        resetTimeout();
        
        // Trigger display update after menu navigation
        if (onMenuUpdate) {
            LOG_DEBUG("MenuManager", "Triggering menu update callback for parent return");
        }
        requestRedraw();
    }
}

void MenuManager::draw() {
    if (!isActive || !currentMenu || !display) return;
    
    // Check for custom draw function
    if (currentMenu->customDraw) {
        currentMenu->customDraw(display, this);
        return;
    }
    
    // Note: Don't call clearBuffer() here as it conflicts with firstPage/nextPage mode
    // The display manager handles the page clearing automatically
    
    // Standard menu drawing
    display->setFont(u8g2_font_6x10_tf);
    
    // Draw title (centered)
    int titleWidth = display->getStrWidth(currentMenu->title.c_str());
    int titleX = (128 - titleWidth) / 2;
    display->drawStr(titleX, 12, currentMenu->title.c_str());
    
    // Draw separator line below title
    display->drawLine(0, 15, 127, 15);
    
    // Draw menu items with proper positioning from y=16 to y=64 (3 slots of 16 pixels each)
    int itemCount = currentMenu->items.size();
    int startIndex = currentMenu->useScrolling ? currentMenu->scrollPosition : 0;
    int endIndex = currentMenu->useScrolling ? 
                   std::min(startIndex + VISIBLE_ITEMS, itemCount) : itemCount;
    
    // Draw 3 equally spaced item slots
    for (int i = startIndex; i < endIndex; i++) {
        int displayIndex = i - startIndex;
        int yPos = MENU_START_Y + (displayIndex * ITEM_HEIGHT) + 12; // +12 for text baseline
        bool isSelected = (i == currentMenu->currentSelection);
        
        drawMenuItem(i, yPos, isSelected);
    }
    
    // Draw scroll indicators if needed (positioned on the right side)
    if (currentMenu->useScrolling && itemCount > VISIBLE_ITEMS) {
        // Up arrow
        if (currentMenu->scrollPosition > 0) {
            display->drawTriangle(120, 20, 124, 16, 128, 20);
        }
        
        // Down arrow  
        if (currentMenu->scrollPosition + VISIBLE_ITEMS < itemCount) {
            display->drawTriangle(120, 58, 124, 62, 128, 58);
        }
    }
}

void MenuManager::drawMenuItem(int index, int yPosition, bool isSelected) {
    if (!currentMenu || !display || index >= currentMenu->items.size()) {
        return;
    }

    const MenuItem& item = currentMenu->items[index];
    String baseText = getMenuItemText(item);

    if (isSelected) {
        // Draw selection background box for the entire item slot
        display->setDrawColor(1);
        display->drawBox(0, yPosition - 11, MAX_ITEM_WIDTH, 14);
        display->setDrawColor(0);
    }

    // Determine indicator text (if any) and reserve space accordingly
    String indicatorText;
    switch (item.type) {
        case MENU_ITEM_SUBMENU:
            indicatorText = ">";
            break;
        case MENU_ITEM_TOGGLE:
            if (item.boolPtr) {
                indicatorText = *item.boolPtr ? "ON" : "OFF";
            }
            break;
        case MENU_ITEM_VALUE:
            if (item.data) {
                indicatorText = String(*(static_cast<int*>(item.data)));
            }
            break;
        case MENU_ITEM_BAUDRATE:
            if (item.uint32Ptr) {
                indicatorText = String(*item.uint32Ptr);
            }
            break;
        case MENU_ITEM_HEX_BYTE:
            if (item.uint8Ptr) {
                char hexStr[6]; // "0xXX\0"
                sprintf(hexStr, "0x%02X", *item.uint8Ptr);
                indicatorText = String(hexStr);
                
                // Add cursor indicator for hex editing
                if (isSelected) {
                    // Add visual indicator for which nibble is being edited
                    if (item.editPosition == 0) {
                        // Editing high nibble - add underscore under first hex digit
                        indicatorText += " _";
                    } else {
                        // Editing low nibble - add underscore under second hex digit  
                        indicatorText += "  _";
                    }
                }
            }
            break;
        default:
            break;
    }

    const int paddingLeft = 4;
    const int paddingRight = 4;
    int indicatorWidth = 0;
    if (indicatorText.length() > 0) {
        indicatorWidth = display->getStrWidth(indicatorText.c_str());
    }

    int textAreaWidth = MAX_ITEM_WIDTH - paddingLeft - paddingRight - indicatorWidth;
    if (indicatorWidth > 0) {
        textAreaWidth -= 2; // maintain gap between text and indicator
    }
    if (textAreaWidth < 0) {
        textAreaWidth = 0;
    }

    String displayText = truncateTextToWidth(baseText, textAreaWidth);
    display->drawStr(paddingLeft, yPosition, displayText.c_str());

    if (indicatorText.length() > 0) {
        int indicatorX = MAX_ITEM_WIDTH - indicatorWidth - paddingRight;
        display->drawStr(indicatorX, yPosition, indicatorText.c_str());
    }

    if (isSelected) {
        display->setDrawColor(1);
    }
}

String MenuManager::getMenuItemText(const MenuItem& item) {
    if (item.dynamicText) {
        return item.dynamicText();
    }
    return item.text;
}

String MenuManager::truncateTextToWidth(const String& text, int maxWidth) const {
    if (!display || maxWidth <= 0) {
        return maxWidth <= 0 ? String("") : text;
    }

    if (display->getStrWidth(text.c_str()) <= maxWidth) {
        return text;
    }

    const char* ellipsis = "...";
    int ellipsisWidth = display->getStrWidth(ellipsis);
    int targetWidth = maxWidth - ellipsisWidth;
    if (targetWidth <= 0) {
        return String(ellipsis);
    }

    String working = text;
    while (working.length() > 0) {
        working.remove(working.length() - 1);
        if (display->getStrWidth(working.c_str()) <= targetWidth) {
            working += ellipsis;
            return working;
        }
    }

    return String(ellipsis);
}

void MenuManager::updateScrollPosition() {
    if (!currentMenu || !currentMenu->useScrolling) return;
    
    int selection = currentMenu->currentSelection;
    int scroll = currentMenu->scrollPosition;
    
    // Adjust scroll position to keep selected item visible
    if (selection < scroll) {
        currentMenu->scrollPosition = selection;
    } else if (selection >= scroll + VISIBLE_ITEMS) {
        currentMenu->scrollPosition = selection - VISIBLE_ITEMS + 1;
    }
}

void MenuManager::resetTimeout() {
    timeoutStart = millis();
}

void MenuManager::checkTimeout() {
    if (isActive && timeoutDuration > 0 && 
        (millis() - timeoutStart) >= timeoutDuration) {
        LOG_DEBUG("MenuManager", "Menu timeout reached");
        exitMenu();
    }
}
