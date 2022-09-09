//   Graphical effects to run on the RGB Shades LED array
//   Each function should have the following components:
//    * Must be declared void with no parameters or will break function pointer array
//    * Check effectInit, if false then init any required settings and set effectInit true
//    * Set effectDelay (the time in milliseconds until the next run of this effect)
//    * All animation should be controlled with counters and effectDelay, no delay() or loops
//    * Pixel data should be written using leds[XY(x,y)] to map coordinates to the RGB Shades layout

// Triple Sine Waves
void threeSine() {

  static byte sineOffset = 0; // counter for current position of sine waves

  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 20;
    fadeActive = 0;
  }

  // Draw one frame of the animation into the LED array
  for (byte x = 0; x < kMatrixWidth; x++) {
    for (int y = 0; y < kMatrixHeight; y++) {

      // Calculate "sine" waves with varying periods
      // sin8 is used for speed; cos8, quadwave8, or triwave8 would also work here
      byte sinDistanceR = qmul8(abs(y * (255 / kMatrixHeight) - sin8(sineOffset * 9 + x * 16)), 2);
      byte sinDistanceG = qmul8(abs(y * (255 / kMatrixHeight) - sin8(sineOffset * 10 + x * 16)), 2);
      byte sinDistanceB = qmul8(abs(y * (255 / kMatrixHeight) - sin8(sineOffset * 11 + x * 16)), 2);

      leds[XY(x, y)] = CRGB(255 - sinDistanceR, 255 - sinDistanceG, 255 - sinDistanceB);
    }
  }

  sineOffset++; // byte will wrap from 255 to 0, matching sin8 0-255 cycle

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

  // Draw one frame of the animation into the LED array
  for (byte x = 0; x < kMatrixWidth; x++) {
    int brightness = abs(x * (256 / kMatrixWidth) - triwave8(riderPos) * 2 + 127) * 3;
    if (brightness > 255) brightness = 255;
    brightness = 255 - brightness;
    CRGB riderColor = CHSV(cycleHue, 255, brightness);
    for (byte y = 0; y < kMatrixHeight; y++) {
      leds[XY(x, y)] = riderColor;
    }
  }

  riderPos++; // byte wraps to 0 at 255, triwave8 is also 0-255 periodic

}


// Shimmering noise, uses global hue cycle
void glitter() {

  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 15;
    fadeActive = 0;
  }

  // Draw one frame of the animation into the LED array
  for (int x = 0; x < kMatrixWidth; x++) {
    for (int y = 0; y < kMatrixHeight; y++) {
      leds[XY(x, y)] = CHSV(cycleHue, 255, random8(5) * 63);
    }
  }
}


// Fills saturated colors into the array from alternating directions
void colorFill() {

  static byte currentColor = 0;
  static byte currentRow = 0;
  static byte currentDirection = 0;

  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 45;
    currentColor = 0;
    currentRow = 0;
    currentDirection = 0;
    currentPalette = RainbowColors_p;
    fadeActive = 0;
  }

  // test a bitmask to fill up or down when currentDirection is 0 or 2 (0b00 or 0b10)
  if (!(currentDirection & 1)) {
    effectDelay = 45; // slower since vertical has fewer pixels
    for (byte x = 0; x < kMatrixWidth; x++) {
      byte y = currentRow;
      if (currentDirection == 2) y = kMatrixHeight - 1 - currentRow;
      leds[XY(x, y)] = currentPalette[currentColor];
    }
  }

  // test a bitmask to fill left or right when currentDirection is 1 or 3 (0b01 or 0b11)
  if (currentDirection & 1) {
    effectDelay = 20; // faster since horizontal has more pixels
    for (byte y = 0; y < kMatrixHeight; y++) {
      byte x = currentRow;
      if (currentDirection == 3) x = kMatrixWidth - 1 - currentRow;
      leds[XY(x, y)] = currentPalette[currentColor];
    }
  }

  currentRow++;

  // detect when a fill is complete, change color and direction
  if ((!(currentDirection & 1) && currentRow >= kMatrixHeight) || ((currentDirection & 1) && currentRow >= kMatrixWidth)) {
    currentRow = 0;
    currentColor += random8(3, 6);
    if (currentColor > 15) currentColor -= 16;
    currentDirection++;
    if (currentDirection > 3) currentDirection = 0;
    effectDelay = 300; // wait a little bit longer after completing a fill
  }


}

