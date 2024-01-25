/** 
 * Rock Band 2 to PC Converter
 * Adapted From https://github.com/evankale/ArduinoMidiDrums
 * I'm really only using the hit-detection code for now, but leaving
 * the MIDI stuff in there for future additions (such as adapting for console use?)
 * Having it commented will exclude it at compile-time, so no harm done there
**/

// Lots of defines and variables
#define NUM_PIEZOS 4           // Number of Pads (does NOT include cymbals)
#define NUM_FOOT_SWITCHES 2    // Number of Kick Pedals

#define DEBUG true             // Enables Serial.println messages
#define MIDI false             // Enables MIDI-out
#define PC_OUT true            // Enables Joystick-out (PC)

#define SNARE_THRESHOLD 200    // Red Pad
#define HTOM_THRESHOLD 200     // Yellow Pad
#define MTOM_THRESHOLD 200     // Blue Pad
#define LTOM_THRESHOLD 200     // Green Pad
// ---------------------------
#define HIHAT_THRESHOLD 100    // Yellow Cymbal
#define RIDECYM_THRESHOLD 250  // Blue Cymbal
#define CRASHCYM_THRESHOLD 250 // Green Cymbal

// ===========================
#define PEDAL_THRESHOLD 50     // Both Kicks (shouldn't be needed as it's a digital signal)


//MIDI note defines for each trigger
/* #define SNARE_NOTE 38
#define HTOM_NOTE 48
#define MTOM_NOTE 47
#define LTOM_NOTE 45
#define HIHAT_NOTE 42
#define RIDECYM_NOTE 51
#define CRASHCYM_NOTE 49
#define KICK_NOTE 36
#define ALTKICK_NOTE 36
#define HIHAT_CHOKE_NOTE 78

// Rock Band Midi Adapter notes
#define SNARE_NOTE 38;  // Midi value for Red Pad
#define HIHAT_NOTE 22;  // Midi value for Yellow Cymbal
#define HTOM_NOTE 48;  // Midi value for Yellow Pad
#define RIDECYM_NOTE 51;  // Midi value for Blue Cymbal
#define MTOM_NOTE 45;  // Midi value for Blue Pad
#define CRASHCYM_NOTE 49;  // Midi value for Green Cymbal
#define LTOM_NOTE 41;  // Midi value for Green pad
#define KICK_NOTE 33;  // Midi value for Kick pedal

//MIDI defines
#define NOTE_ON_CMD 0x90
#define NOTE_OFF_CMD 0x80
*/
#define MAX_MIDI_VELOCITY 127

//MIDI baud rate
//#define SERIAL_RATE 31250

//Pad sensing defines
//ALL TIME MEASURED IN MILLISECONDS
#define SIGNAL_BUFFER_SIZE 100
#define PEAK_BUFFER_SIZE 30
#define MAX_TIME_BETWEEN_PEAKS 20
#define MIN_TIME_BETWEEN_NOTES 50

//Cymbal shifting variables
#define CYMBAL_SHIFT_PIN 5

//map that holds the mux slots of the piezos
unsigned short slotMap[NUM_PIEZOS] = {A1, A2, A3, A4};

//map that holds the respective note to each piezo
unsigned short noteMap[NUM_PIEZOS];

//map that holds the respective threshold to each piezo
unsigned short thresholdMap[NUM_PIEZOS];

// Ring buffers to store analog signal and peaks
short currentSignalIndex[NUM_PIEZOS];
short currentPeakIndex[NUM_PIEZOS];
unsigned short signalBuffer[NUM_PIEZOS][SIGNAL_BUFFER_SIZE];
unsigned short peakBuffer[NUM_PIEZOS][PEAK_BUFFER_SIZE];


boolean noteReady[NUM_PIEZOS];
unsigned short noteReadyVelocity[NUM_PIEZOS];
boolean isLastPeakZeroed[NUM_PIEZOS];
unsigned long lastPeakTime[NUM_PIEZOS];
unsigned long lastNoteTime[NUM_PIEZOS];

