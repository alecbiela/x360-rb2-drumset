/** 
 * Rock Band 2 to PC Converter
 * Adapted From https://github.com/evankale/ArduinoMidiDrums
 * Changed A LOT of code to reflect the reverse (pulling down inputs instead of peaking up) firing style
 * and removed everything to do with MIDI but the original concept of how this works is the same.
**/

// Lots of defines and variables
#define NUM_PIEZOS 4           // Number of Pads (does NOT include cymbals)
#define NUM_FOOT_SWITCHES 2    // Number of Kick Pedals

#define DEBUG true             // Enables Serial.println messages

#define SNARE_THRESHOLD 200    // Red Pad
#define HTOM_THRESHOLD 200     // Yellow Pad
#define MTOM_THRESHOLD 200     // Blue Pad
#define LTOM_THRESHOLD 200     // Green Pad
// ---------------------------
#define HIHAT_THRESHOLD 50    // Yellow Cymbal
#define RIDECYM_THRESHOLD 50  // Blue Cymbal
#define CRASHCYM_THRESHOLD 85 // Green Cymbal

// ===========================
#define PEDAL_THRESHOLD 50     // Both Kicks (shouldn't be needed as it's a digital signal)

//Pad sensing defines
//ALL TIME MEASURED IN MILLISECONDS
#define SIGNAL_BUFFER_SIZE 100
#define CYMBAL_BUFFER_SIZE 100
#define PEAK_BUFFER_SIZE 30
#define MAX_TIME_BETWEEN_PEAKS 20
#define MIN_TIME_BETWEEN_NOTES 30

//Cymbal shifting variables
#define CYMBAL_SHIFT_PIN A5

//map that holds the mux slots of the piezos
unsigned short slotMap[NUM_PIEZOS] = {A1, A2, A3, A4};

//map that holds the respective note to each piezo
unsigned short noteMap[NUM_PIEZOS];

//map that holds the respective threshold to each piezo
unsigned short thresholdMap[NUM_PIEZOS];
unsigned short cymThresholdMap[NUM_PIEZOS];

// Circular buffers to store analog signal and peaks
short currentSignalIndex[NUM_PIEZOS];
short currentPeakIndex[NUM_PIEZOS];
short currentCymbalBufferIndex;
unsigned short signalBuffer[NUM_PIEZOS][SIGNAL_BUFFER_SIZE];
unsigned short peakBuffer[NUM_PIEZOS][PEAK_BUFFER_SIZE];
unsigned short cymbalBuffer[CYMBAL_BUFFER_SIZE];

boolean noteReady[NUM_PIEZOS];
unsigned short cymbalReadyValue[NUM_PIEZOS];
boolean isPadIdle[NUM_PIEZOS];
unsigned long lastPeakTime[NUM_PIEZOS];
unsigned long lastNoteTime[NUM_PIEZOS];

// Buffers for foot pedals
// Needed to separate this from other sensors since Rock Band has binary pedals (it's either on or off)
short currentFootSwitchIndex[NUM_FOOT_SWITCHES];
unsigned short footSwitchSlotMap[NUM_FOOT_SWITCHES] = {A0, 5}; //A0 and 5 being used as digital for now
unsigned short footSwitchNoteMap[NUM_FOOT_SWITCHES];
unsigned short footSwitchThresholdMap[NUM_FOOT_SWITCHES];

boolean footNoteReady[NUM_FOOT_SWITCHES];
int footSwitchLastNote[NUM_FOOT_SWITCHES];

