

#ifndef BASE_SURVEYOR_H
#define BASE_SURVEYOR_H

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

class Surveyor_Base{
	public:
        enum Surveyor_type{
            SURVEYOR_PH = 1,
            SURVEYOR_DO,
            SURVEYOR_ORP,
            SURVEYOR_RTD = 4
        };
        
        virtual bool begin();
	
		virtual float read_voltage();

    protected:
	
		uint8_t pin = A0;
        
		static const int volt_avg_len = 1000;
        static const uint8_t EEPROM_SIZE_CONST = 16;
		static const uint8_t magic_char = 0xAA;
        
        int16_t EEPROM_offset = 0;
};

#endif