// Buffers for foot pedals
// Needed to separate this from other sensors since Rock Band has binary pedals (it's either on or off)
short currentFootSwitchIndex[NUM_FOOT_SWITCHES];
unsigned short footSwitchSlotMap[NUM_FOOT_SWITCHES] = {A0, A5}; //A0 and A5 being used as digital for now
unsigned short footSwitchNoteMap[NUM_FOOT_SWITCHES];
unsigned short footSwitchThresholdMap[NUM_FOOT_SWITCHES];

boolean footNoteReady[NUM_FOOT_SWITCHES];
int footSwitchLastNote[NUM_FOOT_SWITCHES];

uint8_t lastCShift = LOW;

/**
 * The setup function:
 * -Sets up serial
 * -Inits any global variables
 * -Inits the arrays of sensors (pads and kicks)
 * TODO: -Inits the control buttons (dpad, start, select, etc.)
 * TODO: -Sets up the LED pattern for the front (Xbox Button) LED ring
 */
void setup() {

  // Debug Serial Monitoring
  if (DEBUG) {
    Serial.begin(9600);
    while(!Serial) ;  // makes the program wait until the serial monitor is open
  }

  if(PC_OUT){
    //stub for now
  }

  if(DEBUG) Serial.print("Setting up pads on pins ");
  // Init sensor array
  for (byte i=0; i<NUM_PIEZOS; ++i) {
    currentSignalIndex[i] = 0;
    currentPeakIndex[i] = 0;
    memset(signalBuffer[i],0,sizeof(signalBuffer[i]));
    memset(peakBuffer[i],0,sizeof(peakBuffer[i]));
    noteReady[i] = false;
    noteReadyVelocity[i] = 0;
    isLastPeakZeroed[i] = true;
    lastPeakTime[i] = 0;
    lastNoteTime[i] = 0;
    Serial.print(slotMap[i]); Serial.print(", ");
    pinMode(slotMap[i], INPUT_PULLUP);
  }
  Serial.println("");

  // Init footpedal array
  if(DEBUG) Serial.print("Setting up pedals on pins ");
  for (byte i=0; i<NUM_FOOT_SWITCHES; ++i) {
    footNoteReady[i] = false;
    footSwitchLastNote[i] = 0;
    Serial.print(footSwitchSlotMap[i]); Serial.print(", ");
    pinMode(footSwitchSlotMap[i], INPUT_PULLUP);
  }
  Serial.println("");

  thresholdMap[0] = SNARE_THRESHOLD;
  thresholdMap[1] = HTOM_THRESHOLD;
  thresholdMap[2] = MTOM_THRESHOLD;
  thresholdMap[3] = LTOM_THRESHOLD;
  thresholdMap[4] = HIHAT_THRESHOLD;
  thresholdMap[5] = RIDECYM_THRESHOLD;
  thresholdMap[6] = CRASHCYM_THRESHOLD;

  footSwitchThresholdMap[0] = PEDAL_THRESHOLD;
  footSwitchThresholdMap[1] = PEDAL_THRESHOLD;

  if(MIDI){
    /*noteMap[0] = SNARE_NOTE;
    noteMap[1] = HTOM_NOTE;
    noteMap[2] = MTOM_NOTE;
    noteMap[3] = LTOM_NOTE;
    noteMap[4] = HIHAT_NOTE;
    noteMap[5] = RIDECYM_NOTE;  
    noteMap[6] = CRASHCYM_NOTE;
    footSwitchNoteMap[0] = KICK_NOTE;
    footSwitchNoteMap[1] = ALTKICK_NOTE;*/
  } else if(PC_OUT){
    //set up joystick button outputs here?
    noteMap[0] = 0;
    noteMap[1] = 1;
    noteMap[2] = 2;
    noteMap[3] = 3;
    noteMap[4] = 4;
    noteMap[5] = 5;  
    noteMap[6] = 6;
    footSwitchNoteMap[0] = 7;
    footSwitchNoteMap[1] = 8;
  }

  // TODO: Set up control button outputs here

  // Set up Cymbal Shifting
  pinMode(CYMBAL_SHIFT_PIN, INPUT_PULLUP);

  // Temporary Setup for LEDs
  // Just want to turn one on so I know when it's plugged in
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);

  digitalWrite(9, HIGH);
  digitalWrite(10, HIGH);
  digitalWrite(11, HIGH);
  
}

