#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include "headers.h"
#include "time.h"
#include "ph_surveyor.h"
#include "rtd_surveyor.h"
#include <sequencer1.h> //imports a 1 function sequencer
#include <sequencer2.h> //imports a 2 function sequencer
#include <Ezo_i2c.h>    //include the EZO I2C library from https://github.com/Atlas-Scientific/Ezo_I2c_lib
#ifdef ENABLE_ATLAS_EC
#include <Ezo_i2c.h> //include the EZO I2C library from https://github.com/Atlas-Scientific/Ezo_I2c_lib
#endif
#include <Wire.h>                     //include arduinos i2c library
#include <Ezo_i2c_util.h>             //brings in common print statements
#include <Adafruit_Sensor.h>          //Library for Adafruit sensors
#include <DHT.h>                      // Sensor for Humidity and temperature.
#include "FreeSerifBoldItalic9pt7b.h" // For the cool font at the startup
#include <iot_cmd.h>
#include <Ezo_i2c_util.h>

#ifdef ENABLE_GPS
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#endif

#ifdef ENABLE_SETPOINT_FETCH
#include <ArduinoJson.h>

JsonDocument doc;

double EC_MIN, EC_MAX, PH_MIN, PH_MAX;
double EC_AVG = (EC_MIN + EC_MAX) / 2;
double PH_AVG = (PH_MIN + PH_MAX) / 2;

#endif

char EC_data[32]; // we make a 32-byte character array to hold incoming data from the EC sensor.
char *EC_str;     // char pointer used in string parsing.
char *TDS;        // char pointer used in string parsing.
char *SAL;        // char pointer used in string parsing (the sensor outputs some text that we don't need).
char *SG;         // char pointer used in string parsing.

float EC_float;  // float var used to hold the float value of the conductivity.
float TDS_float; // float var used to hold the float value of the total dissolved solids.
float SAL_float; // float var used to hold the float value of the salinity.
float SG_float;  // float var used to hold the float value of the specific gravity.

#ifdef ENABLE_PUMPS
void pump_API();
int last_Dose = 0;
int FERTILIZER_DOSAGE = 10;        // 10ml of Part A 5-15-26
                                   // 10ml of Part B 5-0-0 Calcium Nitrate
                                   // These two components of fertilizer will be dosed at the same time.
int pH_DOSAGE = 10;                // 10ml of Sulfuric Acid or Potassium Hydroxide
int INTERVAL_TIME = 1 * 30 * 1000; // For testing this is just 1/2 minute its taking forever

Ezo_board PMP1 = Ezo_board(101, "PMP1"); // create an PMP circuit object who's address is 56 and name is "PMP1"
Ezo_board PMP2 = Ezo_board(102, "PMP2"); // create an PMP circuit object who's address is 57 and name is "PMP2"
Ezo_board PMP3 = Ezo_board(103, "PMP3"); // create an PMP circuit object who's address is 58 and name is "PMP3"
Ezo_board device_list[] = {              // an array of boards used for sending commands to all or specific boards
    PMP1,
    PMP2,
    PMP3};
bool process_coms(const String &string_buffer);
void print_help();
Ezo_board *default_board = &device_list[0]; // used to store the board were talking to
// gets the length of the array automatically so we dont have to change the number every time we add new boards
const uint8_t device_list_len = sizeof(device_list) / sizeof(device_list[0]);
const unsigned long reading_delay = 100; // how long we wait to receive a response, in milliseconds decreasing from 1000
unsigned int poll_delay = 200 - reading_delay; //there is no response so im going to decrease this decreassing from 2000-reading delay
void step3(); // forward declarations of functions to use them in the sequencer before defining them
void step4();
Sequencer2 PumpSeq(&step3, reading_delay, // calls the steps in sequence with time in between them
                   &step4, poll_delay);
bool polling = false; // variable to determine whether or not were polling the circuits
#endif

Ezo_board EC = Ezo_board(100, "EC"); // create an EC circuit object who's address is 100 and name is "EC"
void step1();                        // forward declarations of functions to use them in the sequencer before defining them
void step2();
Sequencer2 Seq(&step1, 1000, &step2, 300);
Surveyor_RTD RTD = Surveyor_RTD(A1_temp_Pin);
Surveyor_pH pH = Surveyor_pH(pH_Pin);
int wifiManagerTimeout = 120; // in seconds

WiFiManager wm;
Preferences config;
int UNIT_NUMBER;
WiFiManagerParameter unit_number_param("unit_number", "Unit Number", String(UNIT_NUMBER).c_str(), 4);
// String apiId, apiKey;
char apiId[40], apiKey[40];
WiFiManagerParameter api_id_param("api_id", "API ID", apiId, 40);
WiFiManagerParameter api_key_param("api_key", "API Key", apiKey, 40);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

uint8_t user_bytes_received = 0;
const uint8_t bufferlen = 32;
char user_data[bufferlen];

