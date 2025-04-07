#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "headers.h"
#include "time.h"
#include "ph_surveyor.h"
#include "rtd_surveyor.h"
#include <sequencer1.h>               //imports a 1 function sequencer
#include <sequencer2.h>               //imports a 2 function sequencer
#include <Ezo_i2c.h>                  //include the EZO I2C library from https://github.com/Atlas-Scientific/Ezo_I2c_lib
#include <Wire.h>                     //include arduinos i2c library
#include <Ezo_i2c_util.h>             //brings in common print statements
#include <Adafruit_Sensor.h>          //Library for Adafruit sensors
#include <DHT.h>                      // Sensor for Humidity and temperature.
#include "FreeSerifBoldItalic9pt7b.h" // For the cool font at the startup

char EC_data[32]; // we make a 32-byte character array to hold incoming data from the EC sensor.
char *EC_str;     // char pointer used in string parsing.
char *TDS;        // char pointer used in string parsing.
char *SAL;        // char pointer used in string parsing (the sensor outputs some text that we don't need).
char *SG;         // char pointer used in string parsing.

float EC_float;  // float var used to hold the float value of the conductivity.
float TDS_float; // float var used to hold the float value of the total dissolved solids.
float SAL_float; // float var used to hold the float value of the salinity.
float SG_float;  // float var used to hold the float value of the specific gravity.

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

void step1(); // forward declarations of functions to use them in the sequencer before defining them
void step2();
void parse_cmd(char *string);
void printLocalTime();
void startOnDemandWiFiManager();
void saveWMConfig();

// #ifndef DISABLE_DHT11_TEMP // && DISABLE_DHT11_HUMIDITY
#if !defined(DISABLE_DHT11_TEMP) || !defined(DISABLE_DHT11_HUMIDITY)
DHT dht(DHT11PIN, DHTTYPE);
float readDHT11Temp();
float readDHT11humidity();
#endif

void updateDisplay()
{
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setFont(&FreeSerifBoldItalic9pt7b);
  display.setCursor(35, 20);
  display.println("Ryan Fraser Josh Thaw");
  display.display();
  display.setFont();
}

void setup()
{
  pinMode(LED15, OUTPUT);

  config.begin("config");
  UNIT_NUMBER = config.getInt("unit_number", 0);
  strcpy(apiKey, config.getString("api_key", "").c_str());
  strcpy(apiId, config.getString("api_id", "").c_str());
  config.end();
  Wire.begin(SDA_PIN, SCL_PIN);
  Seq.reset(); // initialize the sequencer
  delay(3000);
  EC.send_cmd("o,tds,1"); // send command to enable TDS output
  delay(300);
  EC.send_cmd("o,s,1"); // send command to enable salinity output
  delay(300);
  EC.send_cmd("o,sg,1"); // send command to enable specific gravity output
  delay(300);

  Serial.begin(115200);
#ifndef CALIBRATION_MODE
  Serial.println(F("Use command \"CAL,nnn.n\" to calibrate the circuit to a specific temperature\n\"CAL,CLEAR\" clears the calibration"));
  if (RTD.begin())
  {
    Serial.println("Loaded EEPROM");
  }
#endif

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  updateDisplay();
  delay(2500);

#if !defined(DISABLE_DHT11_TEMP) || !defined(DISABLE_DHT11_HUMIDITY)
  dht.begin();
#endif

#ifndef DISABLE_WIFI
  Serial.println(WiFi.macAddress());
  WiFi.mode(WIFI_STA);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.display();
  display.setCursor(0, 0);
  display.println(WiFi.macAddress());
  if (wm.getWiFiIsSaved())
  {
    display.println("Connecting to previously saved WiFi network:");
    display.println(wm.getWiFiSSID());
    display.display();
    WiFi.begin(wm.getWiFiSSID(), wm.getWiFiPass());
    Serial.print("Connecting to Saved WiFi ..");
    int wait_time = 0;
    while ((WiFi.status() != WL_CONNECTED) && (wait_time < 10))
    {
      Serial.print('.');
      display.print(".");
      digitalWrite(LED15, HIGH);
      display.display();
      delay(500);
      digitalWrite(LED15, LOW);
      delay(500);
      wait_time++;

      if (wait_time > 18)
      {
        Serial.print("WiFi Failed to connect.");
        display.println();
        display.println("WiFi failed to Connect.\nPress the BOOT button for 3 seconds to start the config.");
        display.display();
      }
      // Serial.print(wait_time);
    }
    Serial.println(WiFi.localIP());
    display.println(WiFi.localIP());
    display.display();
  }
  else
  {
    display.println("No WiFi saved.");
    display.println("Press the BOOT button for 3 seconds to start the config.");
    display.display();
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  printLocalTime();
  delay(3000);

  display.clearDisplay();

  pinMode(WIFIMANAGER_TRIGGER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WIFIMANAGER_TRIGGER_PIN), startOnDemandWiFiManager, RISING);
  wm.addParameter(&unit_number_param);
  wm.addParameter(&api_id_param);
  wm.addParameter(&api_key_param);
#endif
#ifdef DISABLE_WIFI
  WiFi.mode(WIFI_MODE_NULL);

#endif

  display.setTextColor(WHITE);
  display.clearDisplay();
}
unsigned int lastTime = 0;

int counter = 0;

int onDemandManagerTrigger = false;
int lastButtonPress = 0;
int didLastManager = 0;
int button_held_wifi_manager = 0;

