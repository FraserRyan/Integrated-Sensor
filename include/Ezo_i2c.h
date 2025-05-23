/*
MIT License

Copyright (c) 2019 Atlas Scientific

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

#ifndef EZO_I2C_H
#define EZO_I2C_H

#include "Arduino.h"

#include <Wire.h>

class Ezo_board
{
public:
	// errors
	enum errors
	{
		SUCCESS,
		FAIL,
		NOT_READY,
		NO_DATA,
		NOT_READ_CMD
	};

	// constructors
	Ezo_board(uint8_t address);					  // Takes I2C address of the device
	Ezo_board(uint8_t address, const char *name); // Takes I2C address of the device
												  // as well a name of your choice
	Ezo_board(uint8_t address, TwoWire *wire);					 // Takes I2C address and TwoWire interface
	Ezo_board(uint8_t address, const char *name, TwoWire *wire); // Takes all 3 parameters

	void send_cmd(const char *command);
	// send any command in a string, see the devices datasheet for available i2c commands

	void send_read_cmd();
	// sends the "R" command to the device and sets issued_read() to true,
	// so we know to parse the data when we receive it with receive_read()

	void send_cmd_with_num(const char *cmd, float num, uint8_t decimal_amount = 3);
	// sends any command with the number appended as a string afterwards.
	// ex. PH.send_cmd_with_num("T,", 25.0); will send "T,25.000"

	void send_read_with_temp_comp(float temperature);
	// sends the "RT" command with the temperature converted to a string
	// to the device and sets issued_read() to true,
	// so we know to parse the data when we receive it with receive_read()

	enum errors receive_cmd(char *sensordata_buffer, uint8_t buffer_len);
	// receive the devices response data into a buffer you supply.
	// Buffer should be long enough to hold the longest command
	// you'll receive. We recommand 32 bytes/chars as a default

	enum errors receive_read_cmd();
	// gets the read response from the device, and parses it into the reading variable
	// if send_read() wasn't used to send the "R" command and issued_read() isnt set, the function will
	// return the "NOT_READ_CMD" error

	bool is_read_poll();
	// function to see if issued_read() was set.
	// Useful for determining if we should call receive_read() (if is_read_poll() returns true)
	// or recieve_cmd() if is_read_poll() returns false)

	float get_last_received_reading();
	// returns the last reading the device received as a float

	const char *get_name();
	// returns a pointer to the name string. This gets the name given to object by the constructor
	// not the device name returned by the "name,?" command.
	// To get the device name use [device].send_cmd("name,?") and read the response with receive_cmd

	void set_name(const char *name);
	// set a pointer to the name string. This sets the name of the object, not the device itself.
	// if you want to rename the device use [device].send_cmd("name,THE_DEVICE_NAME")

	enum errors get_error();
	// returns the error status of the last received command,
	// used to check the validity of the data returned by get_reading()

	uint8_t get_address();
	// returns the address of the device

	void set_address(uint8_t address);
	// lets you change the I2C address of the device

protected:
	uint8_t i2c_address;
	const char *name = 0;
	float reading = 0;
	bool issued_read = false;
	enum errors error;
	const static uint8_t bufferlen = 32;
	TwoWire *wire = &Wire;
};

#endif