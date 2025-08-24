#include "displayManager.h"

extern U8G2_SH1106_128X64_NONAME_1_HW_I2C hwDisplay;

extern StatusViewModel displayViewModel;
extern DisplayMode displayMode;
extern uint8_t factoryResetSelection;

extern QueueHandle_t displayQueueHandle;
extern SemaphoreHandle_t i2cMutex;
extern SemaphoreHandle_t displayModelMutex;

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
	if (xSemaphoreTake(i2cMutex, DISPLAY_UPDATE_INTERVAL) == pdTRUE)
	{
		if (xSemaphoreTake(displayModelMutex, DISPLAY_UPDATE_INTERVAL) == pdTRUE)
		{
			if (displayMode == NORMAL)
			{
				updateDisplayNormal();
			}
			else if (displayMode == MENU)
			{
				updateDisplayMenu();
			}
			else if (displayMode == FACTORY_RESET_CONFIRM)
			{
				updateDisplayFactoryResetConfirm();
			}
			xSemaphoreGive(displayModelMutex);
		}
		else
		{
			Serial.println("updateDisplay - Failed to take i2cMutex");
		}
		xSemaphoreGive(i2cMutex);
	}
	else
	{
		Serial.println("updateDisplay - Failed to take i2cMutex");
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
		hwDisplay.drawStr(4, 34, "MAC: ");
		int strWidth = hwDisplay.getStrWidth(displayViewModel.getMacAddress());
		hwDisplay.drawStr((128 - strWidth - 4), 34, displayViewModel.getMacAddress());
		hwDisplay.drawStr(4, 44, "Serial Number: ");
		strWidth = hwDisplay.getStrWidth(displayViewModel.getSerialNumber());
		hwDisplay.drawStr((128 - strWidth - 4), 44, displayViewModel.getSerialNumber());

		DeviceStatus deviceStatus = displayViewModel.getDeviceStatus();
		NetworkStatus networkStatus = displayViewModel.getNetworkStatus();
		ConnectivityStatus connectivityStatus = displayViewModel.getConnectivityStatus();
		ServicesStatus servicesStatus = displayViewModel.getServicesStatus();

		if (networkStatus == NETWORK_STARTED)
		{
			hwDisplay.drawXBM(1, 48, 17, 16, bitImage_Cloud);
			hwDisplay.setDrawColor(2);
			hwDisplay.drawLine(7, 57, 12, 57);
			hwDisplay.setDrawColor(1);
			hwDisplay.drawLine(7, 55, 12, 60);
			hwDisplay.drawLine(12, 55, 7, 60);
			if (deviceStatus == DEVICE_STARTED)
				hwDisplay.drawStr(28, 58, "Network Ready...");
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
				hwDisplay.drawStr(28, 58, "Not Connected");
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
				hwDisplay.drawStr(28, 58, "Connecting...");
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
				hwDisplay.drawStr(28, 58, "Network Connected");
		}
		if ((networkStatus == NETWORK_CONNECTED_IP) && (connectivityStatus == CONNECTIVITY_ONLINE))
		{
			hwDisplay.setBitmapMode(1);
			hwDisplay.drawXBM(1, 48, 17, 16, bitImage_CloudSync);
			if (deviceStatus == DEVICE_STARTED)
			{
				if (servicesStatus == SERVICES_CONNECTED)
					hwDisplay.drawStr(28, 58, "Services Connected");
				else
					hwDisplay.drawStr(28, 58, "Internet Connected");
			}
		}

		if (deviceStatus >= DEVICE_UPDATING)
		{
			hwDisplay.drawXBM(112, 48, 15, 16, bitImage_FileDownload);
			hwDisplay.drawStr(32, 58, displayViewModel.getStatusString());
		}

	} while (hwDisplay.nextPage());
}

void updateDisplayMenu()
{
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
