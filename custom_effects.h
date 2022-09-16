//   Graphical effects to run on the RGB Shades LED array
//   Each function should have the following components:
//    * Must be declared void with no parameters or will break function pointer array
//    * Check effectInit, if false then init any required settings and set effectInit true
//    * Set effectDelay (the time in milliseconds until the next run of this effect)
//    * All animation should be controlled with counters and effectDelay, no delay() or loops
//    * Pixel data should be written using leds[XY(x,y)] to map coordinates to the RGB Shades layout

void overlaySideBeat() {
  if (isLocalBassPeak) {
    for (int i = 0; i < SIDESIZE; i++) {
      leds[SideTable[i]] = ColorFromPalette(currentOverlayPalette, 150);
    }
  }
}

void overlayTopLineBeatPrediction() {
  if (hasPredictedBeat()) {
    // Serial.print("Predictions: ");
    // Serial.print(lastPredictedBeatMillis);
    // Serial.print(' ');
    // Serial.print(currentMillis);
    // Serial.print(' ');
    // Serial.println(nextPredictedBeatMillis);
    boolean travelRight = beatCounter % 2 == 0;
    byte easedTimeBetweenBeats = ease8InOutQuad(mapToByteRange(currentMillis, lastPredictedBeatMillis, nextPredictedBeatMillis));
    byte activeLED = travelRight ?
      mapFromByteRange(easedTimeBetweenBeats, 0, 13) :
      mapFromByteRange(easedTimeBetweenBeats, 13, 0);
    leds[activeLED] = ColorFromPalette(currentOverlayPalette, 150, 150);
  }
}

// The distance from millis ago to its preceding bass peak. Distance function is linear decrease
// over fadeDurationMillis and output is mapped to toMin and toMax.
uint8_t fadedBassValueAt(long millisAgo, uint16_t fadeDurationMillis, uint8_t toMin = 0, uint8_t toMax = 170) {
  long targetMillis = currentMillis - millisAgo;
  for (int i = rollingPeaks.size() - 1; i >= 0; i--) {
    if (targetMillis >= rollingPeaks[i]) {
      // That means our target time we're trying to find the value of came after this bass peak,
      // so we need to calculate the easing down to 0 from that peak and where the target time falls
      uint32_t fadeEnd = rollingPeaks[i] + fadeDurationMillis;
      if (targetMillis > fadeEnd) {
        return toMin;
      }
      return map(targetMillis, fadeEnd, rollingPeaks[i], toMin, toMax);
    }
  }

  return toMin;
}

#define analyzerFadeFactor 5
#define analyzerScaleFactor 1.5
#define analyzerPaletteFactor 2
void customAnalyzer() {
  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 10;
    selectRandomAudioPalette();
    fadeActive = 0;
  }

  CRGB pixelColor;

  const float yScale = 255.0 / kMatrixHeight;

  for (byte x = 0; x < kMatrixWidth / 2; x++) {
    for (byte y = 0; y < kMatrixHeight; y++) {
      int senseValue = spectrumDecay[x] / analyzerScaleFactor - mapToByteRange(y, kMatrixHeight - 1, 0);
      uint8_t pixelPaletteIndex = constrain(senseValue / analyzerPaletteFactor - 15, 0, 240);
      uint8_t pixelBrightness = constrain(senseValue * analyzerFadeFactor, 0, 255);
      // uint8_t pixelBrightnessMultiplier = mapToHistoricalBassPeaks(0, 0, 100, 600);

      pixelColor = ColorFromPalette(currentPalette, pixelPaletteIndex, pixelBrightness);

      leds[XY(x, y)] = pixelColor;
      leds[XY(kMatrixWidth - x - 1, y)] = pixelColor;
    }
  }

  overlaySideBeat();
  overlayTopLineBeatPrediction();
}

void pulseSpiral() {
  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 10;
    selectRandomAudioPalette();
    fadeActive = 0;
  }

  CRGB pixelColor;

  const float yScale = 255.0 / kMatrixHeight;

  for (byte x = 0; x < kMatrixWidth / 2; x++) {
    for (byte y = 0; y < kMatrixHeight; y++) {
      int adjustedX = x - 3;
      int adjustedY = y - 2;
      // From -PI to PI
      float theta = atan2f(adjustedX, adjustedY);
      float distance = hypot(adjustedX, adjustedY);

      uint8_t pixelPaletteIndex = mapToByteRange((theta + distance) * 100, (-PI + 0) * 100, (PI + 5) * 100) - currentMillis / 8;
      uint8_t pixelBrightness = fadedBassValueAt(mapToMillisAgo(distance * 100, 0, 5 * 100, 400), 500);
      // uint8_t pixelBrightness = mapFromByteRange(pixelPaletteIndex, 0, 150);

      pixelColor = ColorFromPalette(currentPalette, pixelPaletteIndex, pixelBrightness);

      leds[XY(x, y)] = pixelColor;
      leds[XY(kMatrixWidth - x - 1, y)] = pixelColor;
    }
  }
  
  overlaySideBeat();
  overlayTopLineBeatPrediction();
}

// Scanning pattern left/right, uses global hue cycle
void rider() {

  static byte riderPos = 0;

  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 5;
    riderPos = 0;
    fadeActive = 0;
  }

  float bassAdjustment = fadedBassValueAt(0, 500, 50, 255) / 255.0;

  // Draw one frame of the animation into the LED array
  for (byte x = 0; x < kMatrixWidth; x++) {
    int brightness = abs(x * (256 / kMatrixWidth) - triwave8(riderPos) * 2 + 127) * 3;
    if (brightness > 255) brightness = 255;
    brightness = 255 - brightness;

    brightness *= bassAdjustment;

    CRGB riderColor = CHSV(cycleHue, 255, brightness);
    for (byte y = 0; y < kMatrixHeight; y++) {
      leds[XY(x, y)] = riderColor;
    }
  }

  riderPos++; // byte wraps to 0 at 255, triwave8 is also 0-255 periodic
}

// Random pixels scroll sideways, uses current hue
#define rainDir 0
void sideRain() {

  static float x = 0;

  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 10;
    fadeActive = 0;
  }

  // uint8_t brightness = fadedBassValueAt(0, 200, 0, 255);

  x += constrain((spectrumDecay[0] + spectrumDecay[1]) / 600.0, 0.01, 0.9) * 2.3;
  while (x >= 1.0) {
    scrollArray(rainDir);
    x -= 1.0;
  }
  byte randPixel = random8(kMatrixHeight);
  for (byte y = 0; y < kMatrixHeight; y++) leds[XY((kMatrixWidth - 1) * rainDir, y)] = CRGB::Black;
  if ((spectrumDecay[4] + spectrumDecay[5]) * 1.01 >= spectrumPeaks[4] + spectrumPeaks[5]) {
    leds[XY((kMatrixWidth - 1)*rainDir, randPixel)] = ColorFromPalette(currentPalette, cycleHue, 255);
  }
}

// Draw slanting bars scrolling across the array, uses current hue
void slantBars() {

  static float x = 0;
  static byte slantPos = 0;

  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 5;
    fadeActive = 0;
  }
  
  x += constrain((spectrumDecay[0] + spectrumDecay[1]) / 600.0, 0.01, 0.9) * 20.0;
  while (x >= 1.0) {
    slantPos += 1;
    x -= 1.0;
  }

  for (byte x = 0; x < kMatrixWidth; x++) {
    for (byte y = 0; y < kMatrixHeight; y++) {
      leds[XY(x, y)] = CHSV(cycleHue, 255, sin8(x * 32 + y * 32 + slantPos));
    }
  }
}