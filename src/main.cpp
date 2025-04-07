//fka tri_pmp_sample_code.ino

#include <iot_cmd.h>
#include <sequencer2.h>                                          //imports a 4 function sequencer 
#include <Ezo_i2c_util.h>                                        //brings in common print statements
#include <Ezo_i2c.h> //include the EZO I2C library from https://github.com/Atlas-Scientific/Ezo_I2c_lib
#include <Wire.h>    //include arduinos i2c library
#include "headers.h"


#ifndef DISABLE_PERISTALTIC_PUMPS
Ezo_board PMP1 = Ezo_board(101, "PMP1");    //create an PMP circuit object who's address is 56 and name is "PMP1"
Ezo_board PMP2 = Ezo_board(102, "PMP2");    //create an PMP circuit object who's address is 57 and name is "PMP2"
Ezo_board PMP3 = Ezo_board(103, "PMP3");    //create an PMP circuit object who's address is 58 and name is "PMP3"

Ezo_board device_list[] = {               //an array of boards used for sending commands to all or specific boards
  PMP1,
  PMP2,
  PMP3
};

Ezo_board* default_board = &device_list[0]; //used to store the board were talking to

//gets the length of the array automatically so we dont have to change the number every time we add new boards
const uint8_t device_list_len = sizeof(device_list) / sizeof(device_list[0]);

const unsigned long reading_delay = 1000;                 //how long we wait to receive a response, in milliseconds
unsigned int poll_delay = 2000 - reading_delay;

void step1();      //forward declarations of functions to use them in the sequencer before defining them
void step2();
Sequencer2 Seq(&step1, reading_delay,   //calls the steps in sequence with time in between them
               &step2, poll_delay);

bool polling = false;                                     //variable to determine whether or not were polling the circuits
#endif

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);                          //start the I2C
  Serial.begin(115200);                     //start the serial communication to the computer
  #ifndef DISABLE_PERISTALTIC_PUMPS
  Seq.reset();
  #endif
}

void loop() {

  #ifndef DISABLE_PERISTALTIC_PUMPS
  getSetPoint();
  // if(AveragepH){
  //   Seq.run();
  // }
  Seq.run();
  //delay(50);
  #endif

}

#ifndef DISABLE_PERISTALTIC_PUMPS
void step1() {
  //getSetpoint():

  PMP1.send_cmd("d,1");
  PMP2.send_cmd("d,1");
  PMP3.send_cmd("d,1");
  
}

void step2() {
  receive_and_print_reading(PMP1);             //get the reading from the PMP1 circuit
  Serial.print("  ");
  receive_and_print_reading(PMP2);
  Serial.print("  ");
  receive_and_print_reading(PMP3);
  Serial.println();
}
#endif

void getSetPoint(){

}