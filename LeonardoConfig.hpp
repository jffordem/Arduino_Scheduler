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

#ifndef LEONARDOCONFIG_HPP
#define LEONARDOCONFIG_HPP

#include <Led.hpp>
#include <EncoderWheel.hpp>
#include <ButtonHandler.hpp>

/*
Note: the PWM pins on the Leonardo are marked with a dash.
They are near the reset button.
Pins: 3, 5, 6, 9, 10, 11, 13.
*/

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

#endif