void parse_cmd(char *string);
void printLocalTime();
void startOnDemandWiFiManager();
void saveWMConfig();

#ifndef DISABLE_LCD

#include <LCD_I2C.h>
LCD_I2C lcd(LCD_ADDR, 20, 4);
int display_current_page = 0;
void show_display_page(int pageNum);
int last_switch_time = 0;
int cycle_time = 5000;
int display_page_count = 2;
int pages[10];
void setup_pages();

#endif

#if defined(ENABLE_DHT11_TEMP) || defined(ENABLE_DHT11_HUMIDITY)
DHT dht(DHT11PIN, DHTTYPE);
float readDHT11Temp();
float readDHT11humidity();
#endif

#ifdef ENABLE_GPS
SoftwareSerial GPSPort;
TinyGPSPlus gps;
float GPS_LAT = 0, GPS_LONG = 0, GPS_ELEV = 0;
void updateGPS();
#endif

#ifdef ENABLE_ATLAS_TEMP
float atlasTemp;
#endif
#ifdef ENABLE_ATLAS_pH
float atlasPH;
#endif
#ifdef ENABLE_ATLAS_EC
float atlasEC;
#endif

void updateDisplay()
{
#ifdef ENABLE_OLED_DISPLAY
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setFont(&FreeSerifBoldItalic9pt7b);
  display.setCursor(35, 20);
  display.println("Ryan Fraser Josh Thaw");
  display.display();
  display.setFont();
#endif
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ryan Fraser");
  lcd.setCursor(0, 1);
  lcd.print("Josh Thaw");
}

HTTPClient http;
WiFiClientSecure client;

void setup()
{
#ifndef DISABLE_LCD
  pages[0] = 0;
  pages[1] = 1;
  setup_pages();
#endif
  client.setCACert(root_ca);
  Wire.begin(SDA_PIN, SCL_PIN);

#ifndef DISABLE_LCD

  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Starting Sensor");

// delay();
#endif

  Serial.begin(115200);

  Serial.println("");
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Starting Integrated Sensor");
  Serial.println("");
  pinMode(LED15, OUTPUT);

  config.begin("config");
  UNIT_NUMBER = config.getInt("unit_number", 0);
  strcpy(apiKey, config.getString("api_key", "").c_str());
  strcpy(apiId, config.getString("api_id", "").c_str());
  config.end();

  unit_number_param.setValue(String(UNIT_NUMBER).c_str(), 4);
  api_id_param.setValue(String(apiId).c_str(), 40);
  api_key_param.setValue(String(apiKey).c_str(), 40);

#ifdef ENABLE_PUMPS
  PumpSeq.reset();
#endif

#ifdef ENABLE_ATLAS_EC
  Seq.reset(); // initialize the sequencer
  delay(3000);
  EC.send_cmd("o,tds,1"); // send command to enable TDS output
  delay(300);
  EC.send_cmd("o,s,1"); // send command to enable salinity output
  delay(300);
  EC.send_cmd("o,sg,1"); // send command to enable specific gravity output
  delay(300);
#endif

#ifndef CALIBRATION_MODE
  Serial.println(F("Use command \"CAL,nnn.n\" to calibrate the circuit to a specific temperature\n\"CAL,CLEAR\" clears the calibration"));
  if (RTD.begin())
  {
    Serial.println("Loaded EEPROM");
  }
#endif
#ifdef ENABLE_OLED_DISPLAY
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  updateDisplay();
  delay(2500);
#endif

#if defined(ENABLE_DHT11_TEMP) || defined(ENABLE_DHT11_HUMIDITY)
  dht.begin();
#endif

#ifdef ENABLE_GPS
  GPSPort.begin(9600, SWSERIAL_8N1, GPS_RX, GPS_TX, false);
  if (!GPSPort)
  { // If the object did not initialize, then its configuration is invalid
    Serial.println("Invalid SoftwareSerial pin configuration, check config");
    while (1)
    { // Don't continue with invalid configuration
      delay(1000);
    }
  }
  Serial.println("GPS Sensor Setup Complete");
  Serial.println("\n");
#endif

#ifndef DISABLE_WIFI
  Serial.println(WiFi.macAddress());
  WiFi.mode(WIFI_STA);
#ifdef ENABLE_OLED_DISPLAY
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.display();
  display.setCursor(0, 0);
  display.println(WiFi.macAddress());
#endif
  if (wm.getWiFiIsSaved())
  {
    lcd.setCursor(0, 0);
    lcd.clear();
    lcd.print("Connecting to WiFi:");
    lcd.setCursor(0, 1);
    lcd.print(wm.getWiFiSSID());
    lcd.setCursor(0, 2);
#ifdef ENABLE_OLED_DISPLAY
    display.println("Connecting to previously saved WiFi network:");
    display.println(wm.getWiFiSSID());
    display.display();
#endif
    WiFi.begin(wm.getWiFiSSID(), wm.getWiFiPass());
    Serial.print("Connecting to Saved WiFi ..");
    int wait_time = 0;
    while ((WiFi.status() != WL_CONNECTED) && (wait_time < 10))
    {
      Serial.print('.');
#ifdef ENABLE_OLED_DISPLAY
      display.print(".");
      display.display();
#endif
      lcd.print(".");
      digitalWrite(LED15, HIGH);
      delay(500);
      digitalWrite(LED15, LOW);
      delay(500);
      wait_time++;

      if (wait_time > 18)
      {
        Serial.print("WiFi Failed to connect.");
#ifdef ENABLE_OLED_DISPLAY
        display.println();
        display.println("WiFi failed to Connect.\nPress the BOOT button for 3 seconds to start the config.");
        display.display();
#endif
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi Connect Fail");
        lcd.setCursor(0, 1);
        lcd.print("Press BOOT for WiFi Config");
      }
      // Serial.print(wait_time);
    }
    Serial.println(WiFi.localIP());
#ifdef ENABLE_OLED_DISPLAY
    display.println(WiFi.localIP());
    display.display();
#endif
    lcd.setCursor(0, 3);
    lcd.print(WiFi.localIP());
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No WiFi Saved");
    lcd.setCursor(0, 1);
    lcd.print("Press BOOT for WiFi Config");
#ifdef ENABLE_OLED_DISPLAY
    display.println("No WiFi saved.");
    display.println("Press the BOOT button for 3 seconds to start the config.");
    display.display();
#endif
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  printLocalTime();
  delay(3000);
#ifdef ENABLE_OLED_DISPLAY
  display.clearDisplay();
#endif

  pinMode(WIFIMANAGER_TRIGGER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WIFIMANAGER_TRIGGER_PIN), startOnDemandWiFiManager, RISING);
  wm.addParameter(&unit_number_param);
  wm.addParameter(&api_id_param);
  wm.addParameter(&api_key_param);
#endif
#ifdef DISABLE_WIFI
  WiFi.mode(WIFI_MODE_NULL);

#endif
#ifdef ENABLE_OLED_DISPLAY
  display.setTextColor(WHITE);
  display.clearDisplay();
#endif
}
unsigned int lastTime = 0;