// Random pixels scroll sideways, uses current hue
#define rainDir 0
void sideRain() {

  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 30;
    fadeActive = 0;
  }

  scrollArray(rainDir);
  byte randPixel = random8(kMatrixHeight);
  for (byte y = 0; y < kMatrixHeight; y++) leds[XY((kMatrixWidth - 1) * rainDir, y)] = CRGB::Black;
  leds[XY((kMatrixWidth - 1)*rainDir, randPixel)] = CHSV(cycleHue, 255, 255);
}

// Pixels with random locations and random colors selected from a palette
// Use with the fadeAll function to allow old pixels to decay
void confetti() {

  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 10;
    selectRandomPalette();
    fadeActive = 1;
  }

  // scatter random colored pixels at several random coordinates
  for (byte i = 0; i < 4; i++) {
    leds[XY(random16(kMatrixWidth), random16(kMatrixHeight))] = ColorFromPalette(currentPalette, random16(255), 255); //CHSV(random16(255), 255, 255);
    random16_add_entropy(1);
  }

}


// Draw slanting bars scrolling across the array, uses current hue
void slantBars() {

  static byte slantPos = 0;

  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 5;
    fadeActive = 0;
  }

  for (byte x = 0; x < kMatrixWidth; x++) {
    for (byte y = 0; y < kMatrixHeight; y++) {
      leds[XY(x, y)] = CHSV(cycleHue, 255, sin8(x * 32 + y * 32 + slantPos));
    }
  }

  slantPos -= 4;
}

#define analyzerFadeFactor 5
#define analyzerScaleFactor 1.5
#define analyzerPaletteFactor 2
void drawAnalyzer() {
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
    int freqVal = spectrumDecay[x];
    
    for (byte y = 0; y < kMatrixHeight; y++) {
      int senseValue = freqVal / analyzerScaleFactor - yScale * (kMatrixHeight - 1 - y);
      int pixelBrightness = constrain(senseValue * analyzerFadeFactor, 0, 255);
      int pixelPaletteIndex = constrain(senseValue / analyzerPaletteFactor - 15, 0, 240);

      pixelColor = ColorFromPalette(currentPalette, pixelPaletteIndex, pixelBrightness);

      leds[XY(x, y)] = pixelColor;
      leds[XY(kMatrixWidth - x - 1, y)] = pixelColor;
    }
  }
}

#define VUFadeFactor 5
#define VUScaleFactor 2.0
#define VUPaletteFactor 1.5
void drawVU() {
  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 10;
    selectRandomAudioPalette();
    fadeActive = 0;
  }

  CRGB pixelColor;

  const float xScale = 255.0 / (kMatrixWidth / 2);
  float specCombo = (spectrumDecay[0] + spectrumDecay[1] + spectrumDecay[2] + spectrumDecay[3]) / 4.0;

  for (byte x = 0; x < kMatrixWidth / 2; x++) {
    int senseValue = specCombo / VUScaleFactor - xScale * x;
    int pixelBrightness = constrain(senseValue * VUFadeFactor, 0, 255);
    int pixelPaletteIndex = constrain(senseValue / VUPaletteFactor - 15, 0, 240);

    pixelColor = ColorFromPalette(currentPalette, pixelPaletteIndex, pixelBrightness);

    for (byte y = 0; y < kMatrixHeight; y++) {
      leds[XY(x, y)] = pixelColor;
      leds[XY(kMatrixWidth - x - 1, y)] = pixelColor;
    }
  }
}

void audioCirc() {
  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 10;
    fadeActive = 0;
  }

  int lowfreq; int medfreq; int hifreq;

  for (byte x = 0; x < kMatrixWidth; x++) {
    for (int y = 0; y < kMatrixHeight; y++) {
      lowfreq = spectrumValue[0]/(hypot(x-7.5, y-2)*1.5);
      medfreq = spectrumDecay[2]/(hypot(x-7.5, y-2)*1.1);
      hifreq = spectrumDecay[5]/(hypot(x-7.5, y-2)*1.2);

      if (lowfreq < 90) lowfreq = 0;
      if (lowfreq > 255) lowfreq = 255;

      if (medfreq < 60) medfreq = 0;
      if (medfreq > 255) medfreq = 255;

      if (hifreq < 60) hifreq = 0;
      if (hifreq > 255) hifreq = 255;
      
      leds[XY(x,y)] = CRGB(lowfreq, medfreq, hifreq);
    }
  }
}

