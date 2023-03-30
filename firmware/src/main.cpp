#include <Arduino.h>
#include <AsyncElegantOTA.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include "QuickPID.h"
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <Arduino_Json.h>
#include <ESPAsync_WiFiManager.h>
#include <ESP32Time.h>

#include "MyAdvertisedDeviceCallbacks.cpp"
#include "credentials.h"

#define SELF_AP_SSID "rika"
#define SELF_URL "rika" // .local
#define PRESSKEY_RELEASE LOW
#define PRESSKEY_PRESS HIGH
#define PRESS_TIME_MS 500
#define PRESS_INTERVAL_DELAY_MS 2000
#define HISTORY_INTERVAL_DELAY_MS 1000
#define OLED_INTERVAL_DELAY_MS 500
#define PAGE_BUFFER 600
#define GET_CALENDAR_DELAY_MS (10 * 1000)
const String urlFrontend = CREDENTIAL_FRONTEND_URL;
const String urlBackend = CREDENTIAL_BACKEND_URL;

static bool ON_ALLOWED = true;
static bool OFF_ALLOWED = true;
static bool udp_enabled = true;
static uint8_t tempmode = 0;
static const uint8_t PRESSKEY_ONOFF = 18;
static const uint8_t PRESSKEY_MINUS = 17;
static const uint8_t PRESSKEY_PLUS = 4;
static const uint8_t PIN_IN_IS_POWERED = 21;
static const uint8_t PIN_IN_COM_NOT = 15;
static const uint8_t PIN_IN_POWER_PRESSED_NOT = 2;

const char *ssid = CREDENTIALS_SSID;
const char *password = CREDENTIALS_PWD;

/****************************************************************************************************************************
 *****************************************************************************************************************************/
/* --- UTIL --- */
#define uint64_toString(number) String((unsigned long)(((number)&0xFFFF000000000000) >> 48), HEX) + String((unsigned long)(((number)&0xFFFF00000000) >> 32), HEX) + String((unsigned long)(((number)&0xFFFF0000) >> 16), HEX) + String((unsigned long)(((number)&0x0000FFFF)), HEX)
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#if !(defined(ESP32))
#error This code is intended to run on the ESP32 platform! Please check your Tools->Board setting.
#endif
AsyncWebServer webServer(80);
AsyncDNSServer  dnsServer;

// --- RTC ---
static ESP32Time rtc;
static bool rtcTimeIsValid = false;

// --- AHT Sensor---
Adafruit_AHTX0 aht;
float humidity = 0, temp = 0;
#define AHT_INTERVAL_MS (500)
#define AHT_AVERAGING_SAMPLE_COUNT (50)

// --- OLED  ---
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL, SDA);
char bufferL1[40] = {0};
char bufferL2[40] = {0};
char bufferL3[40] = {0};
char bufferL4[40] = {0};
char bufferPID[40] = {0};
char bufferUptime[40] = {0};

// --- PID  ---
const uint32_t sampleTimeUs = 1000000; // 1000ms
const int outputMax = 100;
const int outputMin = 0;
bool printOrPlotter = 1; // on(1) monitor, off(0) plotter
float POn = 1.0;         // proportional on Error to Measurement ratio (0.0-1.0), default = 1.0
float DOn = 0.0;         // derivative on Error to Measurement ratio (0.0-1.0), default = 0.0
float outputStep = 1;
float hysteresis = 1;
float Input = 20, Output = 50, Setpoint = 18;
float Kp = 1, Ki = 0, Kd = 0;
bool pidLoop = true; // false -> autotune at NowTemp + 2C.
QuickPID _myPID = QuickPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, POn, DOn, QuickPID::DIRECT);

// --- GRAPH  ---
const uint16_t historyCnt = 5 * 60;
uint16_t history[historyCnt];
uint16_t historyIdx = 0;

const int wantLvlMax = 20;
int wantLvl = 10; // 0-wantLvlMax
int nowLvl = 0;   // 0-wantLvlMax
int cursor_align = 0;
bool cursor_align_done = 0;
typedef enum
{
   START, // ST 21 (mn) (17:51)
   ON,
   OFF,
   EXIT, // EX 60 (sec) // CL 110 (sec) // Ex 1200~999~0 (sec) //
} rikaStatus_t;
rikaStatus_t rikaStatus = ON;
const String rikaStatus_toString[] = {"START", "ON", "OFF", "EXIT"};
static unsigned long swBtnPressMillis = 0;  // millis last btn press by software
static unsigned long swBtnPressEpoch = 0;   // epoch last btn press by software
static unsigned long userBtnPressEpoch = 0; // epoch last btn press by user
static unsigned long rikaStartEpoch = 0;
static unsigned long rikaCleaningEpoch = 0; // epoch last cleaning message seen // CL 110 (sec)
typedef enum
{
   NONE = 0,
   POWER = 1,
   MINUS = 2,
   PLUS = 3,
   MENU = 4,
   ENTER = 5,
} rikaBtn_t;
const String rikaBtn_toString[] = {"_", "Power", "Minus", "Plus", "Menu", "Enter"};
static rikaBtn_t userBtnLast = NONE;
static rikaBtn_t swBtnLast = NONE;
typedef enum
{
   MSG_RELEASED = 'C',
   MSG_MINUS = 'F',
   MSG_PLUS = 'G',
   MSG_MENU = 'H',
   MSG_ENTER = 'I',
   MSG_CL = 'M',
   MSG_EX = 'N',
} RikaMsg_t;

