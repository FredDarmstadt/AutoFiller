#include "Button.h"
//-----------------------------------------
// Optional Debugging Behaviors
//-----------------------------------------
#define ACCEPT_SERIAL_INPUT
#define PRINT_DEBUGGING_INFO
//#define SHOW_READ_TIMING
//#define SHOW_LOOP_TIMING
//#define SHOW_TIME_BETWEEN_READS
#define SHOW_STATE
#define SHOW_TARGET
#define SHOW_CURRENT_WEIGHT

//-----------------------------------------
// Input/Output
//-----------------------------------------
#define STRAIN_GUAGE_DOUT 3
#define STRAIN_GUAGE_CLK  2
#define FILLER_VALVE LED_BUILTIN //pin 13
#define USER_BUTTON A0

//-----------------------------------------
// Filler Modes and States
//-----------------------------------------
#define MODE_FILL 0
#define MODE_TRAIN_FULL 1 
#define MODE_TRAIN_EMPTY 2

//-----------------------------------------
// Globals
//-----------------------------------------
long scaleZero, weight, weightEmpty, weightFull;

int  mode = MODE_FILL;
bool isReady = false;
bool isHalted = false;
bool isFilling = false;
bool isButtonDown = false;
int buttonState = 0;
int buttonValue;

//-----------------------------------------
// Optional timing instrumentation
//-----------------------------------------
#ifdef SHOW_READ_TIMING
unsigned long readDuration;
#endif
#ifdef SHOW_LOOP_TIMING
unsigned long loop_before, loop_duration;
#endif
#ifdef SHOW_TIME_BETWEEN_READS
unsigned long read_before, time_between_reads;
#endif

//-----------------------------------------
// SETUP THE COMPUTER
//-----------------------------------------
void setup() {
  auto STRAIN_GUAGE_CLK_var = STRAIN_GUAGE_CLK;
  long minTareWeight=0x7FFFFFFFL;

  setFill(false); // definitely not filling!
  
  weight = 0;
  weightEmpty = 0x7FFFFFFFL;
  weightFull = 0;
  
  pinMode(FILLER_VALVE, OUTPUT);
  pinMode(USER_BUTTON, INPUT);
  pinMode(STRAIN_GUAGE_CLK_var, OUTPUT);
  digitalWrite(STRAIN_GUAGE_CLK_var, LOW);

  minTareWeight = 0x7FFFFFFF;
  for(int i=0; i<30; i++)
  {
    while (digitalRead(STRAIN_GUAGE_DOUT) != LOW) {yield();}
    weight = readStrainGauge();
    if (weight < minTareWeight) minTareWeight = weight;
  }
  scaleZero = minTareWeight;

  isReady = true;
  
  Serial.begin(9600);
  #ifdef PRINT_DEBUGGING_INFO
  Serial.println("Automattic Filling Machine Debugger"); 
  Serial.print("Filler Valve pin: ");
  Serial.println(FILLER_VALVE);
  Serial.print("Empty Scale Tare: ");
  Serial.println(scaleZero);
  #endif
}

//-----------------------------------------
// MAIN LOOP OF AUTO-FILLER
//-----------------------------------------
void loop() {
  bool hasReadStrainGuage;

  #ifdef SHOW_LOOP_TIMING
  loop_before = micros();
  #endif
  #ifdef SHOW_TIME_BETWEEN_READS
  read_before = micros();
  #endif

  // see if the chip is ready and if so read the current weight
  hasReadStrainGuage = false;
  if (digitalRead(STRAIN_GUAGE_DOUT) == LOW) 
  {
    weight = readStrainGauge() - scaleZero;
    if (weight < 0) weight = 20; // keep it positive
    hasReadStrainGuage = true;
  }
  
  #ifdef SHOW_TIME_BETWEEN_READS
  if (hasReadStrainGuage) time_between_reads = micros() - read_before;
  #endif
 
  #ifdef ACCEPT_SERIAL_INPUT
    ProcessSerialInput();
  #endif

  if (weight < (weightEmpty / 3)) {
    if (mode == MODE_FILL) {
      isReady = true; // No bottle or removed prematurely
    }
    setFill (false); // bottle removed?
  }
  if (weight > weightFull) {
    setFill(false); // target reached, stop filling
    isReady = false;
  }
  if (isReady && weight > ((4*weightEmpty) / 5)) {
    setFill(true); // bottle added or being added
    // maybe higher number? 80% of weightEmpty
  }
  buttonValue = analogRead(USER_BUTTON);
  buttonState = buttonValue > 800 ? HIGH : LOW;
  if (buttonState == HIGH && !isButtonDown) {
    onButtonPress();
    isButtonDown = true;
  } else if (buttonState == LOW) {
    isButtonDown = false;
  }
  
  #ifdef SHOW_LOOP_TIMING
  loop_duration = micros() - loop_before;
  #endif

  #ifdef PRINT_DEBUGGING_INFO
  //Show things if there is a new reading and there is anything to show
  if (hasReadStrainGuage) DebuggingPrint();
  #endif
}

