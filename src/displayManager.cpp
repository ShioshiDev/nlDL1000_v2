#include "displayManager.h"

extern U8G2_SH1106_128X64_NONAME_1_HW_I2C hwDisplay;

extern StatusViewModel displayViewModel;
extern DisplayMode displayMode;

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
		int strWidth = hwDisplay.getStrWidth(displayViewModel.mac);
		hwDisplay.drawStr((128 - strWidth - 4), 34, displayViewModel.mac);
		hwDisplay.drawStr(4, 44, "Serial Number: ");
		strWidth = hwDisplay.getStrWidth(displayViewModel.serial);
		hwDisplay.drawStr((128 - strWidth - 4), 44, displayViewModel.serial);

		if(*displayViewModel.statusNetwork == NETWORK_STARTED)
		{
			hwDisplay.drawXBM(1, 48, 17, 16, bitImage_Cloud);
			hwDisplay.setDrawColor(2);
			hwDisplay.drawLine(7, 57, 12, 57);
			hwDisplay.setDrawColor(1);
			hwDisplay.drawLine(7, 55, 12, 60);
			hwDisplay.drawLine(12, 55, 7, 60);
			if(*displayViewModel.statusDevice == DEVICE_STARTED)
				hwDisplay.drawStr(28, 58, "Network Starting");
		}
		if((*displayViewModel.statusNetwork == NETWORK_NOT_CONNECTED)||(*displayViewModel.statusNetwork == NETWORK_STOPPED))
		{
			hwDisplay.drawXBM(1, 48, 17, 16, bitImage_Cloud);
			hwDisplay.setDrawColor(2);
			hwDisplay.drawLine(7, 57, 12, 57);
			hwDisplay.setDrawColor(1);
			hwDisplay.drawLine(7, 55, 12, 60);
			hwDisplay.drawLine(12, 55, 7, 60);
			if(*displayViewModel.statusDevice == DEVICE_STARTED)
				hwDisplay.drawStr(28, 58, "Not Connected");
		}
		if(*displayViewModel.statusNetwork == NETWORK_CONNECTED)
		{
			hwDisplay.drawXBM(1, 48, 17, 16, bitImage_Cloud);
			hwDisplay.setDrawColor(2);
			hwDisplay.drawLine(7, 57, 12, 57);
			hwDisplay.setDrawColor(1);
			hwDisplay.drawLine(7, 55, 12, 60);
			hwDisplay.drawLine(12, 55, 7, 60);
			if(*displayViewModel.statusDevice == DEVICE_STARTED)
				hwDisplay.drawStr(28, 58, "No Network");
		}
		if(*displayViewModel.statusNetwork == NETWORK_CONNECTED_IP)
		{
			hwDisplay.setBitmapMode(1);
			hwDisplay.drawXBM(1, 48, 17, 16, bitImage_Cloud);
			hwDisplay.setDrawColor(2);
			hwDisplay.drawLine(6, 57, 14, 57);
			hwDisplay.setDrawColor(1);
			hwDisplay.drawEllipse(10, 57, 3, 3);
			if(*displayViewModel.statusDevice == DEVICE_STARTED)
				hwDisplay.drawStr(28, 58, "Network Connected");
		}
		if(*displayViewModel.statusNetwork == NETWORK_CONNECTED_INTERNET)
		{
			hwDisplay.setBitmapMode(1);
			hwDisplay.drawXBM(1, 48, 17, 16, bitImage_CloudSync);
			if(*displayViewModel.statusDevice == DEVICE_STARTED)
				hwDisplay.drawStr(28, 58, "Internet Connected");
		}
		
		if(*displayViewModel.statusDevice >= DEVICE_UPDATING)
		{
			hwDisplay.drawXBM(112, 48, 15, 16, bitImage_FileDownload);
			hwDisplay.drawStr(32, 58, displayViewModel.status);
		}

	} while (hwDisplay.nextPage());
}

void updateDisplayMenu()
{
}