int counter = 0;

int onDemandManagerTrigger = false;
int lastButtonPress = 0;
int didLastManager = 0;
int button_held_wifi_manager = 0;

void loop()
{

#ifndef DISABLE_LCD
  // For the LCD the first parameter is the Column 0-20
  int lcd_temp = 100;
  // int last_switch_time = 0;
  // int cycle_time = 5000;
  // lcd.clear();

  if (millis() - last_switch_time > cycle_time)
  {
    display_current_page++;
    if (display_current_page == display_page_count)
    {
      display_current_page = 0;
    }
    last_switch_time = millis();
  }
  show_display_page(pages[display_current_page]);
// #ifdef ENABLE_DHT11_TEMP
//   lcd.setCursor(0, 0);
//   lcd.print("Temp:");
//   // char tempStr[3];
//   // dtostrf(lcd_temp,3,3,tempStr);
//   lcd.print(readDHT11Temp(), 0);
//   lcd.print(char(223)); // print degree
//   lcd.print("F");
// #endif
// #ifdef ENABLE_ATLAS_pH
//   lcd.setCursor(0, 1);
//   lcd.print("pH:");
// #endif
// #ifdef ENABLE_ATLAS_EC
//   lcd.setCursor(0, 2);
//   lcd.print("EC:");
// #endif
// #ifndef DISABLE_WIFI
//   lcd.setCursor(0, 3);
//   lcd.print("RSSI:");
//   lcd.print(WiFi.RSSI());
//   lcd.print("dBm");
// #endif
// #ifdef ENABLE_DHT11_HUMIDITY
//   lcd.setCursor(10, 0);
//   lcd.print("Humid:");
//   lcd.print(readDHT11humidity(), 0);
//   lcd.print("%");
// #endif
// #ifndef DISABLE_UNIT_DISPLAY
//   lcd.setCursor(10, 1);
//   lcd.print("Unit #");
//   lcd.print(UNIT_NUMBER);
//   // lcd.print("");
// #endif
#endif

  if (Serial.available() > 0)
  {
    user_bytes_received = Serial.readBytesUntil(13, user_data, sizeof(user_data));
  }

  if (user_bytes_received)
  {
    parse_cmd(user_data);
    user_bytes_received = 0;
    memset(user_data, 0, sizeof(user_data));
  }

// Serial.println(RTD.read_RTD_temp_C());

// uncomment for readings in F
// Serial.println(RTD.read_RTD_temp_F());
#ifndef DISABLE_WIFI
  delay(500);
  if (onDemandManagerTrigger == true)
  {
    if (millis() < didLastManager)
    {
      // Serial.println("too soon");
      didLastManager = 0;
      onDemandManagerTrigger = false;
      return;
    }
    // Serial.println("In trigger code");
    didLastManager = millis() + 5000 + (wifiManagerTimeout * 1000);
    onDemandManagerTrigger == false;
    // { // button pressed
    // nneds to be held for 3 seconds

    // delay(3000);
    // if (digitalRead(WIFIMANAGER_TRIGGER_PIN) == 0)
    // {
    config.begin("config");

    Serial.println("Button held for WM, starting config portal");

    String AP_Name = "ESP_UNIT_";
    AP_Name += String(UNIT_NUMBER);
#ifdef ENABLE_OLED_DISPLAY
    display.setTextSize(1);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("Starting Configuration. Join WiFi:");
    display.println(AP_Name);
    display.println("fa9s8dS7d92J");
    display.println("And go to 192.168.4.1");
    display.display();
#endif
    lcd.clear();

    lcd.print("Config - Join WiFi");
    lcd.setCursor(0, 1);
    lcd.print(AP_Name);
    lcd.setCursor(0, 2);
    lcd.print("fa9s8dS7d92J");
    lcd.setCursor(0, 3);
    lcd.print("Go to 192.168.4.1");
    wm.setConfigPortalTimeout(wifiManagerTimeout);
    wm.setBreakAfterConfig(true);
    wm.setSaveConfigCallback(saveWMConfig);

    if (!wm.startConfigPortal(AP_Name.c_str(), "fa9s8dS7d92J"))
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Config Failed...");
      delay(2000);
    }
    else
    {
      ESP.restart();
    }
  }
