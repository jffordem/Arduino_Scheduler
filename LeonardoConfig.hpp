/*
MIT License

Copyright (c) 2022-2025 jffordem

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <Led.hpp>
#include <EncoderWheel.hpp>
#include <ButtonHandler.hpp>

/*
Arduino Leonardo (ATmega32u4) notes:
  PWM pins: 3, 5, 6, 9, 10, 11, 13.
  INT-capable pins: 0, 1, 2, 3, 7.

  Encoder B: clock=7  -> INT-capable  -> InterruptEncoderControl
  Encoder A: clock=10 -> NOT INT-capable -> EncoderControl (polling fallback)
  To add interrupt support for Encoder A, rewire its clock to pin 0, 1, 2, 3, or 7.
*/

// Board-selected encoder class aliases. Use these in sketches instead of naming
// the class directly so the right implementation is chosen at compile time.
#if defined(__AVR_ATmega32U4__)
  template <class T> using EncoderBControl = InterruptEncoderControl<T>;
  template <class T> using EncoderAControl = EncoderControl<T>;  // pin 10 not INT-capable
#else
  template <class T> using EncoderBControl = InterruptEncoderControl<T>;
  template <class T> using EncoderAControl = InterruptEncoderControl<T>;
#endif

struct {
  const LedConfig DefaultLed = { .pin = LED_BUILTIN, .lowIsOn = false };
  struct {
    const ButtonConfig Button = { .pin = 5, .lowIsPressed = true };
    const EncoderConfig Encoder = { .clockPin = 7, .dataPin = 6 };
  } B;
  struct {
    const ButtonConfig Button = { .pin = 8, .lowIsPressed = true };
    const EncoderConfig Encoder = { .clockPin = 10, .dataPin = 9 };
  } A;
} Config;
