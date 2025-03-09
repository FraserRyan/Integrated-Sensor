#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <SPI.h>
#include <SD.h>
#include "FS.h"

#include "SD_MMC.h" // Include SD_MMC for SDIO this is going to be the first approach with the SD card the function 


#include "headers.h"

#include "time.h"
// new #include <DS1307RTC.h>

File myFile;

#ifdef USE_PULSE_OUT
#include "ph_iso_surveyor.h"
Surveyor_pH_Isolated pH = Surveyor_pH_Isolated(A0);
#else
#include "ph_surveyor.h"

#define SD_CS_PIN 5 // Example: Using GPIO5 for CS
#define SD_CARD_DETECT_PIN 3 // Example: Using GPIO3 for card detect




Surveyor_pH pH = Surveyor_pH(pH_Pin);
#endif

uint8_t user_bytes_received = 0;
const uint8_t bufferlen = 32;
char user_data[bufferlen];


// Moved to headers.h, but to support older versions before this was moved.
#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.path(), levels - 1);
      }
    }
    else
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char *path)
{
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path))
  {
    Serial.println("Dir created");
  }
  else
  {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char *path)
{
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path))
  {
    Serial.println("Dir removed");
  }
  else
  {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available())
  {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print("Environmental Data:\n"))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    Serial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
  }
  file.close();
}
double pHData;
double tempData;

void LogData(String requestBody)
{
  pHData = pH.read_ph(); // I think this pH data isnt even required because the pH sensor data gets passed through the requestBody
  //  readFile(SD, "/Environmental_Data.txt");

  myFile = SD.open("/Environmental_Data.txt", FILE_APPEND);
  if (myFile)
  {
    Serial.println("File opened with sucess");
    myFile.println(requestBody);
  }
  myFile.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2)
{
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2))
  {
    Serial.println("File renamed");
  }
  else
  {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char *path)
{
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path))
  {
    Serial.println("File deleted");
  }
  else
  {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char *path)
{
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file)
  {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len)
    {
      size_t toRead = len;
      if (toRead > 512)
      {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %lu ms\n", flen, end);
    file.close();
  }
  else
  {
    Serial.println("Failed to open file for reading");
  }

  file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++)
  {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %lu ms\n", 2048 * 512, end);
  file.close();
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

void setup()
{

// For SD card Experimental: these pins havent actually been configured in the header yet

// This function is a bool so I dont know if I have to put true as a parameter or instead of SD_MMC 
SD_MMC.setPins(
  SD_MMC_CLK_PIN,
  SD_MMC_CMD_PIN,
  SD_MMC_DAT0_PIN,
  SD_MMC_DAT1_PIN,
  SD_MMC_DAT2_PIN,
  SD_MMC_DAT3_PIN
);


  Wire.begin(SDA_PIN, SCL_PIN);

  pinMode(LED12, OUTPUT);
  pinMode(LED13, OUTPUT);
  pinMode(LED14, OUTPUT);
  pinMode(LED15, OUTPUT);
  pinMode(LED16, OUTPUT);
  pinMode(LED17, OUTPUT);

  Serial.begin(115200);
  Serial.println(WiFi.macAddress());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  int wait_time = 0;
  while ((WiFi.status() != WL_CONNECTED) && (wait_time < 10))
  {
    Serial.print('.');
    delay(500);
    wait_time++;
    if (wait_time > 18)
    {
      Serial.print("WiFi Failed to connect.");
    }
    // Serial.print(wait_time);
  }
  Serial.println(WiFi.localIP());
  // This will get and print the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);

  SD.begin();
  // File file = SD.open("/Environmental_Data.txt");
  // if (!file) {
  //   writeFile(SD, "Environmental_Data.txt","DATA:");
  //   // Serial.println("Failed to open file for reading");
  //   // return;
  // }
  // file.close();
}

unsigned int lastTime = 0;
// int delayTime = 45 * 1000;

int counter = 0;

void loop()
{

  digitalWrite(LED12, HIGH); // turn the LED on (HIGH is the voltage level)
  digitalWrite(LED13, HIGH); // turn the LED on (HIGH is the voltage level)
  digitalWrite(LED14, HIGH); // turn the LED on (HIGH is the voltage level)
  digitalWrite(LED15, HIGH); // turn the LED on (HIGH is the voltage level)
  digitalWrite(LED16, HIGH); // turn the LED on (HIGH is the voltage level)
  digitalWrite(LED17, HIGH); // turn the LED on (HIGH is the voltage level)

  delay(300); // wait for a moment
  digitalWrite(LED12, LOW);
  digitalWrite(LED13, LOW);
  digitalWrite(LED14, LOW);
  digitalWrite(LED15, LOW);
  digitalWrite(LED16, LOW);
  digitalWrite(LED17, LOW);
  delay(300); // wait for a moment

  double reading = analogRead(temp_Pin);
  float voltage = reading * (3.3 / 4096.0);
  float temperatureC = (voltage - 0.5) * 100;
  // Serial.println("Voltage operated: " + String(reading));
  // Serial.println("New Raw Temperature Custom: " + String(analogRead(Custom_Temp_Analogpin)));
  Serial.print(temperatureC);
  Serial.print("\xC2\xB0"); // shows degree symbol
  Serial.print("C  |  ");
  float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
  Serial.print(temperatureF);
  Serial.print("\xC2\xB0"); // shows degree symbol
  Serial.println("F");
  // Serial.println("pH: "          + String(pH.read_ph()));
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
  display.print(" ");
  display.display();

#ifndef DISABLE_API_REQUEST
  if (counter == 0 || ((millis() - lastTime) > delayTime))
  {
    display.fillTriangle(106, 49, 121, 54, 106, 58, 1);
    display.display();
    HTTPClient http;
    WiFiClient client;

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
    requestBody += ",\"id\": \"" + apiId + String("\",\"key\": \"") + apiKey + String("\"");
    requestBody += "}";

    Serial.println("Request Body:");
    Serial.println(requestBody);
    int httpResponseCode = http.POST(requestBody.c_str());

    Serial.print("httpResponseCode: ");
    Serial.println(httpResponseCode);

    lastTime = millis();
    if (httpResponseCode == 200)
    {
      display.fillTriangle(106, 49, 121, 54, 106, 58, 0);
      display.display();
    }
    else
    {
      LogData(requestBody);
    }
  }
#endif
}
