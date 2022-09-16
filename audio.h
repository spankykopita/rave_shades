// Interface with MSGEQ7 chip for audio analysis

#define AUDIODELAY 8

// Pin definitions
#define ANALOGPIN 3
#define STROBEPIN 8
#define RESETPIN 7

// Smooth/average settings
#define SPECTRUMSMOOTH 0.1
#define PEAKDECAY 0.95f
#define NOISEFLOOR 65

// AGC settings
#define AGCSMOOTH 0.004
#define GAINUPPERLIMIT 20.0
#define GAINLOWERLIMIT 0.1

// Global variables
unsigned int spectrumValue[7];  // holds raw adc values
float spectrumDecay[7] = {0};   // holds time-averaged values
float spectrumPeaks[7] = {0};   // holds peak values

float audioAvg = 300.0;
float gainAGC = 1.0;

// Beat tracking
byte beatCounter = 0;
uint32_t lastPredictedBeatMillis;
uint32_t nextPredictedBeatMillis;

uint16_t millisPerBeat = 0;
uint32_t lastConfidentBeatTimeMillis = 0;

// Peak tracking
uint32_t lastLocalBassPeakMillis = 0;
unsigned int lastBassValue = 0;
boolean isLocalBassPeak = false;

unsigned int maxBassValue = 0;

float averageOfCurrentPeaks() {
  unsigned int spectrumSum = 0;
  for (int i = 0; i < 7; i++) {
    spectrumSum += spectrumPeaks[i];
  }
  return spectrumSum / 7.0;
}

float averageOfCurrentDecay() {
  unsigned int spectrumSum = 0;
  for (int i = 0; i < 7; i++) {
    spectrumSum += spectrumDecay[i];
  }
  return spectrumSum / 7.0;
}

float averageOfCurrentValues() {
  unsigned int spectrumSum = 0;
  for (int i = 0; i < 7; i++) {
    spectrumSum += spectrumValue[i];
  }
  return spectrumSum / 7.0;
}

float maxOfCurrentValues() {
  unsigned int maxVal = 0;
  for (int i = 0; i < 7; i++) {
    maxVal = std::max(maxVal, spectrumValue[i]);
  }
  return maxVal;
}

uint16_t bpmToMillisPerBeat(uint16_t bpm) {
  return 1.0 / bpm * 60.0 * 1000.0;
}

uint16_t millisPerBeatToBPM(uint16_t millisPerBeat) {
  return 1.0 / millisPerBeat * 1000.0 * 60.0;
}

const byte MIN_BPM = 60;
const byte MAX_BPM = 125;
const uint16_t MIN_MILLIS_PER_BEAT = bpmToMillisPerBeat(MAX_BPM);
const uint16_t MAX_MILLIS_PER_BEAT = bpmToMillisPerBeat(MIN_BPM);

long lastSampleAnalysis = 0;

boolean hasPredictedBeat() {
  return millisPerBeat != 0 && lastConfidentBeatTimeMillis != 0;
}

uint32_t getLastPredictedBeatMillis() {
  // Int division will round down to the number of beats we've passed without confidence
  uint16_t beatsFromPredictionToLast = (currentMillis - lastConfidentBeatTimeMillis) / millisPerBeat;
  // Scale back up to get the actual time of the last beat
  return beatsFromPredictionToLast * millisPerBeat + lastConfidentBeatTimeMillis;
}

uint32_t getNextPredictedBeatMillis() {
  // Int division will round down to the number of beats we've passed without confidence
  uint16_t beatsFromPredictionToLast = (currentMillis - lastConfidentBeatTimeMillis) / millisPerBeat;
  // Scale back up to get the actual time of the last beat
  return (beatsFromPredictionToLast + 1) * millisPerBeat + lastConfidentBeatTimeMillis;
}

const uint8_t PEAK_ROUNDING = 15;
void fixupPeakGaps(unsigned int* peakGaps, byte size) {
  // Adjust gaps to fit into expected BPM range
  for (int i = 0; i < size; i++) {
    while (!(MIN_MILLIS_PER_BEAT <= peakGaps[i] && peakGaps[i] <= MAX_MILLIS_PER_BEAT)) {
      if (peakGaps[i] < MIN_MILLIS_PER_BEAT) {
        peakGaps[i] *= 2;
      } else {
        peakGaps[i] /= 2.0;
      }
    }

    // For purpose of bucketing into histogram, floor to nearest 15
    peakGaps[i] = (peakGaps[i] + PEAK_ROUNDING / 2) / PEAK_ROUNDING * PEAK_ROUNDING;
  }
}

// Get the most common gap in the histogram or 0 if we don't have confidence
uint16_t getMostCommonGap(unsigned int * peakGaps, byte size) {
  ace_sorting::shellSortKnuth(peakGaps, size);
  Serial.print("Sorted adjusted gaps histo: ");
  printArray(peakGaps, size);

  int mostCommonItem;
  int countOfMostCommonItem = 0;
  int countOfCurrentItem = 0;
  int lastValue = 0;
  for (int i = 0; i < size; i++) {
    if (lastValue == peakGaps[i]) {
      countOfCurrentItem++;
    } else {
      countOfCurrentItem = 1;
    }

    if (countOfCurrentItem > countOfMostCommonItem) {
      countOfMostCommonItem = countOfCurrentItem;
      mostCommonItem = peakGaps[i];
    }

    lastValue = peakGaps[i];
  }

  if (countOfMostCommonItem < 4) {
    Serial.println("No confidence in BPM");
    return 0;
  }

  Serial.print("Most common millis gap: ");
  Serial.println(mostCommonItem);
  // Serial.println(countOfMostCommonItem);

  return mostCommonItem;
}

