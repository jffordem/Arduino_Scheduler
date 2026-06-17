#include "MorseOutput.h"
#include <Arduino.h>

#ifdef MORSE_ACTIVE

#include <Adafruit_NeoPixel.h>
static Adafruit_NeoPixel _px(1, 48, NEO_GRB + NEO_KHZ800);

void LedOutput::begin() {
    _px.begin();
    _px.clear();
    _px.show();
}

void LedOutput::mark() {
    _px.setPixelColor(0, _px.Color(30, 30, 30));
    _px.show();
}

void LedOutput::space() {
    _px.clear();
    _px.show();
}

#else

void LedOutput::begin()  { }
void LedOutput::mark()   { Serial.println("[MORSE] mark"); }
void LedOutput::space()  { Serial.println("[MORSE] space"); }

#endif
