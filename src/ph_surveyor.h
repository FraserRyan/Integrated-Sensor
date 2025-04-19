
#ifndef PH_SURVEYOR_H
#define PH_SURVEYOR_H

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <base_surveyor.h>

class Surveyor_pH : public Surveyor_Base{
	public:
	
		Surveyor_pH(uint8_t pin);
		
		bool begin();
	
		virtual float read_voltage();
		float read_ph(float voltage_mV);
		float read_ph();
		
		void cal_mid(float voltage_mV);
		void cal_mid();
		
		void cal_low(float voltage_mV);
		void cal_low();
		
		void cal_high(float voltage_mV);
		void cal_high();
	
		void cal_clear();

		float getLowCalibration() { return pH.low_cal; }
		float getMidCalibration() { return pH.mid_cal; }
		float getHighCalibration() { return pH.high_cal; }
		
	    void print_calibration_values() {
			Serial.print("Low Calibration: ");
			Serial.println(getLowCalibration());
			Serial.print("Mid Calibration: ");
			Serial.println(getMidCalibration());
			Serial.print("High Calibration: ");
			Serial.println(getHighCalibration());
		}

	private:
		
		struct PH {
		  const uint8_t magic = magic_char;
          const uint8_t type = SURVEYOR_PH;
		  float mid_cal = 1500;
		  float low_cal = 2030;
		  float high_cal = 975;
		};
		struct PH pH;
};

#endif