void audioStripes() {
  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 25;
    selectRandomAudioPalette();
    fadeActive = 0;
  }

  CRGB linecolor;
  int audioLevel;
  int brightLevel;

  for (byte y = 0; y < 5; y++) {

    //audioLevel = spectrumDecay[y+1] / 2.0;
    audioLevel = spectrumPeaks[y+1] / 1.8;
    if (y == 0) audioLevel /= 2;
    if (audioLevel > 239) audioLevel = 239;

    for (byte x = 0; x < kMatrixWidth; x++) {
      brightLevel = (audioLevel-(abs(7.5-x)*20)) * 3.0;
      if (brightLevel < 0) brightLevel = 0;
      if (brightLevel > 254) brightLevel = 254;
      linecolor = ColorFromPalette(currentPalette, audioLevel, brightLevel);
      leds[XY(x, 4-y)] = linecolor;
    }
  }
}


//leds run around the periphery of the shades
void audioShadesOutline() {
  
  static float x = 0;
  
  //startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 15;
    FastLED.clear();
    currentPalette = RainbowColors_p;
    fadeActive = 10;
  }

  static uint8_t beatcount = 0;

  int brightness = std::min(static_cast<int>(spectrumDecay[0] + spectrumDecay[1]), 255);

  CRGB pixelColor = CHSV(cycleHue, 255, brightness);
  
  for (byte k = 0; k < 4; k++) {
    leds[OutlineMap(x+(OUTLINESIZE/4-1)*k)] += pixelColor;
  }

  x += constrain((spectrumDecay[0] + spectrumDecay[1]) / 600.0, 0.1, 1.0);
  
  if (x > (OUTLINESIZE-1)) x = 0;
  if (x < 0) x = OUTLINESIZE - 1;
}

// Ring pulser

void drawRing(int xCenter, int yCenter, float radius, CRGB color) {
  int brightness;
  CRGB tempColor;

  if (radius > 13) radius = 13;
  
  for (int x = 0; x < kMatrixWidth; x++) {
    for (int y = 0; y < kMatrixHeight; y++) {
      brightness = 255 - abs((hypot((float)x - 7.5 - xCenter/256.0, (float)y - 2.0 - yCenter/256.0) - radius)*192.0);
      if (brightness > 255) brightness = 255;
      if (brightness < 0) brightness = 0;
      tempColor = color;
      leds[XY(x, y)] += tempColor.nscale8(brightness);
    }
  }
}

void rings() {

  static float offset  = 0; // counter for radial color wave motion
  static uint16_t plasVector = 0; // counter for orbiting plasma center


  #define RING_GAINSMOOTH 0.01
  static float ringavg1 = 500, ringavg2 = 500, ringavg3 = 500;
  float ringval1, ringval2, ringval3;

  // startup tasks
  if (effectInit == false) {
    effectInit = true;
    effectDelay = 10;
    fadeActive = 0;
  }


  // Calculate current center of plasma pattern (can be offscreen)
  int xOffset;
  int yOffset;

  fillAll(0);



  ringval1 = spectrumDecay[0] + spectrumDecay[1] + spectrumDecay[2];
  ringval2 = spectrumDecay[3] + spectrumDecay[4];
  ringval3 = spectrumDecay[5] + spectrumDecay[6];

  ringavg1 = ringavg1 * (1.0 - RING_GAINSMOOTH) + ringval1 * RING_GAINSMOOTH;
  ringavg2 = ringavg2 * (1.0 - RING_GAINSMOOTH) + ringval2 * RING_GAINSMOOTH;
  ringavg3 = ringavg3 * (1.0 - RING_GAINSMOOTH) + ringval3 * RING_GAINSMOOTH;

  float ring_gain1 = (6.0/ringavg1);
  float ring_gain2 = (6.0/ringavg2);
  float ring_gain3 = (5.0/ringavg3);

  if (ring_gain1 > 0.05) ring_gain1 = 0.05;
  if (ring_gain2 > 0.05) ring_gain2 = 0.05;
  if (ring_gain3 > 0.05) ring_gain3 = 0.05;

  xOffset = cos16(plasVector);
  yOffset = sin16(plasVector);
  drawRing(xOffset/24,yOffset/24, ringval1 * ring_gain1, CRGB::Red);

  xOffset = cos16(plasVector+65535*0.33);
  yOffset = sin16(plasVector+65535*0.33);
    drawRing(xOffset/24,yOffset/24, ringval2 * ring_gain2, CRGB::Green);


  xOffset = cos16(plasVector+65535*0.66);
  yOffset = sin16(plasVector+65535*0.66);
  drawRing(xOffset/24,yOffset/24, ringval3 * ring_gain2, CRGB::Blue);


  offset += 0.2;
  plasVector += 256; // using an int for slower orbit (wraps at 65536)

}