/**
 * The loop function:
 * -Gets time (for deltatime purposes)
 */
void loop() {
  
  unsigned long currentTime = millis();

  // update the piezo buffers
  // the inner function recordNewPeak() will fire a note if appropriate
  for (byte i=0; i<NUM_PIEZOS; ++i) {
    //get a new signal from analog read
    unsigned short newSignal = analogRead(slotMap[i]);
    signalBuffer[i][currentSignalIndex[i]] = newSignal;
    
    //if new signal is 0
    if (newSignal < thresholdMap[i]) {
      if (!isLastPeakZeroed[i] && (currentTime - lastPeakTime[i]) > MAX_TIME_BETWEEN_PEAKS) {
        recordNewPeak(i,0);
      } else {
        //get previous signal
        short prevSignalIndex = currentSignalIndex[i]-1;
        if(prevSignalIndex < 0) prevSignalIndex = SIGNAL_BUFFER_SIZE-1;        
        unsigned short prevSignal = signalBuffer[i][prevSignalIndex];
        
        unsigned short newPeak = 0;
        
        //find the wave peak if previous signal was not 0 by going
        //through previous signal values until another 0 is reached
        while (prevSignal >= thresholdMap[i]) {
          if(signalBuffer[i][prevSignalIndex] > newPeak) {
            newPeak = signalBuffer[i][prevSignalIndex];        
          }
          
          //decrement previous signal index, and get previous signal
          prevSignalIndex--;
          if (prevSignalIndex < 0) prevSignalIndex = SIGNAL_BUFFER_SIZE-1;
          prevSignal = signalBuffer[i][prevSignalIndex];
        }
        
        if(newPeak > 0) {
          recordNewPeak(i, newPeak);
        }
      }
  
    }
        
    currentSignalIndex[i]++;
    if (currentSignalIndex[i] == SIGNAL_BUFFER_SIZE) currentSignalIndex[i] = 0;
  }

  // update foot switches
  // directly fires presses if the kick is considered "on"
  for (short i=0; i<NUM_FOOT_SWITCHES; ++i) {
    //get current signal from the digital foot switches
    unsigned short dSignal = digitalRead(footSwitchSlotMap[i]);

    if ( ( dSignal == LOW  ) && (footSwitchLastNote[i] != LOW) ) {
        footSwitchLastNote[i] = dSignal;
        //midiNoteOn(footSwitchNoteMap[i], MAX_MIDI_VELOCITY);
        if(PC_OUT) digitalWrite(footSwitchNoteMap[i], LOW);
        if(DEBUG) Serial.println("on!");
    } else if ( ( dSignal == HIGH ) && ( footSwitchLastNote[i] != HIGH ) ) {
        footSwitchLastNote[i] = dSignal;
        //midiNoteOff(footSwitchNoteMap[i], 0);
        if(DEBUG) Serial.println("off!");
        if(PC_OUT) digitalWrite(footSwitchNoteMap[i], HIGH);
    }
  }

  // TESTING: WILL MOVE - read the cymbal shift
  unsigned short cShift = digitalRead(5);

  if ( ( cShift == LOW  ) && (lastCShift != LOW) ) {
      //if(PC_OUT) digitalWrite(footSwitchNoteMap[i], LOW);
      if(DEBUG) Serial.println("Cymbal Time!");
  } else if ( ( cShift == HIGH ) && ( lastCShift != HIGH ) ) {
      if(DEBUG) Serial.println("Cymbal Time is Over!");
      //if(PC_OUT) digitalWrite(footSwitchNoteMap[i], HIGH);
  }
  lastCShift = cShift;
}

