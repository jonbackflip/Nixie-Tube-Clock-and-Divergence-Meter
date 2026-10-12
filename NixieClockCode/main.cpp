#include <OneButton.h>
#include <Wire.h>
#include "RTCDriver.h"

#define NUM_TUBES 8
#define DP_PIN 10

// Tube select pins
//const uint8_t digitPins[NUM_TUBES] = { 2, 3, 4, 5, 6, 7, 8, 9 };
const uint8_t digitPins[NUM_TUBES] = { 9, 8, 7, 6, 5, 4, 3, 2

// Timing
const uint32_t COUNT_INTERVAL = 100; // milliseconds
const uint32_t MUX_INTERVAL = 2000;   // microseconds
const uint32_t CURSOR_INTERVAL = 500; // milliseconds

uint32_t previousCountMillis = 0;
uint32_t previousMuxMicros = 0;
uint32_t previousCursorMillis = 0;

// RandMode Timing and Digit Arrays
bool digitStopped[NUM_TUBES];
uint8_t finalDigits[NUM_TUBES];

uint32_t randDisplayMillis = 0;
uint32_t digitStopTime[NUM_TUBES];

// Clock Modes
enum ClockMode {
  MODE_CLOCK,
  MODE_DATE_DISPLAY,
  MODE_TIME_SET,
  MODE_DATE_SET,
  MODE_RAND_WORLDLINE,
  MODE_ANTI_CATHODE_POISONING
};

ClockMode currentMode = MODE_CLOCK;

// THE TIME AND DATE
bool TwentyFourHrFormat = false;

uint32_t setDateModeStartTime = 0;
uint32_t dateDisplayStartMillis = 0;
uint32_t antiCathodePoisoningStartTime = 0;

// Display data
uint8_t digits[NUM_TUBES];
bool dpEnabled[NUM_TUBES];
bool separatorsVisible = true;

bool cursorVisible = false;
uint8_t selectedDigit = 0; // Cursor for SET modes
uint8_t currentTube = 0;   // Keeps track
//uint8_t acpDigit = 0; // Tracks current digit being displayed for Anti Cathode Poisoning

uint8_t acpWave = 0;
uint8_t acpStep = 0;
uint32_t acpLastStepMillis = 0;

const uint8_t ACP_TUBE_OFFSET = 1;
const uint8_t ACP_NUM_WAVES = 1;

OneButton BTN1(11, true, true);
OneButton BTN2(12, true, true);

////////// SETUP //////////
void setup() {

  Serial.begin(115200);

  // Time Set Up
  Wire.begin();
  setTimeI2C(10, 20, 24); // HR / MIN // SEC
  setDateI2C(8,14,26); // MM / DD // YY

  currTime  = readTimeI2C();
  readDateI2C();

  // Tube pins
  for (int i = 0; i < NUM_TUBES; i++) {
    pinMode(digitPins[i], OUTPUT);
    digitalWrite(digitPins[i], LOW);
  }

  // BCD pins
  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);

  // DP pin
  pinMode(DP_PIN, OUTPUT);
  digitalWrite(DP_PIN, LOW);

  // Buttons
  pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  BTN1.attachClick(handleBTN1Click);
  BTN1.attachLongPressStart(enterTimeSetMode);
  BTN2.attachClick(handleBTN2Click);
  BTN2.attachLongPressStart(enterDateSetMode);

  updateDigits_Time(currTime);
  updateDigits_Date();
  
  randomSeed(analogRead(0));
}


