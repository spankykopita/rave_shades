// Assorted useful functions and variables
// TODO move to types file
class AudioSample {
  public:
    unsigned long millis;
    byte spectrumValue;
};

// Global variables
boolean effectInit = false; // indicates if a pattern has been recently switched
uint16_t effectDelay = 0; // time between automatic effect changes
unsigned long effectMillis = 0; // store the time of last effect function run
unsigned long cycleMillis = 0; // store the time of last effect change
unsigned long paletteBlendMillis = 0; // store the time of last palette blend
unsigned long currentMillis; // store current loop's millis value
unsigned long hueMillis; // store time of last hue change
unsigned long eepromMillis; // store time of last setting change
unsigned long audioMillis; // store time of last audio update
byte currentEffect = 0; // index to the currently running effect
boolean autoCycle = false; // flag for automatic effect changes
boolean eepromOutdated = false; // flag for when EEPROM may need to be updated
byte currentBrightness = STARTBRIGHTNESS; // 0-255 will be scaled to 0-MAXBRIGHTNESS
boolean audioEnabled = true; // flag for running audio patterns
boolean audioActive = false;
uint8_t fadeActive = 0;

const uint16_t SAMPLE_WINDOW_MILLIS = 2500;
const byte GAP_BETWEEN_SAMPLES_MILLIS = 20;
const uint16_t MAX_NUMBER_OF_SAMPLES = SAMPLE_WINDOW_MILLIS / GAP_BETWEEN_SAMPLES_MILLIS;

unsigned int maxSample;
Array<AudioSample, MAX_NUMBER_OF_SAMPLES> rollingSamples; // Audio samples for beat detection

uint16_t millisPerBeat = 0;
unsigned long lastConfidentBeatTimeMillis = 0;

CRGBPalette16 currentPalette(RainbowColors_p); // global palette storage
CRGBPalette16 nextPalette(RainbowColors_p); // global palette storage

typedef void (*functionList)(); // definition for list of effect function pointers
// extern byte numEffects;


// Increment the global hue value for functions that use it
byte cycleHue = 0;
byte cycleHueCount = 0;
void hueCycle(byte incr) {
    cycleHueCount = 0;
    cycleHue+=incr;
}

// Set every LED in the array to a specified color
void fillAll(CRGB fillColor) {
  for (byte i = 0; i < NUM_LEDS; i++) {
    leds[i] = fillColor;
  }
}

// Fade every LED in the array by a specified amount
void fadeAll(byte fadeIncr) {
  for (byte i = 0; i < NUM_LEDS; i++) {
    leds[i] = leds[i].fadeToBlackBy(fadeIncr);
  }
}

// Shift all pixels by one, right or left (0 or 1)
void scrollArray(byte scrollDir) {
  
    byte scrollX = 0;
    for (byte x = 1; x < kMatrixWidth; x++) {
      if (scrollDir == 0) {
        scrollX = kMatrixWidth - x;
      } else if (scrollDir == 1) {
        scrollX = x - 1;
      }
      
      for (byte y = 0; y < kMatrixHeight; y++) {
        leds[XY(scrollX,y)] = leds[XY(scrollX + scrollDir*2 - 1,y)];
      }
    } 
}

// Mirror right side of glasses from left
void mirrorArray() {
  for (byte x = 0; x < kMatrixWidth / 2; x++) {
    for (byte y = 0; y < kMatrixHeight; y++) {
      leds[XY(kMatrixWidth - x - 1, y)] = leds[XY(x, y)];
    }
  }
}

// Pick a random palette from a list
void selectRandomPalette() {

  switch(random8(8)) {
    case 0:
    currentPalette = CloudColors_p;
    break;
    
    case 1:
    currentPalette = LavaColors_p;
    break;
    
    case 2:
    currentPalette = OceanColors_p;
    break;
    
    case 4:
    currentPalette = ForestColors_p;
    break;
    
    case 5:
    currentPalette = RainbowColors_p;
    break;
    
    case 6:
    currentPalette = PartyColors_p;
    break;
    
    case 7:
    currentPalette = HeatColors_p;
    break;
  }

}

CRGBPalette16 getRandomAudioPalette() {
  switch (random8(8))
  {
  case 0:
    return CRGBPalette16(CRGB::Red, CRGB::Orange, CRGB::Violet);
  case 1:
    return CRGBPalette16(CRGB::Cyan, CRGB::LightSeaGreen, CRGB::BlueViolet, CRGB::Red);
  case 2:
    return CRGBPalette16(CRGB::LightSkyBlue, CRGB::LightCoral, CRGB::MediumVioletRed);
  case 4:
    return CRGBPalette16(CRGB::Fuchsia, CRGB::DeepPink, CRGB::HotPink, CRGB::Salmon);
  case 5:
    return RainbowColors_p;
  case 6:
    return PartyColors_p;
  case 7:
    return HeatColors_p;
  }
}

// Pick a random palette from a list
void selectRandomAudioPalette() {
  currentPalette = getRandomAudioPalette();
}