#endif

#ifdef ENABLE_ATLAS_EC
  Seq.run(); // run the sequncer to do the polling
#endif
#ifdef ENABLE_OLED_DISPLAY
  display.clearDisplay();
  // temperature Display on OLED
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("TEMP: ");
  display.setTextSize(2);
  display.setCursor(0, 10);
#endif

#ifdef ENABLE_ATLAS_TEMP
  float temperatureF = RTD.read_RTD_temp_F();
  atlasTemp = temperatureF;
#ifdef LESS_SERIAL_OUTPUT
  Serial.print("Temp:");
  Serial.print(temperatureF);
  Serial.println();
#endif
#ifdef ENABLE_OLED_DISPLAY
  display.print(temperatureF, 1);
  display.setTextSize(2);
  display.print("F");
#endif
#endif
#ifndef DISABLE_MCP9701_TEMP
  float sensorValue = analogRead(MCP9701_temp_Pin);
  float voltage = sensorValue * (3.3 / 4095.0);
  float temperatureC = (voltage - 0.5) / 0.01;
  float fahrenheit = (temperatureC * 9.0) / 5.0 + 32;
#ifdef ENABLE_OLED_DISPLAY
  display.print(fahrenheit, 1);
  display.setTextSize(2);
  display.print("F");
#endif
#ifndef LESS_SERIAL_OUTPUT
  Serial.print("MCP9701 Temperature:\t\t\t");
  Serial.print(fahrenheit);
  Serial.print("\xC2\xB0"); // shows degree symbol
  Serial.println("F");
#endif
  float temperatureF = fahrenheit;
#endif
#if !defined ENABLE_ATLAS_TEMP && defined DISABLE_MCP9701_TEMP
#ifdef ENABLE_OLED_DISPLAY
  display.print("-");
#endif
#endif

// Serial.print(temperatureF);
// display.print(" ");
// display.setTextSize(1);
// display.cp437(true);
// display.write(167);
// display.print(fahrenheit);

