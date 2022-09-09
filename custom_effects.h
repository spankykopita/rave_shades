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
      leds[SideTable[i]] = ColorFromPalette(currentPalette, 150);
    }
  }
}

uint32_t travelSwitchMillis;
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
    leds[activeLED] = ColorFromPalette(currentPalette, 150, 150);
  }
}

uint8_t mapToBassPeaks(long value, long fromLow, long fromHigh, long toMillisHigh) {
  long targetMillis = currentMillis - map(value, fromLow, fromHigh, 0, toMillisHigh);
  for (int i = rollingPeaks.size() - 1; i >= 0; i--) {
    if (targetMillis >= rollingPeaks[i]) {
      // That means our target time we're trying to find the value of came after this bass peak,
      // so we need to calculate the easing down to 0 from that peak and where the target time falls
      uint32_t fadeEnd = rollingPeaks[i] + 250;
      if (targetMillis > fadeEnd) {
        return 10;
      }
      return map(targetMillis, fadeEnd, rollingPeaks[i], 10, 170);
    }
  }

  return 10;
}

void customAnalyzer() {
  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 10;
    selectRandomAudioPalette();
    audioActive = true;
    fadeActive = 0;
  }

  CRGB pixelColor;

  const float yScale = 255.0 / kMatrixHeight;

  for (byte x = 0; x < kMatrixWidth / 2; x++) {
    for (byte y = 0; y < kMatrixHeight; y++) {
      int senseValue = spectrumDecay[x] / analyzerScaleFactor - mapToByteRange(y, kMatrixHeight - 1, 0);
      uint8_t pixelPaletteIndex = constrain(senseValue / analyzerPaletteFactor - 15, 0, 240);
      uint8_t pixelBrightness = constrain(senseValue * analyzerFadeFactor, 0, 255);
      // uint8_t pixelBrightnessMultiplier = mapToBassPeaks(0, 0, 100, 600);

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
    audioActive = true;
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
      float distance = sqrt(adjustedX * adjustedX + adjustedY * adjustedY);

      uint8_t pixelPaletteIndex = mapToByteRange((theta + distance) * 100, (-PI + 0) * 100, (PI + 5) * 100) - currentMillis / 8;
      uint8_t pixelBrightness = mapToBassPeaks(distance * 100, 0, 5 * 100, 400);
      // uint8_t pixelBrightness = mapFromByteRange(pixelPaletteIndex, 0, 150);

      pixelColor = ColorFromPalette(currentPalette, pixelPaletteIndex, pixelBrightness);

      leds[XY(x, y)] = pixelColor;
      leds[XY(kMatrixWidth - x - 1, y)] = pixelColor;
    }
  }
  
  overlaySideBeat();
  overlayTopLineBeatPrediction();
}