void loop()
{

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
    display.setTextSize(1);
    Serial.println("Button held for WM, starting config portal");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    String AP_Name = "ESP_UNIT_";
    AP_Name += String(UNIT_NUMBER);
    display.println("Starting Configuration. Join WiFi:");
    display.println(AP_Name);
    display.println("fa9s8dS7d92J");
    display.println("And go to 192.168.4.1");
    display.display();
    wm.setConfigPortalTimeout(wifiManagerTimeout);
    wm.setBreakAfterConfig(true);
    wm.setSaveConfigCallback(saveWMConfig);

    if (!wm.startConfigPortal(AP_Name.c_str(), "fa9s8dS7d92J"))
    {
    }
  }
#endif

#ifndef DISABLE_ATLAS_EC
  Seq.run(); // run the sequncer to do the polling
#endif

  display.clearDisplay();
  // temperature Display on OLED
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("TEMP: ");
  display.setTextSize(2);
  display.setCursor(0, 10);

#ifndef DISABLE_ATLAS_TEMP
  float temperatureF = RTD.read_RTD_temp_F();
  Serial.print(temperatureF);
  display.print(temperatureF, 1);
  display.setTextSize(2);
  display.print("F");
#endif
#ifndef DISABLE_MCP9701_TEMP
  float sensorValue = analogRead(MCP9701_temp_Pin);
  float voltage = sensorValue * (3.3 / 4095.0);
  float temperatureC = (voltage - 0.5) / 0.01;
  float fahrenheit = (temperatureC * 9.0) / 5.0 + 32;
  display.print(fahrenheit, 1);
  display.setTextSize(2);
  display.print("F");
  Serial.print("MCP9701 Temperature:\t\t\t");
  Serial.print(fahrenheit);
  Serial.print("\xC2\xB0"); // shows degree symbol
  Serial.println("F");
  float temperatureF = fahrenheit;
#endif
#if defined DISABLE_ATLAS_TEMP && defined DISABLE_MCP9701_TEMP
  display.print("-");
#endif

  // Serial.print(temperatureF);
  // display.print(" ");
  // display.setTextSize(1);
  // display.cp437(true);
  // display.write(167);
  // display.print(fahrenheit);

  // pH Display on OLED
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("pH: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
#ifndef DISABLE_ATLAS_pH
  display.print(pH.read_ph(), 1);
#else
  display.print("-");
#endif
  display.println(" ");

  // Serial.print("RSSI: \t");
  Serial.print("Received Signal Strength Indicator:\t");
  Serial.print(WiFi.RSSI());
  Serial.println("dBm");

#ifndef DISABLE_ATLAS_pH
  Serial.print("pH: ");
  Serial.println(pH.read_ph(), 1);
#endif

  //
  display.setTextSize(1);
  display.setCursor(60, 0);
  display.println("RSSI:");
  display.setCursor(90, 0);
#ifndef DISABLE_WIFI
  display.print(WiFi.RSSI());
  display.print("dBm");
#else
  display.print("-");
#endif

  display.setCursor(72, 34);
  display.setTextSize(1);
  display.println("EC:");
  display.setCursor(72, 44);
  display.setTextSize(2);
#ifndef DISABLE_ATLAS_EC
  display.print(EC_float / 1000, 1);
#else
  display.print("-");
#endif
  // display.print("mS/cm");

#ifndef DISABLE_DHT11_TEMP
  // float DHT_tempF = readDHTTemp();
  Serial.print("DHT Temperature: \t\t\t");
  Serial.print(readDHT11Temp());
  Serial.print("\xC2\xB0"); // shows degree symbol
  Serial.print("F\t");
// display.print(/1000,1);
#endif

#ifndef DISABLE_DHT11_HUMIDITY
  // readDHT11humidity();
  Serial.print("\nHumidity: \t\t\t\t");
  Serial.print(readDHT11humidity());
  Serial.println(" %");
// display.print(/1000,1);
#endif
  Serial.println("-----------------------------------------------");
  display.display();

#ifndef DISABLE_API_REQUEST
  // Serial.println(apiId);
  if ((counter == 0 || ((millis() - lastTime) > delayTime)) && UNIT_NUMBER != 0 && strcmp(apiId, "") != 0 && strcmp(apiKey, "") != 0)
  {
    display.fillTriangle(106, 10, 121, 15, 106, 23, 1);
    display.display();
    HTTPClient http;
    WiFiClientSecure client;
    client.setCACert(root_ca);

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
#if !defined(DISABLE_DHT11_HUMIDITY)
    requestBody += ",\"humidity\":" + String(readDHT11humidity());
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
      display.fillTriangle(106, 10, 121, 15, 106, 23, 0);
      display.display();
    }
    else
    {
      // LogData(requestBody);
    }
  }
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
  display.setTextSize(1);
  Serial.println("Button held for WM, starting config portal");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  String AP_Name = "ESP_UNIT_";
  AP_Name += String(UNIT_NUMBER);
  display.println("Starting Configuration. Join WiFi:");
  display.println(AP_Name);
  display.println("fa9s8dS7d92J");
  display.println("And go to 192.168.4.1");
  display.display();
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(wifiManagerTimeout);
  api_id_param.setValue(apiId, 40);
  api_key_param.setValue(apiKey, 40);
  unit_number_param.setValue(String(UNIT_NUMBER).c_str(), 4);

  if (!wm.startConfigPortal(AP_Name.c_str(), "fa9s8dS7d92J"))
  {
    Serial.println("failed to connect or hit timeout");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Failed to get configuration.");
  }
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
#if !defined(DISABLE_DHT11_TEMP) || !defined(DISABLE_DHT11_HUMIDITY)
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

#if !defined(DISABLE_DHT11_TEMP) || !defined(DISABLE_DHT11_HUMIDITY)
float readDHT11humidity()
{
  float humidity = dht.readHumidity();
#ifndef DISABLE_DHT11_HUMIDITY
  return humidity;
#endif

  // Serial.print("Humidity: ");
  // Serial.print(h);
  // Serial.println(" %");
}
#endif