// pH Display on OLED
#ifdef ENABLE_OLED_DISPLAY
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("pH: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
#endif
#ifdef ENABLE_ATLAS_pH
  atlasPH = pH.read_ph();
#ifdef ENABLE_OLED_DISPLAY
  display.print(pH.read_ph(), 1);
#endif
#else
#ifdef ENABLE_OLED_DISPLAY
  display.print("-");
#endif
#endif
#ifdef ENABLE_OLED_DISPLAY
  display.println(" ");
#endif

#ifndef LESS_SERIAL_OUTPUT
  // Serial.print("RSSI: \t");
  Serial.print("Received Signal Strength Indicator:\t");
  Serial.print(WiFi.RSSI());
  Serial.println("dBm");
#endif

#ifdef ENABLE_ATLAS_pH
  Serial.print("pH: ");
  Serial.println(atlasPH);
#endif

//
#ifdef ENABLE_OLED_DISPLAY
  display.setTextSize(1);
  display.setCursor(60, 0);
  display.println("RSSI:");
  display.setCursor(90, 0);
#endif
#ifndef DISABLE_WIFI
#ifdef ENABLE_OLED_DISPLAY
  display.print(WiFi.RSSI());
  display.print("dBm");
#endif
#else
#ifdef ENABLE_OLED_DISPLAY
  display.print("-");
#endif
#endif
#ifdef ENABLE_OLED_DISPLAY
  display.setCursor(72, 34);
  display.setTextSize(1);
  display.println("EC:");
  display.setCursor(72, 44);
  display.setTextSize(2);
#endif
#ifdef ENABLE_ATLAS_EC
  atlasEC = EC_float / 1000;
#ifdef ENABLE_OLED_DISPLAY
  display.print(EC_float / 1000, 1);
#endif
#else
  display.print("-");
#endif
  // display.print("mS/cm");

#ifdef ENABLE_DHT11_TEMP
  // float DHT_tempF = readDHTTemp();
  Serial.print("DHT Temperature: \t\t\t");
  Serial.print(readDHT11Temp());
  Serial.print("\xC2\xB0"); // shows degree symbol
  Serial.print("F\t");
// display.print(/1000,1);
#endif

#ifdef ENABLE_DHT11_HUMIDITY
  // readDHT11humidity();
  Serial.print("\nHumidity: \t\t\t\t");
  Serial.print(readDHT11humidity());
  Serial.println(" %");
// display.print(/1000,1);
#endif
#ifndef LESS_SERIAL_OUTPUT
  Serial.println("-----------------------------------------------");

#endif
#ifdef ENABLE_OLED_DISPLAY
  display.display();
#endif

#ifndef DISABLE_API_REQUEST
  // Serial.println(apiId);
  if ((counter == 0 || ((millis() - lastTime) > delayTime)) && UNIT_NUMBER != 0 && strcmp(apiId, "") != 0 && strcmp(apiKey, "") != 0)
  {
#ifdef ENABLE_GPS
    updateGPS();
    Serial.print("lat: ");
    Serial.println(GPS_LAT);
    Serial.print("long: ");
    Serial.println(GPS_LONG);
#endif
#ifdef ENABLE_OLED_DISPLAY
    display.fillTriangle(106, 10, 121, 15, 106, 23, 1);
    display.display();
#endif

#ifdef ENABLE_SETPOINT_FETCH
    http.useHTTP10(true);
#endif
    Serial.print("Loop counter: ");
    Serial.println(++counter);
    http.begin(client, envDataRequestURL.c_str());
    http.addHeader("Content-Type", "application/json");
    printLocalTime();
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
      Serial.println("Failed to obtain time");
      return;
    }
    Serial.println("Time variables");
    char timeHour[3];
    char timeMinute[3];
    strftime(timeHour, 3, "%H", &timeinfo);
    Serial.println(timeHour);
    strftime(timeMinute, 3, "%M", &timeinfo);
    Serial.println(timeMinute);
    char timeWeekDay[10];
    strftime(timeWeekDay, 10, "%A", &timeinfo);
    Serial.println(timeWeekDay);
    Serial.println();

    String requestBody = "{\"unitNumber\":\"";

    requestBody += String(UNIT_NUMBER) + "\",\"pH\":" + String(pH.read_ph()) + ",\"temp\":" + String(temperatureF);
    requestBody += ",\"timeRecorded\": \"" + String(timeWeekDay) + "-" + String(timeHour) + ":" + String(timeMinute) + "\"";
    requestBody += ",\"ec\":" + String(EC_float / 1000);
    requestBody += ",\"rssi\":" + String(WiFi.RSSI());
#if defined(ENABLE_DHT11_HUMIDITY)
    requestBody += ",\"humidity\":" + String(readDHT11humidity());
#endif

#if defined(ENABLE_GPS)
    Serial.println(GPS_LAT);
    if (GPS_LAT != 0.00)
    {
      requestBody += ",\"lat\":" + String(GPS_LAT);
      requestBody += ",\"long\":" + String(GPS_LONG);
    }
#endif

#ifdef ENABLE_SETPOINT_FETCH
    requestBody += ",\"returnSetPoints\":true";
#endif
    requestBody += ",\"id\": \"" + String(apiId) + String("\",\"key\": \"") + String(apiKey) + String("\"");
    requestBody += "}";

    Serial.println("Request Body:");
    Serial.println(requestBody);
    int httpResponseCode = http.POST(requestBody.c_str());

    Serial.print("httpResponseCode: ");
    Serial.println(httpResponseCode);

    lastTime = millis();
    if (httpResponseCode == 200)
    {
#ifdef ENABLE_OLED_DISPLAY
      display.fillTriangle(106, 10, 121, 15, 106, 23, 0);
      display.display();
#endif

#ifdef ENABLE_SETPOINT_FETCH
      DeserializationError error = deserializeJson(doc, http.getStream());

      if (error)
      {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        // return;
      }

      // Fetch the values
      //
      // Most of the time, you can rely on the implicit casts.
      // In other case, you can do doc["time"].as<long>();
      EC_MIN = doc["setPoints"]["ec"]["min"];
      EC_MAX = doc["setPoints"]["ec"]["max"];
      PH_MIN = doc["setPoints"]["pH"]["min"];
      PH_MAX = doc["setPoints"]["pH"]["max"];

#ifndef LESS_SERIAL_OUTPUT
      Serial.print("EC min: ");
      Serial.println(EC_MIN);
      Serial.print("EC max: ");
      Serial.println(EC_MAX);
      Serial.print("pH min: ");
      Serial.println(PH_MIN);
      Serial.print("pH max: ");
      Serial.println(PH_MAX);
#endif
#endif
    }
    else
    {
      // LogData(requestBody);
    }
    http.end();
  }
