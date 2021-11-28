
#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00) >> 8) + (((x)&0xFF) << 8))

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
   float temp;
   int rssi;
   int batt;

   void onResult(BLEAdvertisedDevice advertisedDevice)
   {
      if (advertisedDevice.haveManufacturerData() == true)
      {
         if (advertisedDevice.haveName())
         {
            Serial.print("Device name: ");
            Serial.println(advertisedDevice.getName().c_str());
            Serial.println("");
            Serial.printf("compare(NTC):%d \n", advertisedDevice.getName().compare("NTC"));
         }

         std::string strManufacturerData = advertisedDevice.getManufacturerData();
         if (strManufacturerData.length() == 20 && advertisedDevice.haveName() && 0 == advertisedDevice.getName().compare("NTC"))
         {
            uint8_t cManufacturerData[21];
            strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);
            if (cManufacturerData[0] == 0x7b && cManufacturerData[1] == 0x03)
            {
               rssi = advertisedDevice.getRSSI();
               Serial.printf("RSSI: %d \n", rssi);
               batt = cManufacturerData[15];
               Serial.printf("batt: %d \n", batt);
               int tempX32 = (int)((int)((int)cManufacturerData[17]) << 8) | (int)cManufacturerData[16];
               temp = ((float)tempX32) / 32.0;
               Serial.printf("temp: %.2f*C\n", temp);
            }
         }
      }
   }
};
