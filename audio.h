// Interface with MSGEQ7 chip for audio analysis

#define AUDIODELAY 8

// Pin definitions
#define ANALOGPIN 3
#define STROBEPIN 8
#define RESETPIN 7

// Smooth/average settings
#define SPECTRUMSMOOTH 0.1
#define PEAKDECAY 0.95
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
    maxVal = max(maxVal, spectrumValue[i]);
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
const byte MAX_BPM = 130;
const uint16_t MIN_MILLIS_PER_BEAT = bpmToMillisPerBeat(MAX_BPM);
const uint16_t MAX_MILLIS_PER_BEAT = bpmToMillisPerBeat(MIN_BPM);

long lastSampleMillis = 0;
long lastSampleAnalysis = 0;

float maxOfCurrentSamples() {
  byte maxValue = 0;
  for (uint16_t i = 0; i < rollingSamples.size(); i++) {
    maxValue = max(rollingSamples[i].spectrumValue, maxValue);
  }
  return maxValue;
}

void recordSample() {
  unsigned int value = spectrumValue[1]; // + spectrumValue[5];
  if (value > maxSample) {
    maxSample = value;
  }
  // For lack of a better approach - samples take up a lot of memory but we only need the 
  // values relative to each other. Since I don't know what the max possible value to get
  // from the mic is, just record what the largest sample is. Hopefully this stabilizes
  // quickly and isn't outrageously huge, or else the relative gap between values will be lost.
  byte valueToRecord = mapToByteRange(value, 0, maxSample);

  // Serial.println(currentMillis);
  AudioSample sample;
  // Floor to 10 millisecond increments to simplify the eventual histogram
  sample.millis = currentMillis;
  // Add just the frequencies of bass and drums
  sample.spectrumValue = valueToRecord;
  // Push adds to tail, so when we iterate we are going forward in time (oldest to newest)
  rollingSamples.push_back(sample);
}

// Calculate the threshold for what constitutes a peak
byte selectPeakThreshold() {
  byte samples[rollingSamples.size()];
  // byte maxValue = maxOfCurrentSamples();
  // TODO filter to samples over max / 2 to save memory instead of copying and sorting all

  // Copy all of the samples out of the circular buffer into a new array so that we can sort it
  for (int i = 0; i < rollingSamples.size(); i++) {
    samples[i] = rollingSamples[i].spectrumValue;
  }
  ace_sorting::shellSortKnuth(samples, rollingSamples.size());
  // printArray(samples, rollingSamples.size());

  byte startThresholdSearch = mapFromPercentile(90, 0, rollingSamples.size());
  byte endThresholdSearch = mapFromPercentile(98, 0, rollingSamples.size());
  byte maxSlopeIndex = startThresholdSearch;
  // Search for a high slope among the sorted values to indicate a good threshold
  for (int i = startThresholdSearch; i < endThresholdSearch; i++) {
    if (samples[i] - samples[i - 1] > samples[maxSlopeIndex] - samples[maxSlopeIndex - 1]) {
      maxSlopeIndex = i;
    }
  }

  // Serial.println(maxSlopeIndex);
  return samples[maxSlopeIndex];
}

// Return the times of sample data that was over the threshold
Vector<unsigned long> findPeaks(byte peakThreshold) {
  byte lastSampleValue = 0;
  // This is kinda dangerous because we don't know how many peaks we'll find. C sucks.
  unsigned long peakTimesStorage[MAX_NUMBER_OF_SAMPLES / 10];
  Vector<unsigned long> peakTimes(peakTimesStorage);

  for (int i = 0; i < rollingSamples.size(); i++) {
    AudioSample sample = rollingSamples[i];
    // If the last value was below threshold and this is above threshold, count as a peak
    if (lastSampleValue < peakThreshold && sample.spectrumValue >= peakThreshold) {
      peakTimes.push_back(sample.millis);
    }
    lastSampleValue = sample.spectrumValue;
  }

  return peakTimes;
}

CircularBuffer<uint16_t, 20> adjustedGapHistogram;
void fixupPeakGaps(unsigned int* peakGaps, byte size) {
  // Adjust gaps to fit into expected BPM range
  for (int i = 0; i < size; i++) {
    while (!(MIN_MILLIS_PER_BEAT < peakGaps[i] && peakGaps[i] < MAX_MILLIS_PER_BEAT)) {
      if (peakGaps[i] < MIN_MILLIS_PER_BEAT) {
        peakGaps[i] *= 2;
      } else {
        peakGaps[i] /= 2.0;
      }
    }

    // For purpose of bucketing into histogram, floor to nearest 15
    // TODO implement round
    peakGaps[i] = peakGaps[i] / 15 * 15;

    // Record gaps to histogram
    adjustedGapHistogram.push(peakGaps[i]);
  }
}