// --- CALENDAR ---
unsigned long last_calendar_time = 0;
String json_array;

// --- BLE SCAN ---
#define FEA_BLE_SCAN (0)
#if FEA_BLE_SCAN
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
int scanTime = 3; // In seconds
BLEScan *pBLEScan;
#define BLESCAN_INTERVAL_MS (200)
#endif

// --- WIFI UDP ---
#define FEA_SERIAL2UDP (1)
#if FEA_SERIAL2UDP
#include <WiFiUdp.h>
WiFiUDP Udp;
unsigned int localUdpPort = 5300;
#endif

// wifi manager
   ESPAsync_WiFiManager ESPAsync_wifiManager(&webServer, &dnsServer, "Async_AutoConnect");

// --- HTML ---
void svgGraph(AsyncWebServerRequest *request);
void htmlRoot(AsyncWebServerRequest *request);
void htmlConfigWifi(AsyncWebServerRequest *request);
void handlePost(AsyncWebServerRequest *request);
bool IO_isPowerDown(void);
bool IO_isPowerBtnPressed(void);

float avg(int inputVal)
{
   static int arrDat[AHT_AVERAGING_SAMPLE_COUNT];
   static int pos;
   static long sum;
   static int initCount;
   pos++;
   if (pos >= AHT_AVERAGING_SAMPLE_COUNT)
      pos = 0;
   if (initCount < AHT_AVERAGING_SAMPLE_COUNT)
      initCount++;
   sum = sum - arrDat[pos] + inputVal;
   arrDat[pos] = inputVal;
   return (float)sum / (float)initCount;
}

String GET_Request(const char *server)
{
   HTTPClient http;
   http.begin(server);
   int httpResponseCode = http.GET();
   String payload = "{}";
   if (httpResponseCode > 0)
   {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      payload = http.getString();
   }
   else
   {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
   }
   http.end();
   return payload;
}

String POST_Request(const char *server, const String json)
{
   HTTPClient http;
   http.begin(server);
   http.addHeader("Content-Type", "application/json");
   int httpResponseCode = http.POST(json);
   String payload = "{}";
   if (httpResponseCode > 0)
   {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      payload = http.getString();
   }
   else
   {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
   }
   http.end();
   return payload;
}

