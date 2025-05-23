/*
MIT License

Copyright (c) 2024 Atlas Scientific

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE
*/

#ifndef BASE_SURVEYOR_H
#define BASE_SURVEYOR_H

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

class Surveyor_Base
{
public:
    enum Surveyor_type
    {
        SURVEYOR_PH = 1,
        SURVEYOR_DO,
        SURVEYOR_ORP,
        SURVEYOR_RTD
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