// Get the most common gap in the histogram or 0 if we don't have confidence
uint16_t getMostCommonGap() {
  uint16_t sorted_gaps[adjustedGapHistogram.size()];
  // Copy all of the samples out of the circular buffer into a new array so that we can sort it
  for (int i = 0; i < adjustedGapHistogram.size(); i++) {
    sorted_gaps[i] = adjustedGapHistogram[i];
  }
  ace_sorting::shellSortKnuth(sorted_gaps, adjustedGapHistogram.size());
  Serial.print("Sorted adjusted gaps histo: ");
  printArray(sorted_gaps, adjustedGapHistogram.size());

  int mostCommonItem;
  int countOfMostCommonItem = 0;
  int countOfCurrentItem = 0;
  int lastValue = 0;
  for (int i = 0; i < adjustedGapHistogram.size(); i++) {
    if (lastValue == sorted_gaps[i]) {
      countOfCurrentItem++;
    } else {
      countOfCurrentItem = 1;
    }

    if (countOfCurrentItem > countOfMostCommonItem) {
      countOfMostCommonItem = countOfCurrentItem;
      mostCommonItem = sorted_gaps[i];
    }

    lastValue = sorted_gaps[i];
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

  printSampleValues();
  // printSampleTimes();

  byte peakThreshold = selectPeakThreshold();
  Serial.print("Peak threshold: ");
  Serial.println(peakThreshold);
  if (peakThreshold < 5) {
    // No confidence in BPM;
    millisPerBeat = 0;
    return;
  }

  Vector<unsigned long> peakTimes = findPeaks(peakThreshold);
  // printArray(peakTimes);

  byte peakGapsSize = peakTimes.size() - 1;
  unsigned int peakGaps[peakGapsSize];
  for (int i = 0; i < peakGapsSize; i++) {
    peakGaps[i] = peakTimes[i + 1] - peakTimes[i];
  }
  Serial.print("Gaps: ");
  printArray(peakGaps, peakGapsSize);
  
  fixupPeakGaps(peakGaps, peakGapsSize);

  Serial.print("Adjusted gaps: ");
  printArray(peakGaps, peakGapsSize);

  // Serial.print("Adjust gap histo: ");
  // for (uint16_t i = 0; i < adjustedGapHistogram.size(); i++) {
  //   Serial.print(adjustedGapHistogram[i]);
  //   Serial.print(' ');
  // }
  // Serial.println();

  // Update global beat tracking with either the high confident beat gap or
  // 0 to indicate we don't have confidence
  millisPerBeat = getMostCommonGap();

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
      lastConfidentBeatTimeMillis = peakTimes[i + 1];
      Serial.print("Found confident beat time: ");
      Serial.println(lastConfidentBeatTimeMillis);
    }
  }
}

boolean hasPredictedBeat() {
  return millisPerBeat != 0 && lastConfidentBeatTimeMillis != 0;
}

unsigned long lastPredictedBeatMillis() {
  // Int division will round down to the number of beats we've passed without confidence
  uint16_t beatsFromPredictionToLast = (currentMillis - lastConfidentBeatTimeMillis) / millisPerBeat;
  // Scale back up to get the actual time of the last beat
  return beatsFromPredictionToLast * millisPerBeat + lastConfidentBeatTimeMillis;
}

unsigned long nextPredictedBeatMillis() {
  // Int division will round down to the number of beats we've passed without confidence
  uint16_t beatsFromPredictionToLast = (currentMillis - lastConfidentBeatTimeMillis) / millisPerBeat;
  // Scale back up to get the actual time of the last beat
  return (beatsFromPredictionToLast + 1) * millisPerBeat + lastConfidentBeatTimeMillis;
}

void doAnalogs() {
  static PROGMEM const byte spectrumFactors[7] = {8, 8, 9, 8, 7, 4, 10};

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
    spectrumPeaks[i] = max(spectrumPeaks[i], spectrumDecay[i]) * PEAKDECAY;
  }

  // Calculate audio levels for automatic gain
  audioAvg = (1.0 - AGCSMOOTH) * audioAvg + AGCSMOOTH * (analogsum / 7.0);

  // Calculate gain adjustment factor
  gainAGC = constrain(300.0 / audioAvg, GAINLOWERLIMIT, GAINUPPERLIMIT);
  // Serial.println(gainAGC);

  // Record a new sample every 10 milliseconds
  if (currentMillis - lastSampleMillis > GAP_BETWEEN_SAMPLES_MILLIS) {
    lastSampleMillis = currentMillis;
    recordSample();
  }

  // Analyze samples to determine BPM every ~3 seconds
  if (currentMillis - lastSampleAnalysis > SAMPLE_WINDOW_MILLIS) {
    lastSampleAnalysis = currentMillis;
    analyzeSamples();
    rollingSamples.clear();
  }
}
