#pragma once
#ifndef __MENUMANAGER_H__
#define __MENUMANAGER_H__

#include <Arduino.h>
#include <functional>
#include <vector>
#include <algorithm>
#include <U8g2lib.h>
#include "definitions.h"

// Forward declarations
class MenuManager;
typedef std::function<void(MenuManager*)> MenuActionCallback;
typedef std::function<void(U8G2*, MenuManager*)> MenuDrawCallback;
typedef std::function<String()> MenuTextCallback;

// Menu item types
enum MenuItemType {
    MENU_ITEM_ACTION,     // Executes an action
    MENU_ITEM_SUBMENU,    // Opens a submenu
    MENU_ITEM_BACK,       // Goes back to parent menu
    MENU_ITEM_TOGGLE,     // Boolean toggle
    MENU_ITEM_VALUE,      // Numeric value
    MENU_ITEM_TEXT,       // Text display only
    MENU_ITEM_EXIT,       // Exits menu system
    MENU_ITEM_BAUDRATE,   // Baudrate selection from predefined list
    MENU_ITEM_HEX_BYTE    // Hex byte value (0x00-0xFF) with digit editing
};

// Menu item structure
struct MenuItem {
    String text;
    MenuItemType type;
    MenuActionCallback action;      // Action to perform on select
    MenuTextCallback dynamicText;   // Dynamic text generator
    void* data;                     // Additional data pointer
    int value;                      // For numeric values
    int minValue;                   // Min value for numeric items
    int maxValue;                   // Max value for numeric items
    bool* boolPtr;                  // Pointer to bool for toggle items
    
    // Extended fields for special menu items
    uint32_t* uint32Ptr;            // Pointer to uint32_t for baudrate
    uint8_t* uint8Ptr;              // Pointer to uint8_t for hex byte
    int editPosition;               // Current edit position (for hex editing)
    
    MenuItem(const String& text, MenuItemType type = MENU_ITEM_ACTION) 
        : text(text), type(type), action(nullptr), dynamicText(nullptr), 
          data(nullptr), value(0), minValue(0), maxValue(100), boolPtr(nullptr),
          uint32Ptr(nullptr), uint8Ptr(nullptr), editPosition(0) {}
};

// Menu structure
class Menu {
public:
    String title;
    std::vector<MenuItem> items;
    MenuDrawCallback customDraw;    // Optional custom draw function
    Menu* parent;                   // Parent menu for navigation
    int currentSelection;
    int scrollPosition;
    bool useScrolling;              // Enable 3-item scrolling window
    
    Menu(const String& title, Menu* parent = nullptr) 
        : title(title), parent(parent), currentSelection(0), 
          scrollPosition(0), useScrolling(true), customDraw(nullptr) {}
    
    void addItem(const MenuItem& item) {
        items.push_back(item);
    }
    
    void addAction(const String& text, MenuActionCallback action) {
        MenuItem item(text, MENU_ITEM_ACTION);
        item.action = action;
        items.push_back(item);
    }
    
    void addSubmenu(const String& text, MenuActionCallback action) {
        MenuItem item(text, MENU_ITEM_SUBMENU);
        item.action = action;
        items.push_back(item);
    }
    
    void addToggle(const String& text, bool* boolPtr) {
        MenuItem item(text, MENU_ITEM_TOGGLE);
        item.boolPtr = boolPtr;
        items.push_back(item);
    }
    
    void addValue(const String& text, int* valuePtr, int minVal, int maxVal) {
        MenuItem item(text, MENU_ITEM_VALUE);
        item.data = valuePtr;
        item.minValue = minVal;
        item.maxValue = maxVal;
        items.push_back(item);
    }
    
    void addBaudrate(const String& text, uint32_t* baudratePtr) {
        MenuItem item(text, MENU_ITEM_BAUDRATE);
        item.uint32Ptr = baudratePtr;
        items.push_back(item);
    }
    
    void addHexByte(const String& text, uint8_t* hexPtr) {
        MenuItem item(text, MENU_ITEM_HEX_BYTE);
        item.uint8Ptr = hexPtr;
        item.editPosition = 0; // Start at first hex digit
        items.push_back(item);
    }
    
    void addDynamicText(const String& text, MenuTextCallback textCallback) {
        MenuItem item(text, MENU_ITEM_TEXT);
        item.dynamicText = textCallback;
        items.push_back(item);
    }
    
    void addBack() {
        MenuItem item("< Back", MENU_ITEM_BACK);
        items.push_back(item);
    }
    
    void addExit() {
        MenuItem item("Exit", MENU_ITEM_EXIT);
        items.push_back(item);
    }
    
    int getItemCount() const {
        return items.size();
    }
    
    MenuItem& getCurrentItem() {
        if (currentSelection >= 0 && currentSelection < items.size()) {
            return items[currentSelection];
        }
        static MenuItem dummy("", MENU_ITEM_TEXT);
        return dummy;
    }
};

// Main MenuManager class
class MenuManager {
private:
    U8G2* display;
    Menu* currentMenu;
    Menu* rootMenu;
    bool isActive;
    unsigned long timeoutStart;
    unsigned long timeoutDuration;
    std::function<void()> onMenuExit;
    std::function<void()> onMenuUpdate;  // Callback for when menu needs to be redrawn
    
    // Display constants - 48 pixels available (y=16 to y=64), 3 items = 16 pixels each
    static const int VISIBLE_ITEMS = 3;
    static const int ITEM_HEIGHT = 16;   // 48 pixels / 3 items = 16 pixels per item
    static const int MENU_START_Y = 16;  // Start immediately after title separator
    static const int MAX_ITEM_WIDTH = 118; // 2 pixels short of scroll area (120)

    void requestRedraw();
    String truncateTextToWidth(const String& text, int maxWidth) const;
    
public:
    MenuManager(U8G2* display);
    ~MenuManager();
    
    // Menu management
    void setRootMenu(Menu* menu);
    void showMenu(Menu* menu = nullptr);
    void hideMenu();
    bool isMenuActive() const { return isActive; }
    
    // Navigation
    void handleKeyPress(char key);
    void navigateUp();
    void navigateDown();
    void navigateLeft();
    void navigateRight();
    void selectItem();
    void goBack();
    void exitMenu();
    
    // Menu operations
    void openSubmenu(Menu* submenu);
    void returnToParent();
    
    // Drawing
    void draw();
    void drawMenuItem(int index, int yPosition, bool isSelected);
    String getMenuItemText(const MenuItem& item);
    
    // Timeout management
    void setTimeout(unsigned long duration) { timeoutDuration = duration; }
    void resetTimeout();
    void checkTimeout();
    void setOnMenuExit(std::function<void()> callback) { onMenuExit = callback; }
    void setOnMenuUpdate(std::function<void()> callback) { onMenuUpdate = callback; }
    
    // Getters
    Menu* getCurrentMenu() const { return currentMenu; }
    Menu* getRootMenu() const { return rootMenu; }
    
    // Utility methods for menu builders
    void updateScrollPosition();
    void resetSelection() { 
        if (currentMenu) {
            currentMenu->currentSelection = 0;
            currentMenu->scrollPosition = 0;
        }
    }
};

#endif // __MENUMANAGER_H__
