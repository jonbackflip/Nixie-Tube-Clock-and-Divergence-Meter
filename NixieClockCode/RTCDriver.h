// DS3231SN Driver function declarations
// for my Nixie Clock / Divergence Meter
// Written by Johnathan Guzman
// Finished 07/25/26

#ifndef RTCDRIVER_H
#define RTCDRIVER_H
#include <Arduino.h>

extern uint32_t currTime;
extern uint8_t currDay;
extern uint8_t currMonth;
extern uint8_t currYear;

uint8_t decToBCD(uint8_t val);
uint8_t BCDToDec(uint8_t val);

void setTimeI2C(uint8_t HR, uint8_t MIN, uint8_t SEC);
void setDateI2C(uint8_t MONTH, uint8_t DAY, uint8_t YEAR);

uint32_t readTimeI2C();
void readDateI2C();

void printTranslatedTime(uint32_t val);

#endif