void run_getCallendar(void)
{
   if ((millis() - last_calendar_time) > GET_CALENDAR_DELAY_MS)
   {
      if (WiFi.status() == WL_CONNECTED)
      {
         JSONVar myArray;
         myArray["mac"] = uint64_toString(ESP.getEfuseMac());
         myArray["l1"] = bufferL1;
         myArray["l2"] = bufferL2;
         myArray["l3"] = bufferL3;
         myArray["l4"] = bufferL4;
         myArray["l5"] = bufferPID;
         myArray["l6"] = bufferUptime;
         myArray["kp"] = _myPID.GetKp();
         myArray["ki"] = _myPID.GetKi();
         myArray["kd"] = _myPID.GetKd();
         myArray["pterm"] = _myPID.GetPterm();
         myArray["iterm"] = _myPID.GetIterm();
         myArray["dterm"] = _myPID.GetDterm();
         myArray["settemp"] = Setpoint;
         myArray["nowtemp"] = Input;
         myArray["nowpercent"] = wantLvl * 5;
         myArray["rikastatus"] = rikaStatus_toString[rikaStatus];
         myArray["rikastart"] = rikaStartEpoch;
         myArray["rikacleaning"] = rikaCleaningEpoch;
         myArray["userbtnpress"] = userBtnPressEpoch;
         myArray["userbtnlast"] = rikaBtn_toString[userBtnLast];
         myArray["swbtnpress"] = swBtnPressEpoch;
         myArray["swbtnlast"] = rikaBtn_toString[swBtnLast];
         myArray["onallowedstatus"]  = ON_ALLOWED  ? "true" : "false";
         myArray["offallowedstatus"] = OFF_ALLOWED ? "true" : "false";
         myArray["udp_enabled"] = udp_enabled ? "true" : "false";
         myArray["tempmode"] = tempmode;
         String jsonStringOled = JSON.stringify(myArray);

         json_array = POST_Request(urlBackend.c_str(), jsonStringOled);
         Serial.println(json_array);
         JSONVar my_obj = JSON.parse(json_array);

         if (JSON.typeof(my_obj) == "undefined")
         {
            Serial.println("Parsing input failed!");
            return;
         }

         if (pidLoop)
         {
            Serial.print("JSON object = ");
            Serial.println(my_obj);
            if (1)
            {
               Serial.print("Setpoint: ");
               String jsonString = JSON.stringify(my_obj[CFG_ROOM_NAME]["setpoint"]);
               Serial.println(jsonString.toFloat());
               if (jsonString.toFloat() > 0)
               {
                  Setpoint = jsonString.toFloat();
               }
            }
            if (1)
            {
               Serial.print("Kp: ");
               String jsonString = JSON.stringify(my_obj[CFG_ROOM_NAME]["kp"]);
               Serial.println(jsonString.toFloat());
               if (jsonString.toFloat() >= 0)
               {
                  Kp = jsonString.toFloat();
               }
            }
            if (1)
            {
               Serial.print("Ki: ");
               String jsonString = JSON.stringify(my_obj[CFG_ROOM_NAME]["ki"]);
               Serial.println(jsonString.toFloat());
               if (jsonString.toFloat() >= 0)
               {
                  Ki = jsonString.toFloat();
               }
            }
            if (1)
            {
               Serial.print("Kd: ");
               String jsonString = JSON.stringify(my_obj[CFG_ROOM_NAME]["kd"]);
               Serial.println(jsonString.toFloat());
               if (jsonString.toFloat() >= 0)
               {
                  Kd = jsonString.toFloat();
               }
            }
            if (1)
            {
               Serial.print("ON_ALLOWED: ");
               String jsonString = JSON.stringify(my_obj[CFG_ROOM_NAME]["onallowed"]);
               ON_ALLOWED = jsonString.toInt() == 0 ? false : true;
               Serial.println(ON_ALLOWED);
            }
            if (1)
            {
               Serial.print("OFF_ALLOWED: ");
               String jsonString = JSON.stringify(my_obj[CFG_ROOM_NAME]["offallowed"]);
               OFF_ALLOWED = jsonString.toInt() == 0 ? false : true;
               Serial.println(OFF_ALLOWED);
            }
            if (1)
            {
               Serial.print("udp_enabled: ");
               String jsonString = JSON.stringify(my_obj[CFG_ROOM_NAME]["udp_enabled"]);
               udp_enabled = jsonString.toInt() == 0 ? false : true;
               Serial.println(udp_enabled);
            }
            if (1)
            {
               Serial.print("tempmode: ");
               String jsonString = JSON.stringify(my_obj[CFG_ROOM_NAME]["manual"]);
               tempmode = jsonString.toInt();
               Serial.println(tempmode);
            }
            _myPID.SetTunings(Kp, Ki, Kd);
         }
         if (1)
         {
            Serial.print("timestamp: ");
            String jsonString = JSON.stringify(my_obj["timestamp"]);
            Serial.println(jsonString.toInt());
            if (jsonString.toInt() >= 0 && !rtcTimeIsValid)
            {
               rtc.setTime(jsonString.toInt());
               rtcTimeIsValid = true;
            }
         }
      }
      else
      {
         Serial.println("WiFi Disconnected");
      }
      last_calendar_time = millis();
   }
}

void run_aht(void)
{
   static unsigned long runAhtTime = 0;
   if (millis() - runAhtTime >= AHT_INTERVAL_MS)
   {
      runAhtTime = millis();
      sensors_event_t aht_humidity, aht_temp;
      aht.getEvent(&aht_humidity, &aht_temp);
      humidity = aht_humidity.relative_humidity;
      temp = avg(aht_temp.temperature * 100) / 100;
   }
}

void setup_ble_scan(void)
{
#if FEA_BLE_SCAN
   BLEDevice::init("");
   pBLEScan = BLEDevice::getScan(); // create new scan
   pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
   pBLEScan->setActiveScan(true); // active scan uses more power, but get results faster
   pBLEScan->setInterval(2500);
   pBLEScan->setWindow(2100); // less or equal setInterval value
#endif
}

void run_ble_scan(void)
{
#if FEA_BLE_SCAN
   static unsigned long runBleScanTime = 0;
   if (millis() - runBleScanTime >= BLESCAN_INTERVAL_MS)
   {
      runBleScanTime = millis();
      // BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
      // pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
   }
#endif
}

void setup_serial2udp(void)
{
   // 19.2K, 8-bit, Parity-Even, 1-Stop, Rx-pin-16, inverted, 100ms.
   Serial2.begin(19200, SERIAL_8E1, 16, -1, true, 100);
   Udp.beginPacket(IPADDR_BROADCAST, localUdpPort); // also flushes
}