void analyzeSamples() {
  Serial.println("ANALYZING SAMPLES");

  printSampleTimes();
  if (rollingPeaks.size() < 8) {
    return;
  }
  byte peakGapsSize = rollingPeaks.size() - 1;
  unsigned int peakGaps[peakGapsSize];
  for (int i = 0; i < peakGapsSize; i++) {
    peakGaps[i] = rollingPeaks[i + 1] - rollingPeaks[i];
  }
  Serial.print("Gaps: ");
  printArray(peakGaps, peakGapsSize);
  
  fixupPeakGaps(peakGaps, peakGapsSize);

  Serial.print("Adjusted gaps: ");
  printArray(peakGaps, peakGapsSize);

  // Update global beat tracking with either the high confident beat gap or
  // 0 to indicate we don't have confidence
  millisPerBeat = getMostCommonGap(peakGaps, peakGapsSize);

  if (millisPerBeat == 0) {
    // No confidence in BPM
    return;
  }

  Serial.print("BPM: ");
  Serial.println(millisPerBeatToBPM(millisPerBeat));

  // Find the latest peak gap that matches the beat we calculated and set it, if any
  // (possibly is from a past analysis but still in the histo)
  for (int i = 0; i < peakGapsSize; i++) {
    if (peakGaps[i] == millisPerBeat) {
      lastConfidentBeatTimeMillis = rollingPeaks[i + 1];
      // Attempt to stop jumping of beat visuals
      // if (getNextPredictedBeatMillis() - lastPredictedBeatMillis < 100) {
      //   beatCounter--;
      // }
      Serial.print("Found confident beat time: ");
      Serial.println(lastConfidentBeatTimeMillis);
    }
  }
}

void updateBeats() {
  if (hasPredictedBeat() && nextPredictedBeatMillis < currentMillis) {
    lastPredictedBeatMillis = nextPredictedBeatMillis;
    nextPredictedBeatMillis = getNextPredictedBeatMillis();
    beatCounter++;
  }
}

void doAnalogs() {
  static PROGMEM const byte spectrumFactors[7] = {8, 8, 9, 8, 7, 3, 10};

  // reset MSGEQ7 to first frequency bin
  digitalWrite(RESETPIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(RESETPIN, LOW);
  delayMicroseconds(10);

  // store sum of values for AGC
  unsigned int analogsum = 0;

  // cycle through each MSGEQ7 bin and read the analog values
  for (int i = 0; i < 7; i++) {

    // set up the MSGEQ7
    digitalWrite(STROBEPIN, LOW);
    delayMicroseconds(25); // allow the output to settle

    // read the analog value
    spectrumValue[i] = (analogRead(ANALOGPIN)+analogRead(ANALOGPIN)+analogRead(ANALOGPIN))/3;
    digitalWrite(STROBEPIN, HIGH);
    delayMicroseconds(30);

    // noise floor filter
    if (spectrumValue[i] < NOISEFLOOR) {
      spectrumValue[i] = 0;
    } else {
      spectrumValue[i] -= NOISEFLOOR;
    }

    // apply correction factor per frequency bin
    spectrumValue[i] = spectrumValue[i] * pgm_read_byte(spectrumFactors+i) / 10;

    // prepare average for AGC
    analogsum += spectrumValue[i];

    // apply current gain value
    spectrumValue[i] *= gainAGC;

    // process time-averaged values
    spectrumDecay[i] = (1.0 - SPECTRUMSMOOTH) * spectrumDecay[i] + SPECTRUMSMOOTH * spectrumValue[i];

    // process peak values
    spectrumPeaks[i] = std::max(spectrumPeaks[i] * PEAKDECAY, spectrumDecay[i]);
  }

  if (lastBassValue > spectrumValue[1] && spectrumValue[1] > spectrumPeaks[1] * 1.50f && currentMillis > lastLocalBassPeakMillis + MIN_MILLIS_PER_BEAT / 4) {
    isLocalBassPeak = true;
    lastLocalBassPeakMillis = currentMillis;
    // Record the time of any peaks for BPM calculations
    rollingPeaks.push(currentMillis);
  } else {
    isLocalBassPeak = false;
  }
  lastBassValue = spectrumValue[1];

  // if (spectrumValue[1] > maxBassValue) {
  //   maxBassValue = spectrumValue[1];
  //   Serial.print("New max bass: ");
  //   Serial.println(maxBassValue);
  // }

  // Calculate audio levels for automatic gain
  audioAvg = (1.0 - AGCSMOOTH) * audioAvg + AGCSMOOTH * (analogsum / 7.0);

  // Calculate gain adjustment factor
  gainAGC = constrain(300.0 / audioAvg, GAINLOWERLIMIT, GAINUPPERLIMIT);
  // Serial.println(gainAGC);

  // Analyze samples to determine BPM every ~3 seconds
  if (currentMillis - lastSampleAnalysis > 2500) {
    lastSampleAnalysis = currentMillis;
    analyzeSamples();
  }

  updateBeats();
}