//-----------------------------------------
// Debugging Print
//-----------------------------------------
#ifdef PRINT_DEBUGGING_INFO
void DebuggingPrint()
{
  
  #ifdef SHOW_READ_TIMING
  Serial.print("Tr:"); Serial.print(readDuration);
  #endif

  #ifdef SHOW_TIME_BETWEEN_READS
  Serial.print("  Tb:"); Serial.print(time_between_reads);
  #endif
  
  #ifdef SHOW_LOOP_TIMING
  Serial.print("  Tl:"); Serial.print(loop_duration);
  #endif
  
  #if defined(SHOW_READ_TIMING) || defined(SHOW_TIME_BETWEEN_READS) || defined(SHOW_LOOP_TIMING)
  Serial.print("  ");
  #endif

  #ifdef SHOW_STATE
  if(isReady) { Serial.print(" RDY "); }
         else { Serial.print("-rdy "); }
         
  if(isHalted) { Serial.print(" HLT "); }
          else { Serial.print("-hlt "); }
          
  if(isFilling) { Serial.print(" FILL  "); }
           else { Serial.print("-fill  "); }
          
  switch(mode) {
    case MODE_FILL:        Serial.print("mFill        "); break;
    case MODE_TRAIN_FULL:  Serial.print("mTrain-Full  "); break;
    case MODE_TRAIN_EMPTY: Serial.print("mTrain-Empty "); break;
    default:               Serial.print("ModeUnknown  "); break;
  }
  #endif
  
  #ifdef SHOW_TARGET
  Serial.print("Full ");  Serial.print(weightFull);
  Serial.print("  Empty "); Serial.print(weightEmpty);
  #endif
  
  #ifdef SHOW_CURRENT_WEIGHT
  Serial.print("  Weight "); Serial.print(weight);
  #endif
  
  #if defined(SHOW_STATE) || defined(SHOW_TARGET) || defined(SHOW_CURRENT_WEIGHT) || defined(SHOW_READ_TIMING) || defined(SHOW_LOOP_TIMING) || defined(SHOW_TIME_BETWEEN_READS)
  Serial.println();
  #endif
}
#endif

//-----------------------------------------
// Debugging Input for forcing states
//-----------------------------------------
#ifdef ACCEPT_SERIAL_INPUT
void ProcessSerialInput()
{
  if(Serial.available())
  {
    char temp = Serial.read();
    switch(temp) {
      case '+':
        setFill(true);
        break;
      case '-':
        setFill(false);
        break;
      case 't':
        startTrain();
        break;
      case 'b':
        onButtonPress();
        break;
      default:
        Serial.print(temp);
        Serial.println(" unrecognised");
        break;
    }
  }
}
#endif

//-----------------------------------------
// Button Handling
//-----------------------------------------
long lastButtonTime = 0;

bool onButtonPress() {
  long buttonTime = micros();
  // Check if last button press was less than 0.8 seconds ago
  if ((buttonTime - lastButtonTime) / 1000000 < 0.8 && mode == MODE_FILL) {
    // Enter training mode
    startTrain();
  } else {
    switch(mode) {
      case MODE_FILL:
        if (isHalted)
          unHalt();
        else
          halt();
        break;
      case MODE_TRAIN_FULL:
        //user has signaled the current weight is the full weight
        weightFull = weight; 
        mode = MODE_TRAIN_EMPTY;
        break;
      case MODE_TRAIN_EMPTY:
        //user has signaled the current weight is the empty (tare) weight
        weightEmpty = weight;
        mode = MODE_FILL;
        unHalt();
        break;
    }
  }
  lastButtonTime = buttonTime;
}

//-----------------------------------------
// Auto-Filler Actions
//-----------------------------------------

void startTrain() {
  mode = MODE_TRAIN_FULL;
  halt();
}

// Turns off and disables filler
void halt() {
  isHalted = true;
  setFill(false);
}

// Enables filler after being halted
void unHalt() {
  isHalted = false;
}

void setFill(bool fill) {
  if (isHalted && fill) {
    setFill(false);
  } else {
    // Turn the filler ON or OFF
    digitalWrite(FILLER_VALVE, fill ? HIGH : LOW);
    isFilling = fill;
  }
}

//-----------------------------------------
// Read Strain Gauge
//-----------------------------------------
long readStrainGauge() {
  auto STRAIN_GUAGE_CLK_var = STRAIN_GUAGE_CLK;
  unsigned long value;
  unsigned long signextension = 0UL;
  uint8_t data[3];

  #ifdef SHOW_READ_TIMING
  unsigned long atStart = micros();
  #endif
  
  // pulse the clock pin 24 times to read the data  
  data[2] = shiftIn(STRAIN_GUAGE_DOUT, STRAIN_GUAGE_CLK_var, MSBFIRST);
  data[1] = shiftIn(STRAIN_GUAGE_DOUT, STRAIN_GUAGE_CLK_var, MSBFIRST);
  data[0] = shiftIn(STRAIN_GUAGE_DOUT, STRAIN_GUAGE_CLK_var, MSBFIRST);
  
  // set the channel and the gain factor (128) for the next reading using the clock pin
  // three pulses are 
  digitalWrite(STRAIN_GUAGE_CLK_var, HIGH);  digitalWrite(STRAIN_GUAGE_CLK_var, LOW);
  digitalWrite(STRAIN_GUAGE_CLK_var, HIGH);  digitalWrite(STRAIN_GUAGE_CLK_var, LOW);
  digitalWrite(STRAIN_GUAGE_CLK_var, HIGH);  digitalWrite(STRAIN_GUAGE_CLK_var, LOW);
 
  // Replicate the most significant bit to pad out a 32-bit signed integer
  if (data[2] & 0x80) signextension = 0xFF000000UL; 

  // Construct a 32-bit signed integer
  value = ( signextension 
      | static_cast<unsigned long>(data[2]) << 16
      | static_cast<unsigned long>(data[1]) << 8
      | static_cast<unsigned long>(data[0]) );

  #ifdef SHOW_READ_TIMING
  readDuration = micros() - atStart;
  #endif

  return static_cast<long>(value);
}