void handle_rikaMessage(uint8_t index)
{
   switch ('A' + index)
   {
   case MSG_MINUS:
      /* record only if more than 2sec after SW btn press */
      if (millis() - swBtnPressMillis >= 2000)
      {
         userBtnPressEpoch = rtc.getEpoch();
         userBtnLast = MINUS;
      }
      break;

   case MSG_PLUS:
      /* record only if more than 2sec after SW btn press */
      if (millis() - swBtnPressMillis >= 2000)
      {
         userBtnPressEpoch = rtc.getEpoch();
         userBtnLast = PLUS;
      }
      break;

   case MSG_MENU:
      userBtnPressEpoch = rtc.getEpoch();
      userBtnLast = MENU;
      break;

   case MSG_ENTER:
      userBtnPressEpoch = rtc.getEpoch();
      userBtnLast = ENTER;
      break;

   case MSG_CL:
      rikaCleaningEpoch = rtc.getEpoch();
      break;

   case MSG_EX:
      rikaStatus = EXIT;
      break;

   default:
      break;
   }
}

void run_serial2udp(void)
{
   const uint8_t rows = 15;
   const uint8_t rikaMessage[rows][10] = {
       // STA ?     ?     D1    SUM   STOP
       {0x68, 0x01, 0x13, 0x10, 0x10, 0x16},       // A - normal
       {0x68, 0x01, 0x14, 0x10, 0x10, 0x16},       // B - normal
       {0x68, 0x01, 0x41, 0x00, 0x00, 0x16},       // C - btn released / new screen value
       {0x68, 0x02, 0x12, 0x0D, 0x10, 0x1D, 0x16}, // 2nd start
       // STA ?     ?     TYPE  D1    D2    SUM   STOP
       {0x68, 0x03, 0x41, 0x11, 0x00, 0x00, 0x11, 0x16}, // E - always
       {0x68, 0x03, 0x41, 0x11, 0x08, 0x00, 0x19, 0x16}, // F - Minus
       {0x68, 0x03, 0x41, 0x11, 0x04, 0x00, 0x15, 0x16}, // G - Plus
       {0x68, 0x03, 0x41, 0x11, 0x01, 0x00, 0x12, 0x16}, // H - Menu
       {0x68, 0x03, 0x41, 0x11, 0x02, 0x00, 0x13, 0x16}, // I - Enter

       {0x68, 0x03, 0x14, 0x40, 0x37, 0x37, 0xAE, 0x16}, // ASCII "77" on first start
       {0x68, 0x03, 0x14, 0x40, 0x53, 0x54, 0xE7, 0x16}, // ASCII "ST"
       {0x68, 0x03, 0x14, 0x40, 0x4F, 0x4E, 0xDD, 0x16}, // ASCII "ON" // Also during CL
       {0x68, 0x03, 0x14, 0x40, 0x43, 0x4c, 0xcf, 0x16}, // ASCII "CL" // 2sec
       {0x68, 0x03, 0x14, 0x40, 0x45, 0x58, 0xDD, 0x16}, // ASCII "EX"

       {0x68, 0x05, 0x15, 0x60, 0x00, 0x08, 0x01, 0x05, 0x6E, 0x16}, // 3rd start

       //     STA ?     ?     TYPE  D1    D2    D3    SUM   STOP
       //  {0x68, 0x04, 0x14, 0x41, 0x20, 0x20, 0x30, 0xB1, 0x16}, // counters
       //  {0x68, 0x04, 0x14, 0x41, 0x20, 0x31, 0x35, 0xC7, 0x16}, // 3x ASCII 1~9
       //  {0x68, 0x04, 0x14, 0x41, 0x20, 0x31, 0x38, 0xCA, 0x16}, // 18 sec
       //  {0x68, 0x04, 0x14, 0x41, 0x20, 0x32, 0x30, 0xC3, 0x16}, //
       //  {0x68, 0x04, 0x14, 0x41, 0x20, 0x32, 0x35, 0xC8, 0x16}, //
       //  {0x68, 0x04, 0x14, 0x41, 0x20, 0x33, 0x30, 0xC4, 0x16}, // 30% or 30 sec
       //  {0x68, 0x04, 0x14, 0x41, 0x20, 0x34, 0x30, 0xC5, 0x16}, //
       //  {0x68, 0x04, 0x14, 0x41, 0x20, 0x39, 0x38, 0xD2, 0x16}, // etc
   };
   const uint8_t rikaMessageAscii[2][3] = {
       {0x68, 0x03, 0x14},
       {0x68, 0x04, 0x14},
   };
   const uint8_t rikaMessageButton[1][3] = {
       {0x68, 0x03, 0x41},
   };
#if FEA_SERIAL2UDP
   const uint8_t maxLength = 30;
   uint8_t nowLength = 0, startIdx = 0;
   uint8_t buffer[maxLength];
   nowLength = Serial2.readBytes(buffer, maxLength);
   if (nowLength)
   {
      bool started = false;
      for (int i = 0; i < nowLength; i++)
      {
         if (buffer[i] == 0x68)
         {
            started = true;
            startIdx = i;
         }
         if (started && buffer[i] == 0x16)
         {
            Udp.beginPacket(); // also flushes
            bool isfound = false;
            bool isallowed = true;

            if (memcmp(&buffer[startIdx], &rikaMessageAscii[0][0], 3) == 0)
            {
               isfound = true;
               if(udp_enabled)
               {
                  Udp.printf("7SEG: %c%c%c", buffer[startIdx + 3], buffer[startIdx + 4], buffer[startIdx + 5]);
               }
            }
            else if (memcmp(&buffer[startIdx], &rikaMessageAscii[1][0], 3) == 0)
            {
               isfound = true;
               if(udp_enabled)
               {
                  Udp.printf("7SEG: %c%c%c%c", buffer[startIdx + 3], buffer[startIdx + 4], buffer[startIdx + 5], buffer[startIdx + 6]);
               }
            }

            for (int j = 0; j < rows; j++)
            {
               if (memcmp(&buffer[startIdx], &rikaMessage[j][0], i - startIdx) == 0)
               {
                  if (j == 0 || j == 1 || j == 4)
                  {
                     isallowed = false;
                  }
                  if (isallowed && !isfound)
                  {
                     if(udp_enabled)
                     {
                        Udp.printf("%c", 'A' + j);
                     }
                  }
                  isfound = true;
                  handle_rikaMessage(j);
                  break; // stop search
               }
            }
            if (isallowed && !isfound)
            {
               if(udp_enabled)
               {
                 Udp.write(&buffer[startIdx], i - startIdx + 1);
               }
            }
            started = false;
            if (isallowed)
            {
               Udp.endPacket();
            }
            else
            {
               Udp.beginPacket(); // also flushes
            }
         }
      }
   }
#endif
}

