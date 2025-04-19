#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "ph_surveyor.h"
#include "EEPROM.h"

// Constructor
Surveyor_pH::Surveyor_pH(uint8_t pin)
{
  this->pin = pin;
  this->EEPROM_offset = (pin) * EEPROM_SIZE_CONST;

  
}

// Method definition to print calibration values (in the .cpp file)
// void Surveyor_pH::print_calibration_values() {
//   Serial.print("Low Calibration: ");
//   Serial.println(this->pH.low_cal);
//   Serial.print("Mid Calibration: ");
//   Serial.println(this->pH.mid_cal);
//   Serial.print("High Calibration: ");
//   Serial.println(this->pH.high_cal);
// }

bool Surveyor_pH::begin()
{
#if defined(ESP8266) || defined(ESP32)
  EEPROM.begin(1024);
#endif
  if ((EEPROM.read(this->EEPROM_offset) == magic_char) && (EEPROM.read(this->EEPROM_offset + sizeof(uint8_t)) == SURVEYOR_PH))
  {
    EEPROM.get(this->EEPROM_offset, pH);
    return true;
  }
  return false;
}

float Surveyor_pH::read_voltage()
{
  float voltage_mV = 0;
  for (int i = 0; i < volt_avg_len; ++i)
  {
#if defined(ESP32)
    voltage_mV += analogRead(this->pin) / 4095.0 * 3300.0 + 130;
#else
    voltage_mV += analogRead(this->pin) / 1024.0 * 5000.0;
#endif
  }
  voltage_mV /= volt_avg_len;
  return voltage_mV;
}

float Surveyor_pH::read_ph(float voltage_mV)
{
  if (voltage_mV > pH.mid_cal)
  { 
    return 7.0 - 3.0 / (this->pH.low_cal - this->pH.mid_cal) * (voltage_mV - this->pH.mid_cal);
  }
  else
  {
    return 7.0 - 3.0 / (this->pH.mid_cal - this->pH.high_cal) * (voltage_mV - this->pH.mid_cal);
  }
}

float Surveyor_pH::read_ph()
{
  return (read_ph(read_voltage()));
}

void Surveyor_pH::cal_mid(float voltage_mV)
{
  this->pH.mid_cal = voltage_mV;
  EEPROM.put(this->EEPROM_offset, pH);
#if defined(ESP8266) || defined(ESP32)
  EEPROM.commit();
#endif
}

void Surveyor_pH::cal_mid()
{
  cal_mid(read_voltage());
}

void Surveyor_pH::cal_low(float voltage_mV)
{
  this->pH.low_cal = voltage_mV;
  EEPROM.put(this->EEPROM_offset, pH);
#if defined(ESP8266) || defined(ESP32)
  EEPROM.commit();
#endif
}

void Surveyor_pH::cal_low()
{
  cal_low(read_voltage());
}

void Surveyor_pH::cal_high(float voltage_mV)
{
  this->pH.high_cal = voltage_mV;
  EEPROM.put(this->EEPROM_offset, pH);
#if defined(ESP8266) || defined(ESP32)
  EEPROM.commit();
#endif
}

void Surveyor_pH::cal_high()
{
  cal_high(read_voltage());
}

void Surveyor_pH::cal_clear()
{
  this->pH.mid_cal = 1500;
  this->pH.low_cal = 2030;
  this->pH.high_cal = 975;
  EEPROM.put(this->EEPROM_offset, pH);
}

// void Surveyor_pH::print_calibration_values() {
//   Serial.print("Low Calibration: ");
//   Serial.println(this->pH.low_cal);
//   Serial.print("Mid Calibration: ");
//   Serial.println(this->pH.mid_cal);
//   Serial.print("High Calibration: ");
//   Serial.println(this->pH.high_cal);
// }
