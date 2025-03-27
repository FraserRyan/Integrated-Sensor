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



Surveyor_RTD RTD = Surveyor_RTD(A1_temp_Pin);
Surveyor_pH pH = Surveyor_pH(pH_Pin);

uint8_t user_bytes_received = 0;
const uint8_t bufferlen = 32;
char user_data[bufferlen];

void parse_cmd(char* string);
void printLocalTime();
void startOnDemandWiFiManager();
void saveWMConfig();

void setup() {
  config.begin("config");
  UNIT_NUMBER = config.getInt("unit_number", 0);
  strcpy(apiKey, config.getString("api_key", "").c_str());
  strcpy(apiId, config.getString("api_id", "").c_str());
  config.end();
  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.begin(115200);
  Serial.println(F("Use command \"CAL,nnn.n\" to calibrate the circuit to a specific temperature\n\"CAL,CLEAR\" clears the calibration"));
  if(RTD.begin()){
    Serial.println("Loaded EEPROM");
  }

  Serial.println(WiFi.macAddress());
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

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
      display.display();
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
}
unsigned int lastTime = 0;

int counter = 0;

int onDemandManagerTrigger = false;
int lastButtonPress = 0;
int didLastManager = 0;
int button_held_wifi_manager = 0;

void loop() {

  if (Serial.available() > 0) {
    user_bytes_received = Serial.readBytesUntil(13, user_data, sizeof(user_data));
  }

  if (user_bytes_received) {
    parse_cmd(user_data);
    user_bytes_received = 0;
    memset(user_data, 0, sizeof(user_data));
  }
  
  //Serial.println(RTD.read_RTD_temp_C());
  
  //uncomment for readings in F
  //Serial.println(RTD.read_RTD_temp_F()); 
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

  float temperatureF = RTD.read_RTD_temp_F();
  display.clearDisplay();
  // temperature Display on OLED
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Temperature: ");
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.print(String(temperatureF));

  display.print(" ");
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(2);
  display.print("F");
  // pH Display on OLED
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("pH: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print(String(pH.read_ph()));
  display.println(" ");

  Serial.print(" RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println("dBm");
  Serial.print(temperatureF);
  Serial.print("\xC2\xB0"); // shows degree symbol
  Serial.println("F");
  Serial.print("pH: ");
  Serial.println(pH.read_ph());

  display.setCursor(100, 44);
  display.setTextSize(1);
  display.println("RSSI");
  display.setCursor(82, 54);
  display.print(WiFi.RSSI());
  display.print("dBm");

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

    float average = temperatureF;

    http.begin(client, envDataRequestURL.c_str());

    http.addHeader("Content-Type", "application/json");

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
      //LogData(requestBody);
    }
  }
#endif

}

void parse_cmd(char* string) {
  strupr(string);
  String cmd = String(string);
  if(cmd.startsWith("CAL")){
    int index = cmd.indexOf(',');
    if(index != -1){
      String param = cmd.substring(index+1, cmd.length());
      if(param.equals("CLEAR")){
        RTD.cal_clear();
        Serial.println("CALIBRATION CLEARED");
      }else {
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