void setup()
{
   //---------------------------------------

   digitalWrite(PRESSKEY_MINUS, PRESSKEY_RELEASE);
   digitalWrite(PRESSKEY_PLUS, PRESSKEY_RELEASE);
   digitalWrite(PRESSKEY_ONOFF, PRESSKEY_RELEASE);
   pinMode(PRESSKEY_MINUS, OUTPUT);
   pinMode(PRESSKEY_PLUS, OUTPUT);
   pinMode(PRESSKEY_ONOFF, OUTPUT);
   pinMode(PIN_IN_IS_POWERED, INPUT);
   pinMode(PIN_IN_COM_NOT, INPUT);
   pinMode(PIN_IN_POWER_PRESSED_NOT, INPUT);

   //---------------------------------------

   Serial.begin(115200);

   Wire.setPins(SDA, SCL);
   Wire.begin();

   aht.begin(&Wire);

   u8g2.begin();
   u8g2.clearBuffer();
   u8g2.setFont(u8g2_font_timB10_tf); // u8g2_font_ncenB08_tr
   u8g2.firstPage();
   do
   {
      u8g2.drawStr(0, 10, "Configuration");
      u8g2.drawStr(0, 28, "WIFI: " SELF_AP_SSID);
      u8g2.drawStr(0, 48, "");
      u8g2.drawStr(0, 64, "... connexion");
   } while (u8g2.nextPage());

   // put your setup code here, to run once:
   Serial.print("\nStarting Async_AutoConnect_ESP32_minimal on " + String(ARDUINO_BOARD));
   Serial.println(ESP_ASYNC_WIFIMANAGER_VERSION);

   // ESPAsync_wifiManager.resetSettings();   //reset saved settings
   // ESPAsync_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 132, 1), IPAddress(192, 168, 132, 1), IPAddress(255, 255, 255, 0));
   ESPAsync_wifiManager.setConfigPortalTimeout(15);
   Serial.print("autoConnect");
   ESPAsync_wifiManager.autoConnect(SELF_AP_SSID);

   if (WiFi.status() == WL_CONNECTED)
   {
      Serial.print(F("Connected. Local IP: "));
      Serial.println(WiFi.localIP());

      sprintf(bufferL1, "http://%s", WiFi.localIP().toString().c_str());
      u8g2.firstPage();
      do
      {
         u8g2.drawStr(0, 10, bufferL1);
      } while (u8g2.nextPage());
   }
   else
   {
      Serial.println(ESPAsync_wifiManager.getStatus(WiFi.status()));
   }

   if (MDNS.begin(SELF_URL))
   {
      Serial.println("MDNS responder started: " SELF_URL);
   }

   webServer.on("/", HTTP_GET | HTTP_POST, htmlRoot);
   webServer.on("/config_wifi", HTTP_GET | HTTP_POST, htmlConfigWifi);
   webServer.on("/graph.svg", HTTP_GET, svgGraph);

   AsyncElegantOTA.begin(&webServer); // Start ElegantOTA
   webServer.begin();
   Serial.println("HTTP server started");

   //---------------------------------------

   _myPID.SetOutputLimits(0, outputMax); // tell the PID to range between 0 and the full window size
   if (!pidLoop)
   {
      run_aht();
      Input = temp;
      Setpoint = Input + 2.0;
      Output = 50;
      // AutoTune
      if (constrain(Output, outputMin, outputMax - outputStep - 5) < Output)
      {
         Serial.println(F("AutoTune test exceeds outMax limit. Check output, hysteresis and outputStep values"));
         while (1)
         {
         };
      }
      _myPID.AutoTune(tuningMethod::NO_OVERSHOOT_PID);
      _myPID.autoTune->autoTuneConfig(outputStep, hysteresis, Setpoint, Output, QuickPID::DIRECT, printOrPlotter, sampleTimeUs);
   }
   else
   {
      _myPID.SetSampleTimeUs(sampleTimeUs);
      _myPID.SetMode(QuickPID::AUTOMATIC);
   }

   setup_ble_scan();

   setup_serial2udp();
}