#endif

#ifdef ENABLE_PUMPS
  // String cmd; // variable to hold commands we send to the kit
  // if (receive_command(cmd))
  // {                  // if we sent the kit a command it gets put into the cmd variable
  //   polling = false; // we stop polling
  //   if (!process_coms(cmd))
  //   {                                                                    // then we evaluate the cmd for kit specific commands
  //     process_command(cmd, device_list, device_list_len, default_board); // then if its not kit specific, pass the cmd to the IOT command processing function
  //   }
  // }
  // if (polling == true)
  // { // if polling is turned on, run the sequencer
  //   PumpSeq.run();
  // }
  delay(50);

  PumpSeq.run();
#endif
}

void parse_cmd(char *string)
{
  strupr(string);
  String cmd = String(string);
  if (cmd.startsWith("CAL"))
  {
    int index = cmd.indexOf(',');
    if (index != -1)
    {
      String param = cmd.substring(index + 1, cmd.length());
      if (param.equals("CLEAR"))
      {
        RTD.cal_clear();
        Serial.println("CALIBRATION CLEARED");
      }
      else
      {
        RTD.cal(param.toFloat());
        Serial.println("RTD CALIBRATED");
      }
    }
  }
}

void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Day of week: ");
  Serial.println();
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");

  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay, 10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();
}

void IRAM_ATTR startOnDemandWiFiManager()
{
  Serial.println("Interrupt started");
  onDemandManagerTrigger = true;
  lastButtonPress = millis();
  return;
}

void saveWMConfig()
{
  config.begin("config");

  UNIT_NUMBER = atoi(unit_number_param.getValue());
  config.putInt("unit_number", UNIT_NUMBER);

  strcpy(apiId, api_id_param.getValue());
  config.putString("api_id", apiId);
  strcpy(apiKey, api_key_param.getValue());
  config.putString("api_key", apiKey);

  config.end();
  Serial.println("config saved");
}

void step1()
{
  // send a read command using send_cmd because we're parsing it ourselves
  EC.send_cmd("r");
  // for DO we use the send_read_cmd function so the library can parse it
  // DO.send_read_cmd();
}

void step2()
{

  EC.receive_cmd(EC_data, 32); // put the response into the buffer

  EC_str = strtok(EC_data, ","); // let's parse the string at each comma.
  TDS = strtok(NULL, ",");       // let's parse the string at each comma.
  SAL = strtok(NULL, ",");       // let's parse the string at each comma
  SG = strtok(NULL, ",");        // let's parse the string at each comma.

  Serial.print("EC: "); // we now print each value we parsed separately.
  Serial.print(EC_str); // this is the EC value.

  Serial.print(" TDS: "); // we now print each value we parsed separately.
  Serial.print(TDS);      // this is the TDS value.

  Serial.print(" SAL: "); // we now print each value we parsed separately.
  Serial.print(SAL);      // this is the salinity point.

  Serial.print(" SG: "); // we now print each value we parsed separately.
  Serial.println(SG);    // this is the specific gravity point.

  // receive_and_print_reading(DO);             //get the reading from the DO circuit
  Serial.println();

  EC_float = atof(EC_str);
  // DO.send_cmd_with_num("s,", EC_float);
  // uncomment this section if you want to take the values and convert them into floating point number.
  /*
     EC_float=atof(EC_str);
     TDS_float=atof(TDS);
     SAL_float=atof(SAL);
     SG_float=atof(SG);
  */
}
#ifdef ENABLE_PUMPS
void step3()
{
  PMP1.send_cmd_with_num("d,", 1,.5);
  if (millis() > last_Dose + INTERVAL_TIME)
  {
    // EC DOSING
    if ((EC_float/1000) < EC_MAX)
    // if(1)
    {
      {
 //       PMP1.send_cmd_with_num("d,", 1,.5);
        // PMP2.send_cmd_with_num("d,", 1,.5);
      }
      // PH DOSING
      if (pH.read_ph() < PH_AVG) // This needs to be a more stable value than just a instantaneous pH reading.  pH average should be implemented
      {
        {
          //
          PMP3.send_cmd_with_num("d,", pH_DOSAGE); // For now just to run the pumps i will put this with these functions.
        }
      }
    }
  }
}

void step4()
{
  digitalWrite(LED12,1); //blinking led 12 in step4 to debug
  delay(250);
  digitalWrite(LED12,0);
  // receive_and_print_reading(PMP1);             //get the reading from the PMP1 circuit
  // Serial.print("  ");
  // receive_and_print_reading(PMP2);
  // Serial.print("  ");
  // receive_and_print_reading(PMP3);
  // Serial.println();
}