void selectRandomNoisePalette() {

  switch(random8(4)) {
    case 0:
    currentPalette = CRGBPalette16(CRGB::Black, CRGB::Red, CRGB::Black, CRGB::Blue);
    break;
    
    case 1:
    currentPalette = CRGBPalette16(CRGB::DarkGreen, CRGB::Black, CRGB::Green);
    break;
    
    case 2:
    currentPalette = CRGBPalette16(CRGB(0,0,8), CRGB(0,0,16), CRGB(0,0,32), CRGB::White);
    break;
    
    case 3:
    currentPalette = CRGBPalette16(CRGB(255,0,127), CRGB::Black, CRGB::OrangeRed);
    break;
    
    case 5:
    currentPalette = RainbowColors_p;
    break;
    
    case 6:
    currentPalette = PartyColors_p;
    break;
    
    case 7:
    currentPalette = HeatColors_p;
    break;
  }

}

// Interrupt normal operation to indicate that auto cycle mode has changed
void confirmBlink(CRGB blinkColor, byte count) {

  for (byte i = 0; i < count; i++) {
    fillAll(blinkColor);
    FastLED.show();
    delay(200);
    fillAll(CRGB::Black);
    FastLED.show();
    delay(200);
  }

}

// write EEPROM value if it's different from stored value
void updateEEPROM(byte location, byte value) {
  if (EEPROM.read(location) != value) EEPROM.write(location, value);
}

// Write settings to EEPROM if necessary
void checkEEPROM() {
  if (eepromOutdated) {
    if (currentMillis - eepromMillis > EEPROMDELAY) {
      updateEEPROM(0, 99);
      updateEEPROM(1, currentEffect);
      // updateEEPROM(2, autoCycle);
      updateEEPROM(3, currentBrightness);
      updateEEPROM(4, audioEnabled);
      eepromOutdated = false;
    }
  }
}


#define MAX_DIMENSION ((kMatrixWidth>kMatrixHeight) ? kMatrixWidth : kMatrixHeight)
uint8_t noise[MAX_DIMENSION][MAX_DIMENSION];
uint16_t scale = 72;
static uint16_t nx;
static uint16_t ny;
static uint16_t nz;
uint16_t nspeed = 0;

// Fill the x/y array of 8-bit noise values using the inoise8 function.
void fillnoise8() {
  for(int i = 0; i < MAX_DIMENSION; i++) {
    int ioffset = scale * i;
    for(int j = 0; j < MAX_DIMENSION; j++) {
      int joffset = scale * j;
      noise[i][j] = inoise8(nx + ioffset,ny + joffset,nz);
    }
  }
  nz += nspeed;
}

byte nextBrightness(boolean resetVal) {
    const byte brightVals[6] = {32,64,96,160,224,255};

    if (resetVal) {
      currentBrightness = STARTBRIGHTNESS;
    } else {
      currentBrightness++;
      if (currentBrightness > sizeof(brightVals)/sizeof(brightVals[0])) currentBrightness = 0;
    }

  return brightVals[currentBrightness];
}

long mapToByteRange(long value, long fromLow, long fromHigh) {
  return map(value, fromLow, fromHigh, 0, 255);
}

long mapFromByteRange(long value, long toLow, long toHigh) {
  return map(value, 0, 255, toLow, toHigh);
}

long mapFromPercentile(long percentile, long toLow, long toHigh) {
  return map(percentile, 0, 100, toLow, toHigh);
}

// Print given array.
void printArray(uint16_t* array, uint16_t arraySize) {
  for (uint16_t i = 0; i < arraySize; i++) {
    Serial.print(array[i]);
    Serial.print(' ');
  }
  Serial.println();
}

void printArray(byte* array, uint16_t arraySize) {
  for (uint16_t i = 0; i < arraySize; i++) {
    Serial.print(array[i]);
    Serial.print(' ');
  }
  Serial.println();
}

void printArray(float* array, uint16_t arraySize) {
  for (uint16_t i = 0; i < arraySize; i++) {
    Serial.print(array[i]);
    Serial.print(' ');
  }
  Serial.println();
}

void printArray(Array<unsigned long, 125UL> array) {
  for (uint16_t i = 0; i < array.size(); i++) {
    Serial.print(array[i]);
    Serial.print(' ');
  }
  Serial.println();
}

void printArray(Vector<unsigned long> array) {
  for (uint16_t i = 0; i < array.size(); i++) {
    Serial.print(array[i]);
    Serial.print(' ');
  }
  Serial.println();
}

void printArray(CircularBuffer<uint16_t, 20UL, uint8_t> array) {
  for (uint16_t i = 0; i < array.size(); i++) {
    Serial.print(array[i]);
    Serial.print(' ');
  }
  Serial.println();
}

void printSampleValues() {
  Serial.print("Sample values: ");
  for (uint16_t i = 0; i < rollingSamples.size(); i++) {
    Serial.print(rollingSamples[i].spectrumValue);
    Serial.print(' ');
  }
  Serial.println();
}

void printSampleTimes() {
  Serial.print("Sample times: ");
  for (uint16_t i = 0; i < rollingSamples.size(); i++) {
    Serial.print(rollingSamples[i].millis);
    Serial.print(' ');
  }
  Serial.println();
}