void buttonpress_onoff()
{
   swBtnPressMillis = millis();
   swBtnPressEpoch = rtc.getEpoch();
   swBtnLast = POWER;
   // press OnOff
   Serial.printf("press - ONOFF");
   digitalWrite(PRESSKEY_ONOFF, PRESSKEY_PRESS);
   delay(PRESS_TIME_MS);
   digitalWrite(PRESSKEY_ONOFF, PRESSKEY_RELEASE);
}

void run_pid(void)
{
   if (pidLoop)
   {
      if (printOrPlotter == 0)
      { // plotter
         Serial.print("Setpoint:");
         Serial.print(Setpoint);
         Serial.print(",");
         Serial.print("Input:");
         Serial.print(Input);
         Serial.print(",");
         Serial.print("Output:");
         Serial.print(Output);
         Serial.println(",");
      }
      // Input = _myPID.analogReadFast(inputPin);
      _myPID.Compute();
      // analogWrite(outputPin, Output);
   }
   else
   {
      if (_myPID.autoTune) // Avoid dereferencing nullptr after _myPID.clearAutoTune()
      {
         switch (_myPID.autoTune->autoTuneLoop())
         {
         case _myPID.autoTune->AUTOTUNE:
            // Input = avg(_myPID.analogReadFast(inputPin));
            // analogWrite(outputPin, Output);
            break;

         case _myPID.autoTune->TUNINGS:
            _myPID.autoTune->setAutoTuneConstants(&Kp, &Ki, &Kd); // set new tunings
            _myPID.SetMode(QuickPID::AUTOMATIC);                  // setup PID
            _myPID.SetSampleTimeUs(sampleTimeUs);
            _myPID.SetTunings(Kp, Ki, Kd, POn, DOn); // apply new tunings to PID
            // Setpoint = 500;
            break;

         case _myPID.autoTune->CLR:
            if (!pidLoop)
            {
               _myPID.clearAutoTune(); // releases memory used by AutoTune object
               pidLoop = true;
            }
            break;
         }
      }
   }
}

void run_extra_logic(void)
{
   static unsigned long dontSwitchOffTime = 0;
   static unsigned long dontSwitchOnTime = 0;
   if (IO_isPowerBtnPressed())
   {
      /* record only if more than 2sec after SW btn press */
      // todo check if was the same SW button
      if (millis() - swBtnPressMillis >= 2000)
      {
         userBtnPressEpoch = rtc.getEpoch();
         // TODO debounce race condition
         // if (IO_isPowerDown())
         // {
         //    ONOFF_ALLOWED = false;
         // }
         // else
         // {
         //    rikaStatus = EXIT;
         //    ONOFF_ALLOWED = false;
         // }
      }
   }

   if (IO_isPowerDown())
   {
      if (rikaStatus != OFF)
      {
         _myPID.SetMode(_myPID.MANUAL);
         rikaStatus = OFF;
      }
      if (rikaStatus == OFF && (5==tempmode || temp <= Setpoint) && ON_ALLOWED )
      {
         if (millis() - dontSwitchOnTime >= 60 * 1000)
         {
            buttonpress_onoff();
            rikaStatus = START;
         }
      }
      else
      {
         dontSwitchOnTime = millis();
      }
   }
   else if (pidLoop)
   {
      if (rikaStatus == ON)
      {
         if (Input > Setpoint - 0.5 && Output > 0.5 * outputMax)
         {
            _myPID.SetMode(_myPID.MANUAL);
            Output = 0.5 * outputMax;
         }
         else if (Input > Setpoint && Output >= 0.5 * outputMax)
         {
            _myPID.SetMode(_myPID.MANUAL);
            Output = 0.0;
         }
         else
         {
            _myPID.SetMode(_myPID.AUTOMATIC);
         }
      }
      if (temp > Setpoint + 2 && rikaStatus == ON && OFF_ALLOWED)
      {
         if (millis() - dontSwitchOffTime >= 60 * 1000)
         {
            buttonpress_onoff();
            rikaStatus = EXIT;
         }
      }
      else
      {
         dontSwitchOffTime = millis();
      }
      if (rikaStatus == EXIT)
      {
         _myPID.SetMode(_myPID.MANUAL);
         Output = 0.0;
      }
      if (rikaStatus == OFF)
      {
         rikaStatus = START;
      }
      if (rikaStatus == START)
      {
         rikaStartEpoch = rtc.getEpoch();
         _myPID.SetMode(_myPID.AUTOMATIC);
         rikaStatus = ON;
      }
   }
}

