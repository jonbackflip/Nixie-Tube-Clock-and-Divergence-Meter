// DS3231SN Driver function definitions
// for my Nixie Clock / Divergence Meter
// Written by Johnathan Guzman
// Finished 07/25/26

#include "RTCDriver.h"

#include <Wire.h>

uint32_t currTime;
uint8_t currDay;
uint8_t currMonth;
uint8_t currYear;

uint8_t decToBCD(uint8_t val) {
  return ((val / 10 * 16) + (val % 10));
}

uint8_t BCDToDec(uint8_t val) {
  return ((val / 16 * 10) + (val % 16));
}

////////// SET //////////

void setTimeI2C(uint8_t HR, uint8_t MIN, uint8_t SEC) {

  uint8_t BCD_SEC = decToBCD(SEC);
  uint8_t BCD_MIN = decToBCD(MIN);
  uint8_t BCD_HR  = decToBCD(HR) & ~0x40; // 1011 1111 to ensure 24 HR Mode (Bit 6 is 0)
  
  Wire.beginTransmission(0x68);
  Wire.write(0x00);
  Wire.write(BCD_SEC);
  Wire.write(BCD_MIN);
  Wire.write(BCD_HR);
  Wire.endTransmission();
}

void setDateI2C(uint8_t MONTH, uint8_t DAY, uint8_t YEAR) {

  uint8_t BCD_DAY   = decToBCD(DAY);
  uint8_t BCD_MONTH = decToBCD(MONTH);
  BCD_MONTH = BCD_MONTH % 128;     // Ignore Century Bit //
  uint8_t BCD_YEAR  = decToBCD(YEAR);

  Wire.beginTransmission(0x68);
  Wire.write(0x04);
  Wire.write(BCD_DAY);
  Wire.write(BCD_MONTH);
  Wire.write(BCD_YEAR);
  Wire.endTransmission();
}

////////// READ //////////

uint32_t readTimeI2C() {

  Wire.beginTransmission(0x68);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(0x68, 3);
  
  if (Wire.available() < 3) {
    return currTime;   // read failure
  }

  uint8_t BCD_SEC = Wire.read();
  uint8_t BCD_MIN = Wire.read();
  uint8_t BCD_HR  = Wire.read();

  return BCDToDec(BCD_HR) * 3600UL +
         BCDToDec(BCD_MIN) * 60UL +
         BCDToDec(BCD_SEC);
}

void readDateI2C() {

  Wire.beginTransmission(0x68);
  Wire.write(0x04);
  Wire.endTransmission();
  Wire.requestFrom(0x68, 3);

  currDay   = BCDToDec(Wire.read());
  currMonth = BCDToDec(Wire.read() & 0x7F); // 0111 1111 to get rid of the century bit
  currYear  = BCDToDec(Wire.read());
}

void printTranslatedTime(uint32_t val) {
  int Hr = val / 3600;
  int Min = (val % 3600) / 60;
  int Sec = val % 60;
  char buffer[30];
  sprintf(buffer, "%d : %d : %d", Hr, Min, Sec);
  Serial.print(buffer);
}