////////// LOOP //////////
void loop() {

  switch(currentMode) {
    
    // NORMAL OPERATION //
    case MODE_CLOCK:
    
      currTime = readTimeI2C();
      separatorsVisible = (currTime % 2 == 0);
  
      // Show DATE every 30 seconds
      if ((currTime % 60) == 30) {
          currentMode = MODE_DATE_DISPLAY;
  
          dateDisplayStartMillis = millis();
  
          readDateI2C();
          updateDigits_Date();
      }
      
      // Run ANTI CATHODE POISONING ROUTINE at every new minute
      else if ((currTime % 60) == 0) {
        enterACPRoutine();
      }   
      
      // Show TIME at all other times
      else {
          updateDigits_Time(currTime);
      }
      break;
      
    // DATE Display //  
    case MODE_DATE_DISPLAY:
      if (millis() - dateDisplayStartMillis >= 3000) {
          currentMode = MODE_CLOCK; // Go back to normal operation after 3 seconds
      }
      break;
    
    // CURSOR BLINKS during time-setting and date-setting //
    case MODE_TIME_SET:
    case MODE_DATE_SET:
      if (millis() - previousCursorMillis >= CURSOR_INTERVAL) {
        previousCursorMillis = millis();
    
        cursorVisible = !cursorVisible;
      }
      break;
      
    // RANDOMIZATION SEQUENCE //
    case MODE_RAND_WORLDLINE:
      updateRandomDisplay();
      break;
    
    // ANTI CATHODE POISONING ROUTINE //
    case MODE_ANTI_CATHODE_POISONING:
      updateACPDisplay();
      break;
  }
  

  // MULTIPLEX REFRESH TIMING //
  if (micros() - previousMuxMicros >= MUX_INTERVAL) {

    previousMuxMicros = micros();

    refreshDisplay();
  }
  
  BTN1.tick();
  BTN2.tick();
}

void handleBTN1Click() {

  switch (currentMode) {

    case MODE_CLOCK:
      randWorldLine();
      break;

    case MODE_TIME_SET:
      incrementSelectedDigit();
      break;
      
    case MODE_DATE_SET:
      incrementSelectedDigit_Date();
      break;

  }
}

void handleBTN2Click() {

  switch (currentMode) {

    case MODE_CLOCK:
      toggleTimeFormat();
      break;

    case MODE_TIME_SET:
      advanceSelectedDigit();
      break;
      
    case MODE_DATE_SET:
      advanceSelectedDigit_Date();
      break;
    
  }
}


////////// RANDWORLDLINE //////////
void randWorldLine() {

    currentMode = MODE_RAND_WORLDLINE;

    // Assign random stop times (now() + 500-1500ms)
    for (int i = 0; i < NUM_TUBES; i++) {
      digitStopped[i] = false;
      digitStopTime[i] = millis() + random(500, 1500);
    }

    // Choose final values
    finalDigits[0] = random(0,2); // Only 0 or 1 for tube 1
    finalDigits[1] = 10; // No valid num for tube 2. DP is on instead

    // Digits 2 - 7 choose a num 0-9
    for (int i = 2; i < NUM_TUBES; i++) {
      finalDigits[i] = random(0,10);
    }
}

void updateRandomDisplay() {

  // TO MAKE SURE DP 2 AND 5 STAY OFF
  for (int i = 0; i < NUM_TUBES; i++) {
    dpEnabled[i] = false;
  }

  dpEnabled[1] = true;

  bool allStopped = true;

  for (int i = 0; i < NUM_TUBES; i++) {

    // Skip DP tube [1]
    if (i == 1) {
      digits[i] = 10;
      continue;
    }

    // If not stopped, keep spinning
    if (!digitStopped[i]) {
      allStopped = false;

      digits[i] = random(0, 10);

      if (millis() >= digitStopTime[i]) {
        digitStopped[i] = true;
        digits[i] = finalDigits[i];
      }
    }

    // Else, choose final value
    else {
      digits[i] = finalDigits[i];
    }
  }
  
  // Once all digits stopped, hold for 2 seconds
  if (allStopped) {

    if (randDisplayMillis == 0) {
      randDisplayMillis = millis();
    }

    if (millis() - randDisplayMillis >= 2000) {
      currentMode = MODE_CLOCK;
      randDisplayMillis = 0;
    }
  }
}


