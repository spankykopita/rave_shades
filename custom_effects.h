//   Graphical effects to run on the RGB Shades LED array
//   Each function should have the following components:
//    * Must be declared void with no parameters or will break function pointer array
//    * Check effectInit, if false then init any required settings and set effectInit true
//    * Set effectDelay (the time in milliseconds until the next run of this effect)
//    * All animation should be controlled with counters and effectDelay, no delay() or loops
//    * Pixel data should be written using leds[XY(x,y)] to map coordinates to the RGB Shades layout

void overlaySideBeat() {
  // if (isBeat) {
  //   for (int i = 0; i < SIDESIZE; i++) {
  //     leds[SideTable[i]] = ColorFromPalette(currentPalette, 200);
  //   }
  // }
}

unsigned long travelSwitchMillis;
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
      int pixelPaletteIndex = constrain(senseValue / analyzerPaletteFactor - 15, 0, 240);
      int pixelBrightness = constrain(senseValue * analyzerFadeFactor, 0, 255);

      pixelColor = ColorFromPalette(currentPalette, pixelPaletteIndex, pixelBrightness);

      leds[XY(x, y)] = pixelColor;
      leds[XY(kMatrixWidth - x - 1, y)] = pixelColor;
    }
  }

  overlaySideBeat();
  overlayTopLineBeatPrediction();
}