void pump_API()
{
  http.begin(client, pumpRequestURL.c_str());
  http.addHeader("Content-Type", "application/json");
  String requestBody = "{\"unitNumber\":\"";

  requestBody += String(UNIT_NUMBER) + "\"";
  requestBody += ",\"pump1\":" + String(FERTILIZER_DOSAGE);
  requestBody += ",\"pump2\":" + String(FERTILIZER_DOSAGE);
  requestBody += ",\"pump3\":" + String(pH_DOSAGE);
  requestBody += ",\"id\": \"" + String(apiId) + String("\",\"key\": \"") + String(apiKey) + String("\"");
  requestBody += "}";
  int httpResponseCode = http.POST(requestBody.c_str());
  Serial.print("on pump post, httpResponseCode: ");
  Serial.println(httpResponseCode);
  if (httpResponseCode == 200)
  {
    // code if success
  }
  else
  {
    Serial.println("Error with webreqest for pump data");
  }
}
#endif

#if defined(ENABLE_DHT11_TEMP) || defined(ENABLE_DHT11_HUMIDITY)
float readDHT11Temp()
{
  float DHT11_tempC = dht.readTemperature();
  float DHT11_tempF = (DHT11_tempC * 9.0) / 5.0 + 32;

#ifndef DISABLE_FAHRENEIT
  // Serial.print("DHT11 Temperature: ");
  // Serial.print(DHT11_tempF);
  // Serial.print(" *F\tDHT11");
  return DHT11_tempF;
#endif

#ifndef DISABLE_CELSIUS

  // print the result to Serial Monitor
  Serial.print("DHT11 Temperature: ");
  Serial.print(DHT11_tempC);
  Serial.print(" *C\tDHT11");
  return DHT11_tempC;
#endif
}
#endif

#if defined(ENABLE_DHT11_TEMP) || defined(ENABLE_DHT11_HUMIDITY)
float readDHT11humidity()
{
  float humidity = dht.readHumidity();
#ifdef ENABLE_DHT11_HUMIDITY
  return humidity;
#endif

  // Serial.print("Humidity: ");
  // Serial.print(h);
  // Serial.println(" %");
}
#endif

#ifdef ENABLE_GPS
void updateGPS()
{
  Serial.println("update GPS function");
  // while (GPSPort.available() > 0)
  // {
  //   Serial.println("GPS Avail");
  //   gps.encode(GPSPort.read());

  //   if (gps.location.isValid())
  //   {
  while (GPSPort.available())
  {
    gps.encode(GPSPort.read());
    if (gps.location.isValid())
    {
      Serial.println("GPS Is Updated");
      Serial.println(gps.location.lat());
      GPS_LAT = gps.location.lat();
      GPS_LONG = gps.location.lng();
    }
    // }
  }
}
#endif

#ifndef DISABLE_LCD
bool multiple_temps = false;
void setup_pages()
{
  int temp_count = 0;
#ifdef ENABLE_DHT11_TEMP
  temp_count++;
#endif
#ifdef ENABLE_ATLAS_TEMP
  temp_count++;
#endif
#ifndef DISABLE_MCP9701_TEMP
  temp_count++;
#endif

  if (temp_count > 1)
  {
    pages[display_page_count] = 2;
    display_page_count++;
    multiple_temps = true;
  }
}
#endif