////////// TOGGLETIMEFORMAT //////////
void toggleTimeFormat() {
  TwentyFourHrFormat = !TwentyFourHrFormat;
}


////////// SETTIMEMODE //////////
void enterTimeSetMode() {

  currentMode = MODE_TIME_SET;

  updateDigits_Time(currTime);

  selectedDigit = 0;

  cursorVisible = true;

  previousCursorMillis = millis();

  // skip separator tube
  if (selectedDigit == 2 || selectedDigit == 5) {
    selectedDigit++;
  }

  dpEnabled[2] = true;
  dpEnabled[5] = true;
}

void incrementSelectedDigit() {

  digits[selectedDigit]++;

  // wrap values

  // tens hour
  if (selectedDigit == 0) {
    if (digits[selectedDigit] > 1 && !TwentyFourHrFormat) {
      digits[selectedDigit] = 0;
    }
    if (digits[selectedDigit] > 2 && TwentyFourHrFormat) {
      digits[selectedDigit] = 0;
    }
  }

  // ones hour
  if (selectedDigit == 1 && digits[0] == 1 && !TwentyFourHrFormat) {
    if (digits[selectedDigit] > 2) {
      digits[selectedDigit] = 0;
    }
  }

  if (selectedDigit == 1 && digits[0] == 2 && TwentyFourHrFormat) {
    if (digits[selectedDigit] > 3) {
      digits[selectedDigit] = 0;
    }
  }

  // tens min
  if (selectedDigit == 3) {
    if (digits[selectedDigit] > 5) {
      digits[selectedDigit] = 0;
    }
  }

  // tens sec
  if (selectedDigit == 6) {
    if (digits[selectedDigit] > 5) {
      digits[selectedDigit] = 0;
    }
  }
  
  else { // ones : min, sec
    if (digits[selectedDigit] > 9) {
      digits[selectedDigit] = 0;
    }
  }
  
}

void advanceSelectedDigit() {
  
  selectedDigit++;
  
  if (selectedDigit == 2 || selectedDigit == 5) {
    selectedDigit++;
  }
  
  if (selectedDigit >= NUM_TUBES) {

    applyEditedTime();

    currentMode = MODE_CLOCK;
  }
}

void applyEditedTime() {
  
  int hrs = (digits[0] * 10) + digits[1];
  int mins = (digits[3] * 10) + digits[4];
  int secs = (digits[6] * 10) + digits[7];
  
  setTimeI2C(hrs, mins, secs);
  
  currTime = readTimeI2C();
}


////////// SETDATEMODE //////////
void enterDateSetMode() {
  
  currentMode = MODE_DATE_SET;
  
  updateDigits_Date();
  
  setDateModeStartTime = millis();

  selectedDigit = 0;

  cursorVisible = true;

  previousCursorMillis = millis();

  // skip separator tube
  if (selectedDigit == 2 || selectedDigit == 5) {
    selectedDigit++;
  }

  dpEnabled[2] = true;
  dpEnabled[5] = true;
}

void incrementSelectedDigit_Date() {
  
  digits[selectedDigit]++;

  // wrap values

  // tens month
  if (selectedDigit == 0) {
    if (digits[selectedDigit] > 1) {
      digits[selectedDigit] = 0;
    }
  }

  // ones month
  if (selectedDigit == 1 && digits[0] == 1) {
    if (digits[selectedDigit] > 2) {
      digits[selectedDigit] = 0;
    }
  }

  // tens day
  if (selectedDigit == 3) {
    if (digits[selectedDigit] > 3) {
      digits[selectedDigit] = 0;
    }
  }
  
  // ones day
  if (selectedDigit == 4 && digits[3] == 3) {
    if (digits[selectedDigit] > 1) {
      digits[selectedDigit] = 0;
    }
  }
  
  else { // ones day , tens & ones year
    if (digits[selectedDigit] > 9) {
      digits[selectedDigit] = 0;
    }
  }
}