void run_buttonpress(void)
{
   if (rikaStatus == OFF)
   {
      cursor_align_done = false;
   }

   if (rikaStatus == ON)
   {
      if (cursor_align <= -wantLvlMax || cursor_align >= wantLvlMax)
      {
         cursor_align_done = true;
      }
      if (millis() - swBtnPressMillis >= PRESS_INTERVAL_DELAY_MS)
      {
         if (wantLvl < nowLvl || (wantLvl == 0 && !cursor_align_done))
         {
            if (wantLvl < nowLvl)
            {
               nowLvl--;
            }
            cursor_align--;
            // press -
            swBtnPressMillis = millis();
            swBtnPressEpoch = rtc.getEpoch();
            swBtnLast = MINUS;
            digitalWrite(PRESSKEY_MINUS, PRESSKEY_PRESS);
            Serial.printf("press - %d", nowLvl);
            delay(PRESS_TIME_MS);
            digitalWrite(PRESSKEY_MINUS, PRESSKEY_RELEASE);
         }
         if (wantLvl > nowLvl || (wantLvl == wantLvlMax && !cursor_align_done))
         {
            if (wantLvl > nowLvl)
            {
               nowLvl++;
            }
            cursor_align++;
            // press +
            swBtnPressMillis = millis();
            swBtnPressEpoch = rtc.getEpoch();
            swBtnLast = PLUS;
            digitalWrite(PRESSKEY_PLUS, PRESSKEY_PRESS);
            Serial.printf("press + %d", nowLvl);
            delay(PRESS_TIME_MS);
            digitalWrite(PRESSKEY_PLUS, PRESSKEY_RELEASE);
         }
      }
   }
}

void run_history(void)
{
   static unsigned long runHistoryTime = 0;
   if (millis() - runHistoryTime >= HISTORY_INTERVAL_DELAY_MS)
   {
      runHistoryTime = millis();
      history[historyIdx] = temp * 100;
      historyIdx++;
      if (historyIdx >= historyCnt)
      {
         for (int i = 1; i < historyCnt; i++)
         {
            history[i - 1] = history[i];
         }
         historyIdx--;
      }
   }
}

bool IO_isPowerDown(void)
{
   return digitalRead(PIN_IN_IS_POWERED) == LOW;
}

bool IO_isPowerBtnPressed(void)
{
   return digitalRead(PIN_IN_POWER_PRESSED_NOT) == LOW;
}

void run_oled(void)
{
   static unsigned long runOledTime = 0;
   if (millis() - runOledTime >= OLED_INTERVAL_DELAY_MS)
   {
      runOledTime = millis();
      Serial.printf("powered:%d com:%d onoff:%d \r\n", digitalRead(PIN_IN_IS_POWERED), digitalRead(PIN_IN_COM_NOT), digitalRead(PIN_IN_POWER_PRESSED_NOT));

      Serial.println(bufferL2);
      Serial.println(bufferL3);
      u8g2.firstPage();
      do
      {
         u8g2.drawStr(0, 10, bufferL1);
         u8g2.drawStr(0, 28, bufferL2);
         u8g2.drawStr(0, 46, bufferL3);
         u8g2.drawStr(0, 64, bufferL4);
      } while (u8g2.nextPage());
   }
}