String colors[NUM_PIEZOS] = {"Blue", "Yellow", "Red", "Green"};

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
    // makes the program wait until the serial monitor is open
    // IMPORTANT: make sure you flag debug as "false" before normal use
    while(!Serial) ;  

  if(DEBUG) Serial.print("Setting up pads on pins ");
  // Init sensor array
  for (byte i=0; i<NUM_PIEZOS; ++i) {
    currentSignalIndex[i] = 0;
    currentPeakIndex[i] = 0;
    memset(signalBuffer[i],0xFF,sizeof(signalBuffer[i]));
    memset(peakBuffer[i],0xFF,sizeof(peakBuffer[i]));
    noteReady[i] = false;
    isPadIdle[i] = true;
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


  thresholdMap[0] = MTOM_THRESHOLD;
  thresholdMap[1] = HTOM_THRESHOLD;
  thresholdMap[2] = SNARE_THRESHOLD;
  thresholdMap[3] = LTOM_THRESHOLD;

  cymThresholdMap[0] = RIDECYM_THRESHOLD;
  cymThresholdMap[1] = HIHAT_THRESHOLD;
  cymThresholdMap[2] = 0;
  cymThresholdMap[3] = CRASHCYM_THRESHOLD;

  footSwitchThresholdMap[0] = PEDAL_THRESHOLD;
  footSwitchThresholdMap[1] = PEDAL_THRESHOLD;


  noteMap[0] = 0;
  noteMap[1] = 1;
  noteMap[2] = 2;
  noteMap[3] = 3;
  noteMap[4] = 4;
  noteMap[5] = 5;  
  noteMap[6] = 6;
  footSwitchNoteMap[0] = 7;
  footSwitchNoteMap[1] = 8;

  // TODO: Set up control button outputs here

  // Set up Cymbal Shifting
  memset(cymbalBuffer,0xFF,sizeof(signalBuffer));
  memset(cymbalReadyValue,0xFF,sizeof(cymbalReadyValue));
  //pinMode(CYMBAL_SHIFT_PIN, INPUT_PULLUP);

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
  
  //int cym = analogRead(A5);
  //Serial.println(cym);

  updatePiezos();
  updateFootswitches();

  //TODO: update regular buttons (dpad, start/select, etc)
}

void updatePiezos() {
  unsigned long currentTime;
  unsigned short newSignal;
  
  // update the piezo buffers
  // the inner function recordNewPeak() will fire a note if appropriate
  for (byte i=0; i<NUM_PIEZOS; ++i) {
    //get a new signal from analog read
    currentTime = millis();
    newSignal = analogRead(slotMap[i]);
    //example: pad 1
    // update the signal buffer for pad 1 at whatever the current signal index is for pad 1 to the new signal
    signalBuffer[i][currentSignalIndex[i]] = newSignal;
    
    //if new signal is below the hit threshold for this sensor
    if (newSignal < thresholdMap[i]) {
      //if the pad is not idle but enough time has passed, set it back to idle
      if (!isPadIdle[i] && ((currentTime - lastPeakTime[i]) > MAX_TIME_BETWEEN_PEAKS)) {
        recordNewPeak(i,1024);
      } else {
        //this chunk basically just gets the strongest hit that has been made on the pad within the stored information
        //not actually sure if this is needed since we're not doing velocity
        
        short prevSignalIndex = currentSignalIndex[i]-1;
        if(prevSignalIndex < 0) prevSignalIndex = SIGNAL_BUFFER_SIZE-1;
           
        //get the previous signal on the signal buffer
        unsigned short prevSignal = signalBuffer[i][prevSignalIndex];
        
        unsigned short newPeak = 1024;
        
        //loop while the previous signal is a hit
        while (prevSignal <= thresholdMap[i]) {
          
          //if the previous signal in the buffer is stronger than the stored strongest, it is now the new strongest
          if(signalBuffer[i][prevSignalIndex] < newPeak) {
            newPeak = signalBuffer[i][prevSignalIndex];
          }
          
          //decrement previous signal index, and get previous signal
          prevSignalIndex--;
          if (prevSignalIndex < 0) prevSignalIndex = SIGNAL_BUFFER_SIZE-1;
          prevSignal = signalBuffer[i][prevSignalIndex];
        }

        if(newPeak < 1024) {
          recordNewPeak(i, newPeak);
        }
      }

    //if the pad falls back out of the threshold before time, reset it to idle
    } else if(!isPadIdle[i] && (newSignal > thresholdMap[i])){
      recordNewPeak(i,1024);
    }
        
    currentSignalIndex[i]++;
    if (currentSignalIndex[i] == SIGNAL_BUFFER_SIZE) currentSignalIndex[i] = 0;
  }
}