void recordNewPeak(short slot, short newPeak) {
  isLastPeakZeroed[slot] = (newPeak == 0);
  
  unsigned long currentTime = millis();
  lastPeakTime[slot] = currentTime;
  
  //new peak recorded (newPeak)
  peakBuffer[slot][currentPeakIndex[slot]] = newPeak;
  
  //1 of 3 cases can happen:
  // 1) note ready - if new peak >= previous peak
  // 2) note fire - if new peak < previous peak and previous peak was a note ready
  // 3) no note - if new peak < previous peak and previous peak was NOT note ready
  
  //get previous peak
  short prevPeakIndex = currentPeakIndex[slot]-1;
  if(prevPeakIndex < 0) prevPeakIndex = PEAK_BUFFER_SIZE-1;        
  unsigned short prevPeak = peakBuffer[slot][prevPeakIndex];
   
  if (newPeak > prevPeak && (currentTime - lastNoteTime[slot])>MIN_TIME_BETWEEN_NOTES) {
    noteReady[slot] = true;
    if (newPeak > noteReadyVelocity[slot])
      noteReadyVelocity[slot] = newPeak;
  } else if (newPeak < prevPeak && noteReady[slot]) {
    // skip the calculation if we're just outputting to PC, since it's just a button press
    if(MIDI) noteFire(noteMap[slot], scaleVelocity(noteReadyVelocity[slot], thresholdMap[slot]));
    else noteFire(noteMap[slot], 127);
    noteReady[slot] = false;
    noteReadyVelocity[slot] = 0;
    lastNoteTime[slot] = currentTime;
  }
  
  currentPeakIndex[slot]++;
  if(currentPeakIndex[slot] == PEAK_BUFFER_SIZE) currentPeakIndex[slot] = 0;  
}

// Normalizes the raw input power to MIDI velocity (0-127)
unsigned short scaleVelocity(unsigned short originalVelocity, unsigned short threshold) {
  unsigned short r_min = threshold;
  unsigned short r_max = 1023;
  unsigned short t_min = 1;
  unsigned short t_max = MAX_MIDI_VELOCITY;

  float value = ((float) t_max - (float) t_min) * ((float) originalVelocity - (float) r_min) / ((float) r_max - (float) r_min) + (float) t_min;
  return static_cast<unsigned short>(value);
}

// Fires a note - either outputs MIDI or presses a joystick button
void noteFire(unsigned short note, unsigned short velocity) {
  if(velocity > MAX_MIDI_VELOCITY)
    velocity = MAX_MIDI_VELOCITY;
  
  if(MIDI){
    midiNoteOn(note, velocity);
    midiNoteOff(note, velocity);
  } else if(PC_OUT){
    // we need another array to track note fire times. sometimes it doesn't pick up the press if it's too quick
    rbNoteOn(note);
    rbNoteOff(note);
  }

  if (DEBUG) {
    Serial.print("Firing note "); Serial.print(note); Serial.print(" at "); Serial.println(velocity);
    Serial.println(digitalRead(5));
  }
}

void rbNoteOn(byte note){
    //Serial.print("RB On: ");
    //Serial.println(note);
}

void rbNoteOff(byte note){
    //Serial.print("RB Off: ");
    //Serial.println(note);
}

void midiNoteOn(byte note, byte midiVelocity) {
  /*Serial1.write(NOTE_ON_CMD);
  Serial1.write(note);
  Serial1.write(midiVelocity);
  Serial2.write(NOTE_ON_CMD);
  Serial2.write(note);
  Serial2.write(midiVelocity);
  */
}

void midiNoteOff(byte note, byte midiVelocity) {
  /*Serial1.write(NOTE_OFF_CMD);
  Serial1.write(note);
  Serial1.write(midiVelocity);
  Serial2.write(NOTE_OFF_CMD);
  Serial2.write(note);
  Serial2.write(midiVelocity);
  }*/
}