void advanceSelectedDigit_Date() {
  
  selectedDigit++;
  
  if (selectedDigit == 2 || selectedDigit == 5) {
    selectedDigit++;
  }
  
  if (selectedDigit >= NUM_TUBES) {
    applyEditedDate();
    currentMode = MODE_CLOCK;
  }
}

void applyEditedDate() {
  
  uint8_t month =
    digits[0] * 10 +
    digits[1];

  uint8_t day =
    digits[3] * 10 +
    digits[4];

  uint8_t year =
    digits[6] * 10 +
    digits[7];

  setDateI2C(month, day, year);

  readDateI2C();
}




void enterACPRoutine() {
  currentMode = MODE_ANTI_CATHODE_POISONING;
  acpStep = 0;
  acpWave = 0;
  acpLastStepMillis = millis();

  for (int i = 0; i < NUM_TUBES; i++) {
    digits[i] = 10;
    dpEnabled[i] = false;
  }
}

/*
void updateACPDisplay() {   // OLD ACP Routine //

  if (acpDigit > 9) {
    // Done — return to clock
    currentMode = MODE_CLOCK;
    currTime = readTimeI2C();
    updateDigits_Time(currTime);
    return;
  }

  if (millis() - antiCathodePoisoningStartTime >= COUNT_INTERVAL) {
    antiCathodePoisoningStartTime = millis();

    for (int i = 0; i < NUM_TUBES; i++) {
      digits[i] = acpDigit;
      dpEnabled[i] = false;
    }

    acpDigit++;
  }
}
*/

void updateACPDisplay() {

  if (millis() - acpLastStepMillis < COUNT_INTERVAL) return;
  acpLastStepMillis = millis();

  // Phase 1 runs steps 0 through (9 + 7*OFFSET)
  // Phase 2 runs steps 0 through (10 + 7*OFFSET) — 10 steps to go 9 down to blank
  const int phase1Steps = 9 + (7 * ACP_TUBE_OFFSET) + 1;  // inc wave clears tube 7
  const int phase2Steps = 10 + (7 * ACP_TUBE_OFFSET) + 1; // dec wave + blank clears tube 7

  // Combined step runs phase1 then phase2 back to back
  const int totalSteps = phase1Steps + phase2Steps;

  for (int i = 0; i < NUM_TUBES; i++) {
    dpEnabled[i] = false;

    if (acpStep < phase1Steps) {
      // --- PHASE 1: increment wave sweeps left to right ---
      int tubeVal = acpStep - (i * ACP_TUBE_OFFSET);

      if (tubeVal < 0) {
        digits[i] = 10;       // not reached yet — blank
      } else if (tubeVal > 9) {
        digits[i] = 9;        // wave passed — hold at 9, phase 2 will clear it
      } else {
        digits[i] = tubeVal;
      }

    } else {
      // --- PHASE 2: decrement + fade wave sweeps left to right ---
      int phase2Step = acpStep - phase1Steps;
      int tubeVal = 9 - (phase2Step - (i * ACP_TUBE_OFFSET));

      if (phase2Step < (i * ACP_TUBE_OFFSET)) {
        digits[i] = 9;        // dec wave hasn't reached this tube yet — hold at 9
      } else if (tubeVal < 0) {
        digits[i] = 10;       // wave has fully passed — blank
      } else {
        digits[i] = tubeVal;
      }
    }
  }

  acpStep++;

  if (acpStep >= totalSteps) {
    acpStep = 0;
    acpWave++;

    if (acpWave >= ACP_NUM_WAVES) {
      currentMode = MODE_CLOCK;
      currTime = readTimeI2C();
      updateDigits_Time(currTime);
    }
  }
}