void updateFootswitches(){
  // update foot switches
  // directly fires presses if the kick is considered "on"
  for (short i=0; i<NUM_FOOT_SWITCHES; ++i) {
    //get current signal from the digital foot switches
    unsigned short dSignal = digitalRead(footSwitchSlotMap[i]);

    if ( ( dSignal == LOW  ) && (footSwitchLastNote[i] != LOW) ) {
        footSwitchLastNote[i] = dSignal;
         digitalWrite(footSwitchNoteMap[i], LOW);
        if(DEBUG) Serial.println("on!");
    } else if ( ( dSignal == HIGH ) && ( footSwitchLastNote[i] != HIGH ) ) {
        footSwitchLastNote[i] = dSignal;
        if(DEBUG) Serial.println("off!");
        digitalWrite(footSwitchNoteMap[i], HIGH);
    }
  }
}

void recordNewPeak(short slot, short newPeak) {
  isPadIdle[slot] = (newPeak == 1024);
  
  unsigned long currentTime = millis();
  lastPeakTime[slot] = currentTime;
  
  //put this new peak into the peak buffer
  peakBuffer[slot][currentPeakIndex[slot]] = newPeak;
  
  //1 of 3 cases can happen:
  // 1) note becomes ready to fire - if new peak is stronger than the previous peak (and the minimum time between notes has passed)
  // 2) note fire - if new peak is weaker than the previous, and the note is ready to fire
  // 3) no note - if new peak is weaker than the previous, and the note is NOT ready to fire
  
  //get previous peak
  short prevPeakIndex = currentPeakIndex[slot]-1;
  if(prevPeakIndex < 0) prevPeakIndex = PEAK_BUFFER_SIZE-1;        
  unsigned short prevPeak = peakBuffer[slot][prevPeakIndex];
  
  //case 1
  if ((newPeak < prevPeak) && ((currentTime - lastNoteTime[slot]) > MIN_TIME_BETWEEN_NOTES) && (!noteReady[slot])) {
    noteReady[slot] = true;
    cymbalReadyValue[slot] = analogRead(A5);
    Serial.print("Note at "); Serial.print(slot); Serial.print(" is ready, cymbal is at "); Serial.println(cymbalReadyValue[slot]);
  //case 2
  } else if (newPeak > prevPeak && noteReady[slot]) {
    noteFire(slot);
    noteReady[slot] = false;
    lastNoteTime[slot] = currentTime;
  }

  //increment the peak buffer for this note slot
  currentPeakIndex[slot]++;
  if(currentPeakIndex[slot] == PEAK_BUFFER_SIZE) currentPeakIndex[slot] = 0;  
}

// Fires a note
void noteFire(byte n) {
  // we need another array to track note fire times. sometimes it doesn't pick up the press if it's too quick
  rbNoteOn(n);
  rbNoteOff(n);

  // find the index with the strongest cymbal hit
  unsigned short cymIndex = 10;
  unsigned short cymStrength = 65535;

  for(byte i = 0; i<NUM_PIEZOS; ++i){
    if(cymbalReadyValue[i] < cymStrength){
      cymStrength = cymbalReadyValue[i];
      cymIndex = i;
    }
  }

  // if this pad has the strongest cymbal strength, hit a cymbal, then wipe the array
  if((cymIndex == n) && (cymStrength < cymThresholdMap[n])){
    Serial.print("Firing CYMBAL "); Serial.println(colors[n]);
    memset(cymbalReadyValue,0xFF,sizeof(cymbalReadyValue));
  } else{
    Serial.print("Firing PAD "); Serial.println(colors[n]);
  }
}

void rbNoteOn(byte n){
    //Serial.print("RB On: ");
    //Serial.println(note);
}

void rbNoteOff(byte n){
    //Serial.print("RB Off: ");
    //Serial.println(note);
}