void loop()
{
   run_getCallendar();
   run_aht();
   run_history();

   Input = temp;
   run_pid();
   run_extra_logic();
   wantLvl = Output / (outputMax / wantLvlMax);
   if(5==tempmode) // Flamme 0%
   {
      wantLvl = 0;
   }

   run_buttonpress();

   int sec = millis() / 1000;
   int min = sec / 60;
   int hr = min / 60;
   sprintf(bufferL2, "%.2f*C %.0f%%rH", temp, humidity);
   sprintf(bufferL3, "Set:%.1f*C O:%.0f%%", Setpoint, Output);
   sprintf(bufferL4, "%s: %d -> %d", _myPID.GetMode() == _myPID.AUTOMATIC ? "Auto" : "Manu", nowLvl * 5, wantLvl * 5);
   sprintf(bufferPID, "Kp:%.4f Ki:%.4f Kd:%.4f", Kp, Ki, Kd);
   sprintf(bufferUptime, "Uptime: %02d:%02d:%02d %s %s", hr, min % 60, sec % 60, IO_isPowerDown() ? "Off" : "On", rikaStatus_toString[rikaStatus].c_str());

   run_oled();

   run_ble_scan();

   run_serial2udp();

   // delay(100);
}

void svgGraph(AsyncWebServerRequest *request)
{
   char temp[PAGE_BUFFER];
   String out = "";
   out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"350\" height=\"200\">\n";
   out += "<rect width=\"350\" height=\"200\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
   out += "<g stroke=\"black\" stroke-width=\"1\">\n";
   out += "<line x1=\"0\" y1=\"100\" x2=\"350\" y2=\"100\" stroke=\"red\" />\n";

   float min = Setpoint, max = Setpoint, scaleC, scalePx;
   for (int x = 0; x < historyIdx; x++)
   {
      min = MIN(min, (float)history[x] / 100);
      max = MAX(max, (float)history[x] / 100);
   }
   scaleC = (int)(MAX(max - Setpoint, Setpoint - min)) + 1; // 26.7-25.5
   scalePx = 100 / scaleC;

   snprintf(temp, PAGE_BUFFER, "<text x=\"10\" y=\"15\" fill=\"red\">%.2f*C</text>\n", Setpoint + scaleC);
   out += temp;
   snprintf(temp, PAGE_BUFFER, "<text x=\"10\" y=\"95\" fill=\"green\">%.2f*C</text>\n", Setpoint);
   out += temp;
   snprintf(temp, PAGE_BUFFER, "<text x=\"10\" y=\"195\" fill=\"blue\">%.2f*C</text>\n", Setpoint - scaleC);
   out += temp;

   float y = history[0];
   for (int x = 1; x < historyIdx; x++)
   {
      float y2 = history[x];
      snprintf(temp, PAGE_BUFFER, "<line x1=\"%d\" y1=\"%.2f\" x2=\"%d\" y2=\"%.2f\" class=\"temp_%.2f\" />\n",
               x - 1,
               100 - (y / 100 - Setpoint) * scalePx,
               x,
               100 - (y2 / 100 - Setpoint) * scalePx,
               y / 100);
      out += temp;
      y = y2;
   }
   out += "</g>\n</svg>\n";
   request->send(200, "image/svg+xml", out);
}

void handlePost(AsyncWebServerRequest *request)
{
   if (request->hasParam("setpoint", true))
   {
      AsyncWebParameter *p = request->getParam("setpoint", true);
      Setpoint = p->value().toFloat();
   }
   if (request->hasParam("setpoint", false))
   {
      AsyncWebParameter *p = request->getParam("setpoint", false);
      Setpoint = p->value().toFloat();
   }
}

void htmlRoot(AsyncWebServerRequest *request)
{
   char str[PAGE_BUFFER];
   handlePost(request);

   snprintf(str, PAGE_BUFFER,
            "<html><head>\
<meta http-equiv='refresh' content='10'/>\
<title>" SELF_AP_SSID "</title>\
<style>body { background-color: #e5e5e5; Color: #000088; }</style></head>\
<body><h1>Rika-Arduino</h1> \
%s <br> \
%s <br> \
%s <br> \
%s <br> \
%s <br> \
%s <br>\
<img src=\"/graph.svg\" /> <br> \
<a href='%s'>Changer la Température</a> <br> \
<a href='/update'>Mettre à jour le Firmware</a> <br> \
<a href='/config_wifi'>Configuration WIFI</a> <br> \
</body></html>",
            bufferL1,
            bufferL2,
            bufferL3,
            bufferL4,
            bufferPID,
            bufferUptime,
            urlFrontend.c_str());

   request->send(200, "text/html", str);
}

void htmlConfigWifi(AsyncWebServerRequest *request)
{
   char str[PAGE_BUFFER];
   handlePost(request);

   snprintf(str, PAGE_BUFFER,
            "<html><head>\
<meta http-equiv='refresh' content='10'/>\
<title>" SELF_AP_SSID "</title>\
<style>body { background-color: #e5e5e5; Color: #000088; }</style></head>\
<body> \
Connectez vous au réseau '" SELF_AP_SSID "' puis <a href='/'>continuer</a> <br> \
</body></html>");

   request->send(200, "text/html", str);

   ESPAsync_wifiManager.startConfigPortal(SELF_AP_SSID);
}
