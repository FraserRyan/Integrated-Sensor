#ifndef PH_SURVEYOR_H
#define PH_SURVEYOR_H

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "EEPROM.h"

class Surveyor_pH
{
public:
  struct Calibration
  {
    float mid_cal;
    float low_cal;
    float high_cal;

  };

  Surveyor_pH(uint8_t pin);
  bool begin();
  float read_voltage();
  float read_ph();
  float read_ph(float voltage_mV);

  void print_calibration_values();
  void cal_mid(float voltage_mV);
  void cal_mid();
  void cal_low(float voltage_mV);
  void cal_low();
  void cal_high(float voltage_mV);
  void cal_high();
  void cal_clear();

  // Getter methods for calibration values
  float get_mid_cal() { return pH.mid_cal; }
  float get_low_cal() { return pH.low_cal; }
  float get_high_cal() { return pH.high_cal; }

private:
  uint8_t pin;
  int EEPROM_offset;
  Calibration pH;
  static const int volt_avg_len = 1000;
  static const uint8_t EEPROM_SIZE_CONST = 16;
  static const uint8_t magic_char = 0xAA;
};

#endif