void show_display_page(int pageNum)
{
  lcd.clear();
  if (pageNum == 0)
  {
    if (!multiple_temps)
    {
#ifdef ENABLE_DHT11_TEMP
      lcd.setCursor(0, 0);
      lcd.print("Temp:");
      // char tempStr[3];
      // dtostrf(lcd_temp,3,3,tempStr);
      lcd.print(readDHT11Temp(), 0);
      lcd.print(char(223)); // print degree
      lcd.print("F");
#endif
#ifdef ENABLE_ATLAS_TEMP
      // float temperatureF = RTD.read_RTD_temp_F();
      float temperatureF = atlasTemp;
      lcd.setCursor(0, 0);
      lcd.print("Temp:");
      lcd.print(temperatureF, 1);
      lcd.print(char(223)); // print degree
      lcd.print("F");
#endif
#ifndef DISABLE_MCP9701_TEMP
      float sensorValue = analogRead(MCP9701_temp_Pin);
      float voltage = sensorValue * (3.3 / 4095.0);
      float temperatureC = (voltage - 0.5) / 0.01;
      float fahrenheit = (temperatureC * 9.0) / 5.0 + 32;
      lcd.setCursor(0, 0);
      lcd.print("Temp:");
      lcd.print(fahrenheit, 1);
      lcd.print(char(223)); // print degree
      lcd.print("F");
#ifndef LESS_SERIAL_OUTPUT
      Serial.print("MCP9701 Temperature:\t\t\t");
      Serial.print(fahrenheit);
      Serial.print("\xC2\xB0"); // shows degree symbol
      Serial.println("F");
#endif
      float temperatureF = fahrenheit;
#endif
    }
#ifdef ENABLE_ATLAS_pH
    lcd.setCursor(0, 1);
    lcd.print("pH:");
    lcd.print(atlasPH);
#endif
#ifdef ENABLE_ATLAS_EC
    lcd.setCursor(0, 2);
    lcd.print("EC:");
    lcd.print(atlasEC);
#endif
#ifndef DISABLE_WIFI
    lcd.setCursor(0, 3);
    lcd.print("RSSI:");
    lcd.print(WiFi.RSSI());
    lcd.print("dBm");
#endif
#ifdef ENABLE_DHT11_HUMIDITY
    lcd.setCursor(10, 0);
    lcd.print("Humid:");
    lcd.print(readDHT11humidity(), 0);
    lcd.print("%");
#endif
#ifndef DISABLE_UNIT_DISPLAY
    lcd.setCursor(10, 1);
    lcd.print("Unit #");
    lcd.print(UNIT_NUMBER);
    // lcd.print("");
#endif
  }
  if (pageNum == 1)
  {
    lcd.setCursor(0, 0);
    if (WiFi.status() == WL_CONNECTED)
    {
      lcd.print("Connected to WiFi:");
    }
    else
    {
      lcd.print("Not Connected to: ");
    }
    lcd.setCursor(0, 1);

    lcd.print(WiFi.SSID());
    lcd.setCursor(0, 2);
    lcd.print(WiFi.localIP());
    lcd.setCursor(0, 3);
    lcd.print(WiFi.macAddress());
  }
  if (pageNum == 2)
  {
    int row_count = 0;
#ifdef ENABLE_DHT11_TEMP
    lcd.setCursor(0, row_count++);
    lcd.print("Temp DHT:");
    // char tempStr[3];
    // dtostrf(lcd_temp,3,3,tempStr);
    lcd.print(readDHT11Temp(), 0);
    lcd.print(char(223)); // print degree
    lcd.print("F");
#endif
#ifdef ENABLE_ATLAS_TEMP
    // float temperatureF = RTD.read_RTD_temp_F();
    float temperatureF = atlasTemp;
    lcd.setCursor(0, row_count++);
    lcd.print("Temp Atlas:");
    lcd.print(temperatureF, 1);
    lcd.print(char(223)); // print degree
    lcd.print("F");
#endif
#ifndef DISABLE_MCP9701_TEMP
    float sensorValue = analogRead(MCP9701_temp_Pin);
    float voltage = sensorValue * (3.3 / 4095.0);
    float temperatureC = (voltage - 0.5) / 0.01;
    float fahrenheit = (temperatureC * 9.0) / 5.0 + 32;
    lcd.setCursor(0, row_count++);
    lcd.print("Temp 9701:");
    lcd.print(fahrenheit, 1);
    lcd.print(char(223)); // print degree
    lcd.print("F");
    float temperatureF = fahrenheit;
#endif
  }
}
#ifdef ENABLE_PUMPS
// bool process_coms(const String &string_buffer)
// { // function to process commands that manipulate global variables and are specifc to certain kits
//   if (string_buffer == "HELP")
//   {
//     print_help();
//     return true;
//   }
//   else if (string_buffer.startsWith("POLL"))
//   {
//     polling = true;
//     PumpSeq.reset();

//     int16_t index = string_buffer.indexOf(','); // check if were passing a polling delay parameter
//     if (index != -1)
//     {                                                                 // if there is a polling delay
//       float new_delay = string_buffer.substring(index + 1).toFloat(); // turn it into a float

//       float mintime = reading_delay;
//       if (new_delay >= (mintime / 1000.0))
//       {                                                               // make sure its greater than our minimum time
//         PumpSeq.set_step2_time((new_delay * 1000.0) - reading_delay); // convert to milliseconds and remove the reading delay from our wait
//       }
//       else
//       {
//         Serial.println("delay too short"); // print an error if the polling time isnt valid
//       }
//     }
//     return true;
//   }
//   return false; // return false if the command is not in the list, so we can scan the other list or pass it to the circuit
// }

void print_help()
{
  Serial.println(F("Atlas Scientific Tri PMP sample code                                       "));
  Serial.println(F("Commands:                                                                  "));
  Serial.println(F("poll         Takes readings continuously of all sensors                    "));
  Serial.println(F("                                                                           "));
  Serial.println(F("PMP[N]:[query]       issue a query to a pump named PMP[N]                  "));
  Serial.println(F("  ex: PMP2:status    sends the status command to pump named PMP2           "));
  Serial.println(F("      PMP1:d,100     requests that PMP1 dispenses 100ml                    "));
  Serial.println();
  Serial.println(F("      The list of all pump commands is available in the Tri PMP datasheet  "));
  Serial.println();
  iot_cmd_print_listcmd_help();
  Serial.println();
  iot_cmd_print_allcmd_help();
}
#endif