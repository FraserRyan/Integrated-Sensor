This is the header config for a board with no connection to any sensors:
//#define ENABLE_ATLAS_EC
//#define ENABLE_ATLAS_TEMP
//#define ENABLE_ATLAS_pH
#define DISABLE_CELSIUS
#define DHTTYPE DHT11
//#define ENABLE_CALIBRATION
//#define DISABLE_FAHRENHEIT
//#define ENABLE_DHT11_TEMP
//#define ENABLE_DHT11_HUMIDITY
#define DISABLE_MCP9701_TEMP
//#define DISABLE_WIFI
//#define DISABLE_API_REQUEST
// #define ENABLE_GPS
#define ENABLE_SETPOINT_FETCH
#define LESS_SERIAL_OUTPUT
#define LCD_ADDR 0x27  // Define I2C Address where the PCF8574A (Blue blacklight)
//#define LCD_ADDR 0x3F //This is for the LCD without a backlight
#define ENABLE_OLED_DISPLAY
//#define ENABLE_PUMPS
//#define ENABLE_PH_UP
//#define ENABLE_PH_DOWN






Here is pin mapping for ESP32-S3
Do not use strapping pins
https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html

Strapping pin: GPIO0, GPIO3, GPIO45 and GPIO46 are strapping pins. For more information, please refer to ESP32-S3 datasheet.

SPI0/1: GPIO26 ~ GPIO32 are usually used for SPI flash and PSRAM and not recommended for other uses. 
When using Octal flash or Octal PSRAM or both, GPIO33 ~ GPIO37 are connected to SPIIO4 ~ SPIIO7 and SPIDQS. 
Therefore, on boards embedded with ESP32-S3R8 / ESP32-S3R8V chip, GPIO33 ~ GPIO37 are also not recommended for other uses.

USB-JTAG: GPIO19 and GPIO20 are used by USB-JTAG by default. 
If they are reconfigured to operate as normal GPIOs, USB-JTAG functionality will be disabled.


| GPIO   | Analog Function | RTC GPIO     | Comment         |
|--------|------------------|--------------|-----------------|
| GPIO0  |                  | RTC_GPIO0    | Strapping pin   |
| GPIO1  | ADC1_CH0         | RTC_GPIO1    |                 |
| GPIO2  | ADC1_CH1         | RTC_GPIO2    |                 |
| GPIO3  | ADC1_CH2         | RTC_GPIO3    | Strapping pin   |
| GPIO4  | ADC1_CH3         | RTC_GPIO4    |                 |
| GPIO5  | ADC1_CH4         | RTC_GPIO5    |                 |
| GPIO6  | ADC1_CH5         | RTC_GPIO6    |                 |
| GPIO7  | ADC1_CH6         | RTC_GPIO7    |                 |
| GPIO8  | ADC1_CH7         | RTC_GPIO8    |                 |
| GPIO9  | ADC1_CH8         | RTC_GPIO9    |                 |
| GPIO10 | ADC1_CH9         | RTC_GPIO10   |                 |
| GPIO11 | ADC2_CH0         | RTC_GPIO11   |                 |
| GPIO12 | ADC2_CH1         | RTC_GPIO12   |                 |
| GPIO13 | ADC2_CH2         | RTC_GPIO13   |                 |
| GPIO14 | ADC2_CH3         | RTC_GPIO14   |                 |
| GPIO15 | ADC2_CH4         | RTC_GPIO15   |                 |
| GPIO16 | ADC2_CH5         | RTC_GPIO16   |                 |
| GPIO17 | ADC2_CH6         | RTC_GPIO17   |                 |
| GPIO18 | ADC2_CH7         | RTC_GPIO18   |                 |
| GPIO19 | ADC2_CH8         | RTC_GPIO19   | USB-JTAG        |
| GPIO20 | ADC2_CH9         | RTC_GPIO20   | USB-JTAG        |
| GPIO21 |                  | RTC_GPIO21   |                 |
| GPIO26 |                  |              | SPI0/1          |
| GPIO27 |                  |              | SPI0/1          |
| GPIO28 |                  |              | SPI0/1          |
| GPIO29 |                  |              | SPI0/1          |
| GPIO30 |                  |              | SPI0/1          |
| GPIO31 |                  |              | SPI0/1          |
| GPIO32 |                  |              | SPI0/1          |
| GPIO33 |                  |              | SPI0/1          |
| GPIO34 |                  |              | SPI0/1          |
| GPIO35 |                  |              | SPI0/1          |
| GPIO36 |                  |              | SPI0/1          |
| GPIO37 |                  |              | SPI0/1          |
| GPIO38 |                  |              |                 |
| GPIO39 |                  |              |                 |
| GPIO40 |                  |              |                 |
| GPIO41 |                  |              |                 |
| GPIO42 |                  |              |                 |
| GPIO43 |                  |              |                 |
| GPIO44 |                  |              |                 |
| GPIO45 |                  |              | Strapping pin   |
| GPIO46 |                  |              | Strapping pin   |
| GPIO47 |                  |              |                 |
| GPIO48 |                  |              |                 |