////////// DISPLAY REFRESH //////////
void refreshDisplay() {

  // Turn OFF all tubes and DP first
  disableAllDigits();
  digitalWrite(DP_PIN, LOW);

  //delayMicroseconds(20); // not strictly necessary

  if (((currentMode == MODE_TIME_SET && currentTube == selectedDigit) ||
      (currentMode == MODE_DATE_SET && currentTube == selectedDigit)) && cursorVisible) {

    // Do not write a valid number to the K155ID1 during time/date setting
    writeDigit(10);
  }
  
  else {
    // Write current digit to K155ID1
    writeDigit(digits[currentTube]);
  }

  // Enable DP when a DP digit is encountered
  if (dpEnabled[currentTube]) {
    digitalWrite(DP_PIN, HIGH);
  }

  // Enable current tube
  digitalWrite(digitPins[currentTube], HIGH);

  // Move to next tube
  currentTube++;

  // Loop back after all tubes have been refreshed
  if (currentTube >= NUM_TUBES) {
    currentTube = 0;
  }
}

////////// PUT TIME INTO DIGIT ARRAY //////////
void updateDigits_Time(uint32_t currTime) {
  
  int currSecs = currTime % 60;
  int currMins = (currTime / 60) % 60;
  int displayHrs = (currTime / 3600) % 24;

  if (!TwentyFourHrFormat) { // If Twelve Hr Format
    displayHrs %= 12;
    
    if (displayHrs == 0) { // Show 12 instead of 00 for 12 AM
      displayHrs = 12;
    }
  }

  // HH
  digits[0] = displayHrs / 10;
  digits[1] = displayHrs % 10;

  // blank separator tube
  digits[2] = 10;

  // MM
  digits[3] = currMins / 10;
  digits[4] = currMins % 10;

  // blank separator tube
  digits[5] = 10;

  // SS
  digits[6] = currSecs / 10;
  digits[7] = currSecs % 10;

  for (int i = 0; i < NUM_TUBES; i++) {
    dpEnabled[i] = false;
  }

  if (separatorsVisible) {
    dpEnabled[2] = true;
    dpEnabled[5] = true;
  }
}

////////// PUT DATE INTO DIGIT ARRAY //////////
void updateDigits_Date() {
  
  // Month
  digits[0] = currMonth / 10;
  digits[1] = currMonth % 10;
  
  // blank separator tube
  digits[2] = 10;
  
  // Day
  digits[3] = currDay / 10;
  digits[4] = currDay % 10;
  
  // blank separator tube
  digits[5] = 10;
  
  // Year
  digits[6] = currYear / 10;
  digits[7] = currYear % 10;
  
  for (int i = 0; i < NUM_TUBES; i++) {
    dpEnabled[i] = false;
  }

  if (separatorsVisible) {
    dpEnabled[2] = true;
    dpEnabled[5] = true;
  }
}


////////// TURN OFF ALL TUBES //////////
void disableAllDigits() {

  for (int i = 0; i < NUM_TUBES; i++) {
    digitalWrite(digitPins[i], LOW);
  }
}

////////// WRITE SINGLE DIGIT TO K155ID1 //////////
void writeDigit(uint8_t value) {

  // Convert value into BCD bits

  int A = bitRead(value, 0);
  int B = bitRead(value, 1);
  int C = bitRead(value, 2);
  int D = bitRead(value, 3);

  muxWrite(D, C, B, A);
}

////////// MUX FUNCTION //////////
void muxWrite(int D, int C, int B, int A) {

  digitalWrite(A3, D);
  digitalWrite(A2, C);
  digitalWrite(A1, B);
  digitalWrite(A0, A);
}

////////// DEBUG FUNCTION //////////
void printMuxState() {

  int d3 = digitalRead(A3);
  int d2 = digitalRead(A2);
  int d1 = digitalRead(A1);
  int d0 = digitalRead(A0);

  int decimal = (d3 << 3) | (d2 << 2) | (d1 << 1) | d0;

  char buffer[30];

  sprintf(buffer, "%d %d %d %d : %d",
          d3, d2, d1, d0, decimal);

  Serial.println(buffer);
}